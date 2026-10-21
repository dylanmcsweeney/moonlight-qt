#include "uistartuprecovery.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QSysInfo>
#include <QStringList>
#include <QDateTime>
#include <QDebug>

struct UiStartupRecovery::State
{
    struct Attempt {
        QString id;
        int backend = UiStartupRecovery::Automatic;
        QString stage;
        QString startedAt;
    };

    bool recoveryActive = false;
    int failedBackend = UiStartupRecovery::Automatic;
    QString failedAttempt;
    QList<Attempt> attempts;
};

namespace {
constexpr int JournalVersion = 1;
const int ValidBackendMin = UiStartupRecovery::Automatic;
const int ValidBackendMax = UiStartupRecovery::OpenGL;

bool validBackend(int backend)
{
    return backend >= ValidBackendMin && backend <= ValidBackendMax;
}

QString attemptLockPath(const QString& stateDirectory, const QString& id)
{
    return QDir(stateDirectory).absoluteFilePath(
        QStringLiteral("attempt-%1.lock").arg(id));
}

QString cleanMarkerPath(const QString& stateDirectory, const QString& id)
{
    return QDir(stateDirectory).absoluteFilePath(
        QStringLiteral("attempt-%1.clean").arg(id));
}

QJsonObject toJson(const UiStartupRecovery::State::Attempt& attempt)
{
    return QJsonObject{
        {QStringLiteral("id"), attempt.id},
        {QStringLiteral("backend"), attempt.backend},
        {QStringLiteral("stage"), attempt.stage},
        {QStringLiteral("startedAt"), attempt.startedAt}
    };
}

bool sameHost(const QString& host)
{
    return host.compare(QSysInfo::machineHostName(), Qt::CaseInsensitive) == 0;
}

bool acquireJournalLock(QLockFile& lock, const QString& path, int waitMs)
{
    // Do not let QLockFile's age rule break a lock owned by another machine.
    if (QFileInfo::exists(path)) {
        QLockFile probe(path);
        qint64 pid = 0;
        QString hostname;
        QString appname;
        if (!probe.getLockInfo(&pid, &hostname, &appname) || pid <= 0 ||
                !sameHost(hostname)) {
            return false;
        }
        lock.setStaleLockTime(0);
    }
    else {
        // A concurrently created lock is too new for the default age rule.
        lock.setStaleLockTime(30000);
    }
    return lock.tryLock(waitMs);
}

// Return true only when the attempt is known to be abandoned. Foreign-host,
// malformed, and otherwise uncertain lock records remain pending.
bool attemptIsAbandoned(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        return true;
    }

    QLockFile probe(path);
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (!probe.getLockInfo(&pid, &hostname, &appname) || pid <= 0 ||
            hostname.isEmpty() || !sameHost(hostname)) {
        return false;
    }

    // A zero stale timeout asks Qt to use its same-host process liveness probe
    // immediately. QLockFile does not infer failure from file age.
    probe.setStaleLockTime(0);
    if (!probe.tryLock(0)) {
        return false;
    }
    probe.unlock();
    return true;
}
}

UiStartupRecovery::UiStartupRecovery(const QString& settingsDirectory,
                                     int requestedBackend,
                                     bool graphical,
                                     bool safeUi,
                                     int explicitEnvironmentBackend,
                                     bool explicitRetry,
                                     QObject* parent)
    : QObject(parent),
      m_SettingsDirectory(QDir(settingsDirectory).absolutePath()),
      m_StateDirectory(QDir(m_SettingsDirectory).absoluteFilePath(
          QStringLiteral("ui-startup-recovery"))),
      m_StatePath(QDir(m_StateDirectory).absoluteFilePath(
          QStringLiteral("state.json"))),
      m_GlobalLockPath(QDir(m_StateDirectory).absoluteFilePath(
          QStringLiteral("journal.lock"))),
      m_RequestedBackend(validBackend(requestedBackend) ? requestedBackend : Automatic),
      m_EnvironmentBackend(validBackend(explicitEnvironmentBackend)
                           ? explicitEnvironmentBackend : -1),
      m_EffectiveBackend(m_RequestedBackend),
      m_AttemptBackend(m_RequestedBackend),
      m_Graphical(graphical),
      m_SafeUiActive(graphical && safeUi),
      m_ExplicitRetry(graphical && explicitRetry && !safeUi),
      m_RecoveryActive(false),
      m_Initialized(false),
      m_AttemptFinished(true)
{
    if (m_SafeUiActive) {
        m_EffectiveBackend = Direct3D11;
        m_StatusMessage = tr("Safe UI mode is using Direct3D 11 for this launch.");
    }
    else if (m_EnvironmentBackend >= 0) {
        m_EffectiveBackend = m_EnvironmentBackend;
    }
    m_AttemptBackend = m_EffectiveBackend;

    if (!graphical) {
        return;
    }

    m_Elapsed.start();
    initializeJournal(graphical);
}

UiStartupRecovery::~UiStartupRecovery()
{
    finishCleanly();
}

int UiStartupRecovery::effectiveBackend() const { return m_EffectiveBackend; }
bool UiStartupRecovery::recoveryActive() const { return m_RecoveryActive; }
bool UiStartupRecovery::safeUiActive() const { return m_SafeUiActive; }
QString UiStartupRecovery::warning() const { return m_Warning; }
QString UiStartupRecovery::statusMessage() const { return m_StatusMessage; }
QString UiStartupRecovery::attemptId() const { return m_AttemptId; }

bool UiStartupRecovery::loadState(State& state, QString* error) const
{
    QFile file(m_StatePath);
    if (!file.exists()) {
        state = State{};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = tr("Could not read the UI startup recovery journal: %1")
                                .arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = tr("The UI startup recovery journal is malformed.");
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != JournalVersion ||
            !root.value(QStringLiteral("attempts")).isArray()) {
        if (error) *error = tr("The UI startup recovery journal has an unsupported format.");
        return false;
    }

    state.recoveryActive = root.value(QStringLiteral("recoveryActive")).toBool(false);
    state.failedBackend = root.value(QStringLiteral("failedBackend"))
                              .toInt(Automatic);
    state.failedAttempt = root.value(QStringLiteral("failedAttempt")).toString();
    if (!validBackend(state.failedBackend)) {
        if (error) *error = tr("The UI startup recovery journal contains an invalid backend.");
        return false;
    }

    const QJsonArray attempts = root.value(QStringLiteral("attempts")).toArray();
    QSet<QString> attemptIds;
    for (const QJsonValue& value : attempts) {
        if (!value.isObject()) {
            if (error) *error = tr("The UI startup recovery journal contains an invalid attempt.");
            return false;
        }
        const QJsonObject object = value.toObject();
        State::Attempt attempt;
        attempt.id = object.value(QStringLiteral("id")).toString();
        attempt.backend = object.value(QStringLiteral("backend")).toInt(-1);
        attempt.stage = object.value(QStringLiteral("stage")).toString();
        attempt.startedAt = object.value(QStringLiteral("startedAt")).toString();
        const QUuid parsedId(attempt.id);
        const QString canonicalId = parsedId.toString(QUuid::WithoutBraces);
        if (attempt.id.isEmpty() || parsedId.isNull() ||
                canonicalId.compare(attempt.id, Qt::CaseInsensitive) != 0 ||
                attemptIds.contains(canonicalId) || !validBackend(attempt.backend)) {
            if (error) *error = tr("The UI startup recovery journal contains an invalid attempt.");
            return false;
        }
        attempt.id = canonicalId;
        attemptIds.insert(canonicalId);
        state.attempts.append(attempt);
    }
    return true;
}

bool UiStartupRecovery::saveState(const State& state)
{
    QJsonArray attempts;
    for (const State::Attempt& attempt : state.attempts) {
        attempts.append(toJson(attempt));
    }
    const QJsonObject root{
        {QStringLiteral("version"), JournalVersion},
        {QStringLiteral("recoveryActive"), state.recoveryActive},
        {QStringLiteral("failedBackend"), state.failedBackend},
        {QStringLiteral("failedAttempt"), state.failedAttempt},
        {QStringLiteral("attempts"), attempts}
    };

    QSaveFile file(m_StatePath);
    if (!file.open(QIODevice::WriteOnly)) {
        failSafe(tr("Could not write the UI startup recovery journal: %1")
                 .arg(file.errorString()));
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0 ||
            !file.commit()) {
        failSafe(tr("Could not commit the UI startup recovery journal: %1")
                 .arg(file.errorString()));
        return false;
    }
    return true;
}

bool UiStartupRecovery::initializeJournal(bool graphical)
{
    if (!graphical) return true;
    QDir directory;
    if (!directory.mkpath(m_StateDirectory)) {
        failSafe(tr("Could not create the UI startup recovery directory."));
        return false;
    }

    QLockFile journalLock(m_GlobalLockPath);
    if (!acquireJournalLock(journalLock, m_GlobalLockPath, 2500)) {
        failSafe(tr("Could not lock the UI startup recovery journal; using a safe renderer for this launch."));
        return false;
    }

    State state;
    QString readError;
    if (!loadState(state, &readError)) {
        if (m_SafeUiActive) {
            // Safe UI is a one-launch escape hatch. Leave unreadable metadata
            // exactly as found and run this safe attempt without journaling.
            failSafe(readError);
            return false;
        }
        // Preserve the damaged file for diagnosis, then install a fail-safe
        // latch. This metadata is separate from user preferences.
        const QString archived = m_StatePath + QStringLiteral(".corrupt-") +
                                 QUuid::createUuid().toString(QUuid::WithoutBraces);
        QFile::rename(m_StatePath, archived);
        state = State{};
        state.recoveryActive = true;
        state.failedBackend = Direct3D12;
        state.failedAttempt = QStringLiteral("corrupt-journal");
        m_Warning = readError;
    }

    QList<State::Attempt> remaining;
    QStringList cleanMarkersToRemove;
    bool abandonedD3D12 = false;
    QString abandonedId;
    for (const State::Attempt& attempt : state.attempts) {
        const QString markerPath = cleanMarkerPath(m_StateDirectory, attempt.id);
        if (QFileInfo::exists(markerPath)) {
            cleanMarkersToRemove.append(markerPath);
            continue;
        }
        if (m_SafeUiActive) {
            // Keep earlier failure evidence intact for the next normal launch.
            remaining.append(attempt);
            continue;
        }
        if (attemptIsAbandoned(attemptLockPath(m_StateDirectory, attempt.id))) {
            qWarning().noquote() << QStringLiteral("UI startup attempt %1 abandoned at stage %2 after %3 ms")
                                    .arg(attempt.id, attempt.stage)
                                    .arg(QDateTime::fromString(attempt.startedAt, Qt::ISODateWithMs)
                                         .msecsTo(QDateTime::currentDateTimeUtc()));
            if (attempt.backend == Direct3D12) {
                abandonedD3D12 = true;
                abandonedId = attempt.id;
            }
        }
        else {
            remaining.append(attempt);
        }
    }
    state.attempts = remaining;
    if (abandonedD3D12) {
        state.recoveryActive = true;
        state.failedBackend = Direct3D12;
        state.failedAttempt = abandonedId;
    }

    // A persisted Direct3D 11 preference is already an explicit user choice.
    if (!m_SafeUiActive && m_RequestedBackend == Direct3D11 &&
            (m_EnvironmentBackend < 0 ||
             m_EnvironmentBackend == Direct3D11)) {
        state.recoveryActive = false;
        state.failedBackend = Automatic;
        state.failedAttempt.clear();
    }

    m_RecoveryActive = state.recoveryActive;
    if (m_ExplicitRetry) {
        state.recoveryActive = false;
        state.failedBackend = Automatic;
        state.failedAttempt.clear();
    }

    m_RecoveryActive = state.recoveryActive;
    if (!m_SafeUiActive && m_RecoveryActive) {
        m_EffectiveBackend = Direct3D11;
        m_StatusMessage = tr("UI startup recovery is using Direct3D 11. The saved graphics preference is unchanged.");
    }
    else if (!m_SafeUiActive && m_EnvironmentBackend < 0) {
        m_EffectiveBackend = m_RequestedBackend;
    }

    if (m_ExplicitRetry) {
        // Explicit retry is a one-time child-process instruction. Clear the
        // old latch before registering this process's D3D12 attempt.
        state.recoveryActive = false;
        state.failedBackend = Automatic;
        state.failedAttempt.clear();
        m_EffectiveBackend = m_EnvironmentBackend >= 0
            ? m_EnvironmentBackend : m_RequestedBackend;
        m_AttemptBackend = m_EffectiveBackend;
        m_RecoveryActive = false;
        m_StatusMessage = tr("Retrying the saved UI graphics preference.");
    }

    m_AttemptId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_AttemptLock = std::make_unique<QLockFile>(
        attemptLockPath(m_StateDirectory, m_AttemptId));
    m_AttemptLock->setStaleLockTime(0);
    if (!m_AttemptLock->tryLock(0)) {
        failSafe(tr("Could not register the UI startup attempt; using a safe renderer for this launch."));
        return false;
    }
    State::Attempt attempt;
    attempt.id = m_AttemptId;
    m_AttemptBackend = m_EffectiveBackend;
    attempt.backend = m_AttemptBackend;
    attempt.stage = QStringLiteral("begin");
    attempt.startedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    state.attempts.append(attempt);
    if (!saveState(state)) {
        m_AttemptLock->unlock();
        m_AttemptLock.reset();
        return false;
    }
    for (const QString& markerPath : cleanMarkersToRemove) {
        QFile::remove(markerPath);
    }

    m_AttemptFinished = false;
    m_Initialized = true;
    if (m_StatusMessage.isEmpty()) {
        m_StatusMessage = tr("UI startup attempt is being monitored.");
    }
    return true;
}

void UiStartupRecovery::failSafe(const QString& message)
{
    const int oldBackend = m_EffectiveBackend;
    const QString oldWarning = m_Warning;
    const QString oldStatus = m_StatusMessage;
    if (m_Warning.isEmpty()) m_Warning = message;
    if (m_Graphical && !m_SafeUiActive && !m_Initialized) {
#ifdef Q_OS_WIN
        if (m_RequestedBackend != OpenGL ||
                m_EnvironmentBackend == Direct3D12) {
            m_EffectiveBackend = Direct3D11;
            m_AttemptBackend = Direct3D11;
        }
#endif
    }
    if (m_StatusMessage.isEmpty()) {
        m_StatusMessage = tr("UI startup recovery could not save its state. A safe renderer will be used where available.");
    }
    if (oldBackend != m_EffectiveBackend || oldWarning != m_Warning ||
            oldStatus != m_StatusMessage) {
        emit stateChanged();
    }
    qWarning().noquote() << message;
}

void UiStartupRecovery::stage(const QString& name)
{
    if (!m_Graphical || m_AttemptFinished) return;
    qInfo().noquote() << QStringLiteral("UI startup attempt %1: %2 (%3 ms)")
                         .arg(m_AttemptId, name).arg(m_Elapsed.elapsed());
    updateCurrentAttempt(name);
}

void UiStartupRecovery::recordActualBackend(int backend)
{
    if (!validBackend(backend) || !m_Graphical || m_AttemptFinished) return;
    m_AttemptBackend = backend;
    qInfo().noquote() << QStringLiteral("UI startup attempt %1: renderer backend %2 (%3 ms)")
                         .arg(m_AttemptId).arg(backend).arg(m_Elapsed.elapsed());
    updateCurrentAttempt(QStringLiteral("renderer-identified"));
}

void UiStartupRecovery::updateCurrentAttempt(const QString& stageName)
{
    QLockFile journalLock(m_GlobalLockPath);
    if (!acquireJournalLock(journalLock, m_GlobalLockPath, 1000)) {
        failSafe(tr("Could not update the UI startup recovery journal."));
        return;
    }
    State state;
    if (!loadState(state)) {
        failSafe(tr("Could not read the UI startup recovery journal while updating the attempt."));
        return;
    }
    for (State::Attempt& attempt : state.attempts) {
        if (attempt.id == m_AttemptId) {
            attempt.stage = stageName;
            attempt.backend = m_AttemptBackend;
            break;
        }
    }
    saveState(state);
}

void UiStartupRecovery::markHealthy()
{
    if (!m_Graphical || m_AttemptFinished) return;
    stage(QStringLiteral("healthy"));
    finishCleanly();
    m_StatusMessage = m_RecoveryActive
        ? tr("The UI is healthy; the Direct3D 11 recovery override remains active until explicitly cleared.")
        : tr("The UI startup completed successfully.");
    emit stateChanged();
}

void UiStartupRecovery::finishCleanly()
{
    if (!m_Graphical || m_AttemptFinished) return;
    const QString markerPath = cleanMarkerPath(m_StateDirectory, m_AttemptId);
    QSaveFile marker(markerPath);
    const bool markerSaved = marker.open(QIODevice::WriteOnly) && marker.commit();
    if (!markerSaved) {
        failSafe(tr("Could not record a clean UI shutdown."));
    }

    bool journalCleaned = false;
    QLockFile journalLock(m_GlobalLockPath);
    if (acquireJournalLock(journalLock, m_GlobalLockPath, 1000)) {
        State state;
        if (loadState(state)) {
            for (int i = state.attempts.size() - 1; i >= 0; --i) {
                if (state.attempts.at(i).id == m_AttemptId) {
                    state.attempts.removeAt(i);
                }
            }
            journalCleaned = saveState(state);
        }
        else {
            failSafe(tr("Could not read the UI startup recovery journal during clean shutdown."));
        }
    }
    else {
        failSafe(tr("Could not lock the UI startup recovery journal during clean shutdown."));
    }
    if (m_AttemptLock) {
        m_AttemptLock->unlock();
        m_AttemptLock.reset();
    }
    if (markerSaved && journalCleaned) {
        QFile::remove(markerPath);
    }
    m_AttemptFinished = true;
    m_Initialized = false;
}

bool UiStartupRecovery::retryPreferredBackend()
{
    if (!m_Graphical) return false;
    QLockFile journalLock(m_GlobalLockPath);
    if (!acquireJournalLock(journalLock, m_GlobalLockPath, 1000)) {
        failSafe(tr("Could not clear the UI startup recovery latch for a retry."));
        return false;
    }
    State state;
    if (!loadState(state)) {
        failSafe(tr("Could not read the UI startup recovery latch for a retry."));
        return false;
    }
    state.recoveryActive = false;
    state.failedBackend = Automatic;
    state.failedAttempt.clear();
    const int retryBackend = m_EnvironmentBackend >= 0
        ? m_EnvironmentBackend : m_RequestedBackend;
    for (State::Attempt& attempt : state.attempts) {
        if (attempt.id == m_AttemptId) {
            attempt.backend = retryBackend;
            attempt.stage = QStringLiteral("retry-requested");
        }
    }
    if (!saveState(state)) {
        return false;
    }
    m_RecoveryActive = false;
    m_EffectiveBackend = retryBackend;
    m_AttemptBackend = retryBackend;
    m_StatusMessage = tr("Retrying the saved UI graphics preference.");
    emit stateChanged();
    return true;
}

bool UiStartupRecovery::clearRecoveryForSavedBackend(int backend)
{
    if (backend != Direct3D11 || !m_Graphical) return false;
    QLockFile journalLock(m_GlobalLockPath);
    if (!acquireJournalLock(journalLock, m_GlobalLockPath, 1000)) {
        failSafe(tr("Could not clear the UI startup recovery latch after saving Direct3D 11."));
        return false;
    }
    State state;
    if (!loadState(state)) {
        failSafe(tr("Could not read the UI startup recovery latch after saving Direct3D 11."));
        return false;
    }
    state.recoveryActive = false;
    state.failedBackend = Automatic;
    state.failedAttempt.clear();
    if (!saveState(state)) return false;
    m_RecoveryActive = false;
    m_StatusMessage = tr("Direct3D 11 was saved as the UI graphics preference.");
    emit stateChanged();
    return true;
}

bool UiStartupRecovery::relatchRecovery()
{
    if (!m_Graphical) return false;
    QLockFile journalLock(m_GlobalLockPath);
    if (!acquireJournalLock(journalLock, m_GlobalLockPath, 1000)) {
        failSafe(tr("Could not restore the UI startup recovery latch."));
        return false;
    }
    State state;
    if (!loadState(state)) {
        failSafe(tr("Could not read the UI startup recovery latch."));
        return false;
    }
    state.recoveryActive = true;
    state.failedBackend = Direct3D12;
    if (state.failedAttempt.isEmpty()) state.failedAttempt = QStringLiteral("retry-not-started");
    if (!saveState(state)) return false;
    m_RecoveryActive = true;
    m_EffectiveBackend = Direct3D11;
    m_StatusMessage = tr("UI startup recovery is using Direct3D 11.");
    emit stateChanged();
    return true;
}
