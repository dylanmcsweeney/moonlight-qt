#include "streamprofilemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QSettings>
#include <QDateTime>
#include <QSaveFile>
#include <QUuid>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <signal.h>
#endif

namespace {

const char* const ROOT_GROUP = "streamProfiles";
const char* const TEMPLATE_GROUP = "template";
const char* const HOSTS_GROUP = "hosts";
const char* const PROFILE_ORDER = "profileOrder";
const char* const ACTIVE_PROFILE = "activeProfile";
const char* const NEXT_PROFILE_NUMBER = "nextProfileNumber";
const char* const PROFILE_NAME = "name";
const char* const COORD_GROUP = "settingsCoordination";
const char* const COORD_REVISION = "revision";
const char* const COORD_INSTANCE = "changedBy";
const char* const COORD_INSTANCE_NUMBER = "instanceNumber";
const char* const COORD_CHANGED_AT = "changedAt";
const QStringList APP_PREFERENCE_KEYS = {
    QStringLiteral("mdns"), QStringLiteral("detectnetblocking"),
    QStringLiteral("richpresence"), QStringLiteral("uidisplaymode"),
    QStringLiteral("uiframepacingmode"), QStringLiteral("uigraphicsbackend"),
    QStringLiteral("uid3d11swapchainmode"), QStringLiteral("language"),
    QStringLiteral("uiswapfacebuttons"), QStringLiteral("showpcprofilecontrols")
};

qulonglong readRevision(QSettings& settings)
{
    return settings.value(QStringLiteral("settingsCoordination/revision"), 0).toULongLong();
}

QStringList profileHostUuids(QSettings& settings)
{
    const QString prefix = QStringLiteral("streamProfiles/hosts/");
    QStringList result;
    for (const QString& key : settings.allKeys()) {
        if (!key.startsWith(prefix) || !key.endsWith(QStringLiteral("/profileOrder"))) {
            continue;
        }
        const QString encoded = key.mid(prefix.size(), key.size() - prefix.size() -
                                         QStringLiteral("/profileOrder").size());
        const QString uuid = QString::fromUtf8(QByteArray::fromHex(encoded.toLatin1()));
        if (!uuid.isEmpty() && !result.contains(uuid)) result.append(uuid);
    }
    return result;
}

void copyManagedSettings(QSettings& source, QSettings& destination)
{
    const QString profilePrefix = QStringLiteral("streamProfiles/");
    QStringList destinationProfileKeys;
    for (const QString& key : destination.allKeys()) {
        if (key.startsWith(profilePrefix)) {
            destinationProfileKeys.append(key);
        }
    }
    for (const QString& key : std::as_const(destinationProfileKeys)) {
        destination.remove(key);
    }
    for (const QString& key : source.allKeys()) {
        if (key.startsWith(profilePrefix)) {
            destination.setValue(key, source.value(key));
        }
    }
    for (const QString& key : APP_PREFERENCE_KEYS) {
        if (source.contains(key)) {
            destination.setValue(key, source.value(key));
        }
        else {
            destination.remove(key);
        }
    }
}

bool isProcessRunning(qint64 pid)
{
    if (pid <= 0) return false;
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
    const DWORD waitResult = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return waitResult == WAIT_TIMEOUT;
#else
    if (::kill(pid_t(pid), 0) == 0) return true;
    return errno == EPERM;
#endif
}

}

StreamProfileManager* StreamProfileManager::get()
{
    static StreamProfileManager manager;
    QQmlEngine::setObjectOwnership(&manager, QQmlEngine::CppOwnership);
    return &manager;
}

StreamProfileManager::StreamProfileManager(QObject* parent)
    : QObject(parent)
{
    m_InstanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_SnapshotPath = m_SnapshotDirectory.filePath(QStringLiteral("settings.ini"));
    if (initializeSnapshot()) {
        registerInstance();
        initializeTemplate();
    }
    else {
        qCritical() << "Settings coordination could not create a safe shared-settings snapshot";
    }
}

StreamProfileManager::~StreamProfileManager()
{
    unregisterInstance();
}

QString StreamProfileManager::sharedStateDirectory() const
{
    QSettings shared;
    if (shared.format() == QSettings::IniFormat) {
        const QString filePath = shared.fileName();
        if (!filePath.isEmpty()) {
            const QString directory = QFileInfo(filePath).absolutePath();
            QDir().mkpath(directory);
            return directory;
        }
    }
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (path.isEmpty()) {
        path = QDir::homePath() + QStringLiteral("/.config/Moonlight");
    }
    QDir().mkpath(path);
    return path;
}

QString StreamProfileManager::lockFilePath() const
{
    return QDir(sharedStateDirectory()).filePath(QStringLiteral("moonlight-settings.lock"));
}

bool StreamProfileManager::initializeSnapshot()
{
    if (!m_SnapshotDirectory.isValid()) return false;
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) {
        qWarning() << "Unable to lock settings while creating instance snapshot";
        return false;
    }
    QSettings shared;
    shared.sync();
    if (shared.status() != QSettings::NoError) {
        qWarning() << "Unable to read shared settings for instance snapshot" << shared.status();
        return false;
    }
    QSettings local(m_SnapshotPath, QSettings::IniFormat);
    for (const QString& key : shared.allKeys()) {
        local.setValue(key, shared.value(key));
    }
    m_SnapshotRevision = readRevision(shared);
    m_AcknowledgedRevision = m_SnapshotRevision;
    m_LastObservedRevision = m_SnapshotRevision;
    local.setValue(QStringLiteral("settingsCoordination/revision"), m_SnapshotRevision);
    local.sync();
    if (local.status() != QSettings::NoError) return false;
    m_SnapshotReady = true;
    return true;
}

bool StreamProfileManager::loadSharedIntoSnapshot(qulonglong* revision)
{
    if (!m_SnapshotReady) return false;
    QSettings shared;
    shared.sync();
    if (shared.status() != QSettings::NoError) {
        return false;
    }
    QSettings local(m_SnapshotPath, QSettings::IniFormat);
    // The snapshot is a full copy so unmanaged settings remain available to
    // legacy readers, but only the managed groups are ever published.
    local.clear();
    for (const QString& key : shared.allKeys()) {
        local.setValue(key, shared.value(key));
    }
    const qulonglong currentRevision = readRevision(shared);
    local.sync();
    if (revision) *revision = currentRevision;
    return local.status() == QSettings::NoError;
}

void StreamProfileManager::registerInstance()
{
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) return;
    QDir dir(sharedStateDirectory());
    const QStringList files = dir.entryList(QStringList() << "instance-*.json", QDir::Files);
    QSet<int> used;
    for (const QString& file : files) {
        QFile record(dir.filePath(file));
        if (!record.open(QIODevice::ReadOnly)) continue;
        const QJsonObject item = QJsonDocument::fromJson(record.readAll()).object();
        const qint64 pid = item.value(QStringLiteral("pid")).toVariant().toLongLong();
        if (!isProcessRunning(pid)) {
            record.close();
            QFile::remove(dir.filePath(file));
            continue;
        }
        used.insert(item.value(QStringLiteral("number")).toInt());
    }
    while (used.contains(m_InstanceNumber)) ++m_InstanceNumber;
    writeInstanceStatus();
}

void StreamProfileManager::unregisterInstance()
{
    if (m_InstanceId.isEmpty()) return;
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) return;
    QFile::remove(QDir(sharedStateDirectory()).filePath(
                      QStringLiteral("instance-%1.json").arg(m_InstanceId)));
}

void StreamProfileManager::writeInstanceStatus()
{
    if (m_InstanceId.isEmpty()) return;
    QJsonObject item;
    item.insert(QStringLiteral("id"), m_InstanceId);
    item.insert(QStringLiteral("pid"), QCoreApplication::applicationPid());
    item.insert(QStringLiteral("number"), m_InstanceNumber);
    item.insert(QStringLiteral("snapshotSourceRevision"),
                static_cast<qint64>(m_SnapshotRevision));
    item.insert(QStringLiteral("acknowledgedRevision"),
                static_cast<qint64>(m_AcknowledgedRevision));
    item.insert(QStringLiteral("localDirty"), m_LocalSettingsDirty);
    item.insert(QStringLiteral("updatedAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QSaveFile record(QDir(sharedStateDirectory()).filePath(
                         QStringLiteral("instance-%1.json").arg(m_InstanceId)));
    if (record.open(QIODevice::WriteOnly)) {
        record.write(QJsonDocument(item).toJson(QJsonDocument::Compact));
        record.commit();
    }
}

void StreamProfileManager::updateInstanceStatus()
{
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(1000)) return;
    writeInstanceStatus();
}

bool StreamProfileManager::publishSnapshot(bool force)
{
    bool lockHeld = false;
    auto fail = [this, &lockHeld](const QString& message) {
        if (lockHeld) writeInstanceStatus();
        if (m_LastError != message) {
            m_LastError = message;
            emit lastErrorChanged();
        }
        return false;
    };
    if (!m_SnapshotReady) return fail(tr("The local settings snapshot is unavailable."));
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) return fail(tr("Could not lock shared settings."));
    lockHeld = true;
    QSettings shared;
    shared.sync();
    if (shared.status() != QSettings::NoError) return fail(tr("Could not read shared settings."));
    const qulonglong currentRevision = readRevision(shared);
    if (!force && currentRevision != m_AcknowledgedRevision) {
        return fail(tr("Another Moonlight instance changed settings. Check for external changes before saving."));
    }
    if (force && currentRevision != m_LastObservedRevision) {
        m_LastObservedRevision = currentRevision;
        return fail(tr("Shared settings changed again. Check for external changes before overriding."));
    }
    QSettings local(m_SnapshotPath, QSettings::IniFormat);
    local.sync();
    if (local.status() != QSettings::NoError) return fail(tr("Could not read the local settings snapshot."));
    copyManagedSettings(local, shared);
    const qulonglong nextRevision = currentRevision + 1;
    shared.setValue(QStringLiteral("settingsCoordination/revision"), nextRevision);
    shared.setValue(QStringLiteral("settingsCoordination/changedBy"), m_InstanceId);
    shared.setValue(QStringLiteral("settingsCoordination/instanceNumber"), m_InstanceNumber);
    shared.setValue(QStringLiteral("settingsCoordination/changedAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    shared.sync();
    if (shared.status() != QSettings::NoError) return fail(tr("Could not write shared settings."));
    local.setValue(QStringLiteral("settingsCoordination/revision"), nextRevision);
    local.sync();
    if (local.status() != QSettings::NoError) {
        m_LocalSettingsDirty = true;
        return fail(tr("Shared settings were written, but the local snapshot could not be updated."));
    }
    m_SnapshotRevision = m_AcknowledgedRevision = nextRevision;
    m_LastObservedRevision = nextRevision;
    const bool dirtyWas = m_LocalSettingsDirty;
    m_LocalSettingsDirty = false;
    writeInstanceStatus();
    const bool errorWas = !m_LastError.isEmpty();
    if (errorWas) {
        m_LastError.clear();
    }
    lock.unlock();
    lockHeld = false;
    if (dirtyWas) emit localSettingsDirtyChanged();
    if (errorWas) emit lastErrorChanged();
    emit sharedRevisionChanged();
    return true;
}

QVariantMap StreamProfileManager::checkForExternalChanges()
{
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    QVariantMap result;
    if (!m_SnapshotReady) {
        result.insert(QStringLiteral("error"), QStringLiteral("Settings snapshot is unavailable"));
        return result;
    }
    if (!lock.tryLock(5000)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Unable to lock shared settings"));
        return result;
    }
    QSettings shared;
    shared.sync();
    if (shared.status() != QSettings::NoError) {
        result.insert(QStringLiteral("error"), QStringLiteral("Unable to read shared settings"));
        return result;
    }
    const qulonglong revision = readRevision(shared);
    m_LastObservedRevision = revision;
    result.insert(QStringLiteral("revision"), QVariant::fromValue(revision));
    result.insert(QStringLiteral("changed"), revision != m_AcknowledgedRevision);
    result.insert(QStringLiteral("changedBy"), shared.value(QStringLiteral("settingsCoordination/changedBy")).toString());
    result.insert(QStringLiteral("changedAt"), shared.value(QStringLiteral("settingsCoordination/changedAt")).toString());
    result.insert(QStringLiteral("instanceNumber"), shared.value(QStringLiteral("settingsCoordination/instanceNumber")).toInt());
    result.insert(QStringLiteral("localSettingsDirty"), m_LocalSettingsDirty);
    return result;
}

bool StreamProfileManager::acknowledgeExternalChanges()
{
    if (!m_SnapshotReady) {
        setLastError(tr("The local settings snapshot is unavailable."));
        return false;
    }
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) {
        setLastError(tr("Could not lock shared settings."));
        return false;
    }
    QSettings shared;
    shared.sync();
    if (shared.status() != QSettings::NoError) {
        lock.unlock();
        setLastError(tr("Could not read shared settings."));
        return false;
    }
    const qulonglong currentRevision = readRevision(shared);
    if (currentRevision != m_LastObservedRevision) {
        lock.unlock();
        setLastError(tr("Shared settings changed again. Check for external changes before continuing."));
        return false;
    }
    m_AcknowledgedRevision = currentRevision;
    m_LastObservedRevision = m_AcknowledgedRevision;
    const bool dirtyChanged = !m_LocalSettingsDirty;
    if (dirtyChanged) {
        m_LocalSettingsDirty = true;
    }
    writeInstanceStatus();
    lock.unlock();
    if (dirtyChanged) emit localSettingsDirtyChanged();
    if (!m_LastError.isEmpty()) {
        m_LastError.clear();
        emit lastErrorChanged();
    }
    emit sharedRevisionChanged();
    return true;
}

QVariantMap StreamProfileManager::reloadSnapshot(const QString& hostUuid,
                                                 bool discardLocalChanges)
{
    QVariantMap result;
    if (m_LocalSettingsDirty && !discardLocalChanges) {
        result.insert(QStringLiteral("error"), tr("Save or discard local settings before reloading."));
        return result;
    }
    QString oldActiveId;
    QString oldActiveName;
    StreamingPreferences oldActiveSettings;
    StreamingPreferences oldTemplateSettings;
    loadTemplate(oldTemplateSettings);
    QSettings oldSnapshot(m_SnapshotPath, QSettings::IniFormat);
    QStringList affectedHosts = profileHostUuids(oldSnapshot);
    if (!hostUuid.isEmpty()) {
        QSettings before(m_SnapshotPath, QSettings::IniFormat);
        before.beginGroup(ROOT_GROUP);
        before.beginGroup(HOSTS_GROUP);
        before.beginGroup(hostKey(hostUuid));
        oldActiveId = before.value(ACTIVE_PROFILE).toString();
        if (!oldActiveId.isEmpty()) {
            loadProfile(hostUuid, oldActiveId, oldActiveName, oldActiveSettings);
        }
    }
    qulonglong revision = 0;
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Unable to lock shared settings"));
        return result;
    }
    if (!loadSharedIntoSnapshot(&revision)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Unable to reload shared settings"));
        return result;
    }
    m_SnapshotRevision = m_AcknowledgedRevision = revision;
    m_LastObservedRevision = revision;
    m_LocalSettingsDirty = false;
    writeInstanceStatus();
    QSettings newSnapshot(m_SnapshotPath, QSettings::IniFormat);
    for (const QString& uuid : profileHostUuids(newSnapshot)) {
        if (!affectedHosts.contains(uuid)) affectedHosts.append(uuid);
    }
    StreamingPreferences newTemplateSettings;
    loadTemplate(newTemplateSettings);
    const bool templateSettingsChanged =
        !oldTemplateSettings.streamSettingsEqual(newTemplateSettings);
    QString newActiveId;
    QString newActiveName;
    StreamingPreferences newActiveSettings;
    if (!hostUuid.isEmpty()) {
        QSettings after(m_SnapshotPath, QSettings::IniFormat);
        after.beginGroup(ROOT_GROUP);
        after.beginGroup(HOSTS_GROUP);
        after.beginGroup(hostKey(hostUuid));
        newActiveId = after.value(ACTIVE_PROFILE).toString();
        if (!newActiveId.isEmpty()) {
            loadProfile(hostUuid, newActiveId, newActiveName, newActiveSettings);
        }
    }
    const bool activeProfileChanged = oldActiveId != newActiveId ||
                                      oldActiveName != newActiveName;
    const bool activeSettingsChanged = activeProfileChanged ||
                                       (!oldActiveId.isEmpty() &&
                                        !oldActiveSettings.streamSettingsEqual(newActiveSettings));
    result.insert(QStringLiteral("revision"), QVariant::fromValue(revision));
    result.insert(QStringLiteral("oldActiveProfileId"), oldActiveId);
    result.insert(QStringLiteral("activeProfileId"), newActiveId);
    result.insert(QStringLiteral("activeSettingsChanged"), activeSettingsChanged);
    result.insert(QStringLiteral("activeProfileChanged"), activeProfileChanged);
    result.insert(QStringLiteral("templateSettingsChanged"), templateSettingsChanged);
    result.insert(QStringLiteral("error"), QString());
    lock.unlock();
    StreamingPreferences* appPreferences = StreamingPreferences::get();
    if (loadApplicationPreferences(*appPreferences)) {
        appPreferences->notifyAllChanged();
    }
    for (const QString& uuid : std::as_const(affectedHosts)) {
        emit profilesChanged(uuid);
    }
    emit sharedRevisionChanged();
    emit localSettingsDirtyChanged();
    return result;
}

bool StreamProfileManager::discardLocalChanges()
{
    const QVariantMap result = reloadSnapshot(QString(), true);
    return result.value(QStringLiteral("error")).toString().isEmpty();
}

bool StreamProfileManager::saveLocalSnapshot()
{
    if (!m_SnapshotReady) return false;
    if (!m_LocalSettingsDirty) return true;
    return publishSnapshot(false);
}

bool StreamProfileManager::overrideExternalChanges()
{
    if (!m_SnapshotReady) return false;
    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    StreamingPreferences* appPreferences = StreamingPreferences::get();
    appPreferences->saveApplicationSettings(settings);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setLastError(tr("Could not stage application settings for override."));
        return false;
    }
    if (!m_LocalSettingsDirty) {
        m_LocalSettingsDirty = true;
        emit localSettingsDirtyChanged();
    }
    return publishSnapshot(true);
}

bool StreamProfileManager::saveApplicationPreferences(const StreamingPreferences& preferences)
{
    if (!m_SnapshotReady) return false;
    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    QVariantMap oldValues;
    for (const QString& key : APP_PREFERENCE_KEYS) {
        oldValues.insert(key, settings.value(key));
    }
    preferences.saveApplicationSettings(settings);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;
    bool changed = false;
    for (const QString& key : APP_PREFERENCE_KEYS) {
        changed |= oldValues.value(key) != settings.value(key);
    }
    if (!changed) {
        return m_LocalSettingsDirty ? publishSnapshot() : true;
    }
    if (!m_LocalSettingsDirty) {
        m_LocalSettingsDirty = true;
        emit localSettingsDirtyChanged();
    }
    return publishSnapshot();
}

bool StreamProfileManager::loadApplicationPreferences(StreamingPreferences& preferences)
{
    if (!m_SnapshotReady) return false;
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;
    preferences.resetToStock();
    preferences.loadApplicationSettings(settings);
    return true;
}

void StreamProfileManager::setLastError(const QString& message)
{
    if (m_LastError == message) return;
    m_LastError = message;
    emit lastErrorChanged();
}

QString StreamProfileManager::hostKey(const QString& hostUuid) const
{
    return QString::fromLatin1(hostUuid.toUtf8().toHex());
}

QString StreamProfileManager::createProfileId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void StreamProfileManager::initializeTemplate()
{
    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    if (!settings.childGroups().contains(TEMPLATE_GROUP)) {
        settings.beginGroup(TEMPLATE_GROUP);
        StreamingPreferences stock;
        stock.saveStreamSettings(settings);
        settings.endGroup();
        settings.sync();
        m_LocalSettingsDirty = true;
        publishSnapshot();
    }
    settings.endGroup();
}

void StreamProfileManager::loadTemplate(StreamingPreferences& preferences)
{
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(TEMPLATE_GROUP);
    preferences.resetToStock();
    preferences.loadStreamSettings(settings);
    settings.endGroup();
    settings.endGroup();
}

void StreamProfileManager::saveTemplate(const StreamingPreferences& preferences)
{
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.remove(TEMPLATE_GROUP);
    settings.beginGroup(TEMPLATE_GROUP);
    preferences.saveStreamSettings(settings);
    settings.endGroup();
    settings.endGroup();
    settings.sync();
}

void StreamProfileManager::ensureHost(const QString& hostUuid)
{
    ensureHost(hostUuid, false);
}

void StreamProfileManager::ensureExistingHost(const QString& hostUuid)
{
    ensureHost(hostUuid, true);
}

void StreamProfileManager::ensureHost(const QString& hostUuid, bool useStockDefaults)
{
    if (hostUuid.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));

    QStringList order = settings.value(PROFILE_ORDER).toStringList();
    const QStringList originalOrder = order;
    const QString originalActiveProfile = settings.value(ACTIVE_PROFILE).toString();
    const int originalNextNumber = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
    settings.beginGroup("profiles");
    const QStringList storedProfiles = settings.childGroups();
    settings.endGroup();

    QStringList validOrder;
    for (const QString& profileId : std::as_const(order)) {
        if (storedProfiles.contains(profileId) && !validOrder.contains(profileId)) {
            validOrder.append(profileId);
        }
    }
    order = validOrder;

    if (order.isEmpty()) {
        const QString profileId = createProfileId();
        order.append(profileId);
        settings.setValue(PROFILE_ORDER, order);
        settings.setValue(ACTIVE_PROFILE, profileId);
        settings.setValue(NEXT_PROFILE_NUMBER, 2);

        settings.beginGroup("profiles");
        settings.beginGroup(profileId);
        settings.setValue(PROFILE_NAME, tr("Profile 1"));

        StreamingPreferences initial;
        if (!useStockDefaults) {
            loadTemplate(initial);
        }
        initial.saveStreamSettings(settings);

        settings.endGroup();
        settings.endGroup();
    }
    else {
        settings.setValue(PROFILE_ORDER, order);
        if (!order.contains(settings.value(ACTIVE_PROFILE).toString())) {
            settings.setValue(ACTIVE_PROFILE, order.first());
        }

        int nextNumber = qMax(1, settings.value(NEXT_PROFILE_NUMBER, 1).toInt());
        const QRegularExpression defaultNamePattern(
            QStringLiteral("^Profile (\\d+)$"));
        settings.beginGroup("profiles");
        for (const QString& profileId : std::as_const(order)) {
            settings.beginGroup(profileId);
            const QRegularExpressionMatch match = defaultNamePattern.match(
                settings.value(PROFILE_NAME).toString());
            if (match.hasMatch()) {
                nextNumber = qMax(nextNumber, match.captured(1).toInt() + 1);
            }
            settings.endGroup();
        }
        settings.endGroup();
        settings.setValue(NEXT_PROFILE_NUMBER, nextNumber);
    }
    settings.sync();
    const bool changed = originalOrder != order ||
                         originalActiveProfile != settings.value(ACTIVE_PROFILE).toString() ||
                         originalNextNumber != settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
    if (changed) {
        m_LocalSettingsDirty = true;
        emit localSettingsDirtyChanged();
        publishSnapshot();
    }
}

QVariantList StreamProfileManager::profiles(const QString& hostUuid)
{
    ensureHost(hostUuid);

    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    const QString active = settings.value(ACTIVE_PROFILE).toString();
    const QStringList order = settings.value(PROFILE_ORDER).toStringList();

    QVariantList result;
    settings.beginGroup("profiles");
    for (const QString& profileId : order) {
        settings.beginGroup(profileId);
        QVariantMap profile;
        profile.insert("profileId", profileId);
        profile.insert("name", settings.value(PROFILE_NAME, tr("Profile")).toString());
        profile.insert("active", profileId == active);
        result.append(profile);
        settings.endGroup();
    }

    return result;
}

QString StreamProfileManager::activeProfileId(const QString& hostUuid)
{
    ensureHost(hostUuid);
    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    return settings.value(ACTIVE_PROFILE).toString();
}

QString StreamProfileManager::activeProfileName(const QString& hostUuid)
{
    const QString active = activeProfileId(hostUuid);
    QString name;
    StreamingPreferences unused;
    QMutexLocker locker(&m_Mutex);
    loadProfile(hostUuid, active, name, unused);
    return name;
}

bool StreamProfileManager::activateProfile(const QString& hostUuid, const QString& profileId)
{
    ensureHost(hostUuid);
    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    const QStringList order = settings.value(PROFILE_ORDER).toStringList();
    if (!order.contains(profileId)) {
        return false;
    }
    const QString oldActiveProfile = settings.value(ACTIVE_PROFILE).toString();
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    settings.setValue(ACTIVE_PROFILE, profileId);
    settings.sync();
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot()) {
        QSettings rollback(m_SnapshotPath, QSettings::IniFormat);
        rollback.beginGroup(ROOT_GROUP);
        rollback.beginGroup(HOSTS_GROUP);
        rollback.beginGroup(hostKey(hostUuid));
        rollback.setValue(ACTIVE_PROFILE, oldActiveProfile);
        rollback.sync();
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        return false;
    }
    locker.unlock();
    emit profilesChanged(hostUuid);
    return true;
}

void StreamProfileManager::loadProfile(const QString& hostUuid, const QString& profileId,
                                       QString& name, StreamingPreferences& preferences)
{
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    settings.beginGroup("profiles");
    settings.beginGroup(profileId);
    name = settings.value(PROFILE_NAME, tr("Profile")).toString();
    preferences.resetToStock();
    preferences.loadStreamSettings(settings);
}

void StreamProfileManager::saveProfile(const QString& hostUuid, const QString& profileId,
                                       const QString& name, const StreamingPreferences& preferences)
{
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    settings.beginGroup("profiles");
    settings.remove(profileId);
    settings.beginGroup(profileId);
    settings.setValue(PROFILE_NAME, name);
    preferences.saveStreamSettings(settings);
    settings.sync();
}

StreamProfileEditor* StreamProfileManager::createEditor(const QString& hostUuid,
                                                        const QString& profileId,
                                                        bool newProfile)
{
    ensureHost(hostUuid);

    QString id = profileId;
    QString name;
    auto editor = new StreamProfileEditor(this, hostUuid, id, name, newProfile, false);

    QMutexLocker locker(&m_Mutex);
    if (newProfile) {
        QSettings settings(m_SnapshotPath, QSettings::IniFormat);
        settings.beginGroup(ROOT_GROUP);
        settings.beginGroup(HOSTS_GROUP);
        settings.beginGroup(hostKey(hostUuid));
        const int number = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
        editor->m_Name = tr("Profile %1").arg(number);
        loadTemplate(*editor->m_Settings);
    }
    else {
        if (id.isEmpty()) {
            QSettings settings(m_SnapshotPath, QSettings::IniFormat);
            settings.beginGroup(ROOT_GROUP);
            settings.beginGroup(HOSTS_GROUP);
            settings.beginGroup(hostKey(hostUuid));
            id = settings.value(ACTIVE_PROFILE).toString();
            editor->m_ProfileId = id;
        }
        loadProfile(hostUuid, id, editor->m_Name, *editor->m_Settings);
    }
    editor->acceptCurrentState();
    QQmlEngine::setObjectOwnership(editor, QQmlEngine::JavaScriptOwnership);
    return editor;
}

StreamProfileEditor* StreamProfileManager::createTemplateEditor()
{
    auto editor = new StreamProfileEditor(this, QString(), QString(), QString(), false, true);
    QMutexLocker locker(&m_Mutex);
    loadTemplate(*editor->m_Settings);
    editor->acceptCurrentState();
    QQmlEngine::setObjectOwnership(editor, QQmlEngine::JavaScriptOwnership);
    return editor;
}

StreamingPreferences* StreamProfileManager::createActiveSettings(const QString& hostUuid, QObject* parent)
{
    const QString id = activeProfileId(hostUuid);
    auto preferences = new StreamingPreferences(parent);
    QString unused;
    QMutexLocker locker(&m_Mutex);
    loadProfile(hostUuid, id, unused, *preferences);
    preferences->richPresence = StreamingPreferences::get()->richPresence;
    return preferences;
}

int StreamProfileManager::profileCount(const QString& hostUuid)
{
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    return settings.value(PROFILE_ORDER).toStringList().count();
}

bool StreamProfileManager::saveEditor(StreamProfileEditor* editor, bool overrideExternal)
{
    QMutexLocker locker(&m_Mutex);
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    if (editor->m_TemplateMode) {
        StreamingPreferences previousTemplate;
        loadTemplate(previousTemplate);
        saveTemplate(*editor->m_Settings);
        if (overrideExternal) {
            QSettings appSettings(m_SnapshotPath, QSettings::IniFormat);
            StreamingPreferences::get()->saveApplicationSettings(appSettings);
            appSettings.sync();
        }
        m_LocalSettingsDirty = true;
        emit localSettingsDirtyChanged();
        if (!publishSnapshot(overrideExternal)) {
            saveTemplate(previousTemplate);
            m_LocalSettingsDirty = wasLocallyDirty;
            emit localSettingsDirtyChanged();
            return false;
        }
        return true;
    }
    if (editor->m_Name.trimmed().isEmpty()) {
        setLastError(tr("Profile name cannot be empty."));
        return false;
    }

    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    QStringList order = settings.value(PROFILE_ORDER).toStringList();
    const QStringList originalOrder = order;
    const QString originalActiveProfile = settings.value(ACTIVE_PROFILE).toString();
    const int originalNextNumber = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
    QString originalName;
    StreamingPreferences originalSettings;
    if (!editor->m_NewProfile) {
        loadProfile(editor->m_HostUuid, editor->m_ProfileId, originalName, originalSettings);
    }

    const QString savedProfileId = editor->m_NewProfile ? createProfileId() : editor->m_ProfileId;
    if (editor->m_NewProfile) {
        order.append(savedProfileId);
        settings.setValue(PROFILE_ORDER, order);
        settings.setValue(NEXT_PROFILE_NUMBER,
                          settings.value(NEXT_PROFILE_NUMBER, 1).toInt() + 1);
    }
    else if (!order.contains(savedProfileId)) {
        // The profile may have been deleted by another instance after this
        // editor was opened. An explicit Save resurrects its stable ID.
        order.append(savedProfileId);
        settings.setValue(PROFILE_ORDER, order);
    }

    saveProfile(editor->m_HostUuid, savedProfileId,
                editor->m_Name.trimmed(), *editor->m_Settings);
    settings.setValue(ACTIVE_PROFILE, savedProfileId);
    settings.sync();
    if (overrideExternal) {
        QSettings appSettings(m_SnapshotPath, QSettings::IniFormat);
        StreamingPreferences::get()->saveApplicationSettings(appSettings);
        appSettings.sync();
    }
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot(overrideExternal)) {
        QSettings rollback(m_SnapshotPath, QSettings::IniFormat);
        rollback.beginGroup(ROOT_GROUP);
        rollback.beginGroup(HOSTS_GROUP);
        rollback.beginGroup(hostKey(editor->m_HostUuid));
        rollback.setValue(PROFILE_ORDER, originalOrder);
        rollback.setValue(ACTIVE_PROFILE, originalActiveProfile);
        rollback.setValue(NEXT_PROFILE_NUMBER, originalNextNumber);
        rollback.endGroup();
        rollback.endGroup();
        rollback.endGroup();
        rollback.sync();
        if (editor->m_NewProfile) {
            QSettings cleanup(m_SnapshotPath, QSettings::IniFormat);
            cleanup.beginGroup(ROOT_GROUP);
            cleanup.beginGroup(HOSTS_GROUP);
            cleanup.beginGroup(hostKey(editor->m_HostUuid));
            cleanup.beginGroup("profiles");
            cleanup.remove(savedProfileId);
            cleanup.sync();
        }
        else {
            saveProfile(editor->m_HostUuid, editor->m_ProfileId,
                        originalName, originalSettings);
        }
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        return false;
    }
    editor->m_ProfileId = savedProfileId;
    editor->m_NewProfile = false;
    locker.unlock();
    emit profilesChanged(editor->m_HostUuid);
    return true;
}

bool StreamProfileManager::copyEditor(StreamProfileEditor* editor)
{
    if (editor->m_TemplateMode || editor->m_NewProfile || editor->m_Dirty) {
        return false;
    }

    QMutexLocker locker(&m_Mutex);
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    QStringList order = settings.value(PROFILE_ORDER).toStringList();
    const QStringList originalOrder = order;
    const int number = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
    const QString originalActiveProfile = settings.value(ACTIVE_PROFILE).toString();
    const QString id = createProfileId();
    const QString name = tr("Profile %1").arg(number);
    order.append(id);
    settings.setValue(PROFILE_ORDER, order);
    settings.setValue(NEXT_PROFILE_NUMBER, number + 1);
    settings.setValue(ACTIVE_PROFILE, id);
    saveProfile(editor->m_HostUuid, id, name, *editor->m_Settings);
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot()) {
        QSettings rollback(m_SnapshotPath, QSettings::IniFormat);
        rollback.beginGroup(ROOT_GROUP);
        rollback.beginGroup(HOSTS_GROUP);
        rollback.beginGroup(hostKey(editor->m_HostUuid));
        rollback.setValue(PROFILE_ORDER, originalOrder);
        rollback.setValue(NEXT_PROFILE_NUMBER, number);
        rollback.setValue(ACTIVE_PROFILE, originalActiveProfile);
        rollback.beginGroup("profiles");
        rollback.remove(id);
        rollback.sync();
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        return false;
    }

    editor->m_ProfileId = id;
    editor->m_Name = name;
    locker.unlock();
    emit profilesChanged(editor->m_HostUuid);
    return true;
}

bool StreamProfileManager::deleteEditor(StreamProfileEditor* editor)
{
    if (editor->m_TemplateMode || editor->m_NewProfile) {
        return false;
    }

    QMutexLocker locker(&m_Mutex);
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    QStringList order = settings.value(PROFILE_ORDER).toStringList();
    const QStringList originalOrder = order;
    const QString originalActiveProfile = settings.value(ACTIVE_PROFILE).toString();
    QString oldName;
    StreamingPreferences oldSettings;
    loadProfile(editor->m_HostUuid, editor->m_ProfileId, oldName, oldSettings);
    if (order.count() <= 1 || !order.removeOne(editor->m_ProfileId)) {
        return false;
    }

    settings.setValue(PROFILE_ORDER, order);
    if (settings.value(ACTIVE_PROFILE).toString() == editor->m_ProfileId) {
        settings.setValue(ACTIVE_PROFILE, order.first());
    }
    settings.beginGroup("profiles");
    settings.remove(editor->m_ProfileId);
    settings.sync();
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot()) {
        QSettings rollback(m_SnapshotPath, QSettings::IniFormat);
        rollback.beginGroup(ROOT_GROUP);
        rollback.beginGroup(HOSTS_GROUP);
        rollback.beginGroup(hostKey(editor->m_HostUuid));
        rollback.setValue(PROFILE_ORDER, originalOrder);
        rollback.setValue(ACTIVE_PROFILE, originalActiveProfile);
        rollback.endGroup();
        rollback.endGroup();
        rollback.endGroup();
        saveProfile(editor->m_HostUuid, editor->m_ProfileId, oldName, oldSettings);
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        return false;
    }
    locker.unlock();
    emit profilesChanged(editor->m_HostUuid);
    return true;
}

bool StreamProfileManager::setEditorAsDefault(StreamProfileEditor* editor)
{
    if (editor->m_TemplateMode || editor->m_NewProfile || editor->m_Dirty) {
        return false;
    }
    QMutexLocker locker(&m_Mutex);
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    QString oldName;
    StreamingPreferences oldSettings;
    loadTemplate(oldSettings);
    saveTemplate(*editor->m_Settings);
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot()) {
        saveTemplate(oldSettings);
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        return false;
    }
    return true;
}

bool StreamProfileManager::switchEditorProfile(
    StreamProfileEditor* editor, const QString& profileId)
{
    if (editor->m_TemplateMode || editor->m_Dirty || profileId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    const QStringList order = settings.value(PROFILE_ORDER).toStringList();
    if (!order.contains(profileId)) {
        return false;
    }

    const QString oldProfileId = editor->m_ProfileId;
    const QString oldName = editor->m_Name;
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    QString newName;
    StreamingPreferences newSettings;
    loadProfile(editor->m_HostUuid, profileId, newName, newSettings);
    settings.setValue(ACTIVE_PROFILE, profileId);
    settings.sync();
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot()) {
        QSettings rollback(m_SnapshotPath, QSettings::IniFormat);
        rollback.beginGroup(ROOT_GROUP);
        rollback.beginGroup(HOSTS_GROUP);
        rollback.beginGroup(hostKey(editor->m_HostUuid));
        rollback.setValue(ACTIVE_PROFILE, oldProfileId);
        rollback.sync();
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        return false;
    }
    editor->m_ProfileId = profileId;
    editor->m_Name = newName;
    editor->m_NewProfile = false;
    editor->m_Settings->copyStreamSettingsFrom(newSettings);
    locker.unlock();

    editor->acceptCurrentState();
    editor->m_Settings->notifyAllChanged();
    emit profilesChanged(editor->m_HostUuid);
    return true;
}

bool StreamProfileManager::beginEditorProfile(StreamProfileEditor* editor)
{
    if (editor->m_TemplateMode || editor->m_Dirty) {
        return false;
    }

    QMutexLocker locker(&m_Mutex);
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    const int number = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();

    editor->m_ProfileId.clear();
    editor->m_Name = tr("Profile %1").arg(number);
    editor->m_NewProfile = true;
    loadTemplate(*editor->m_Settings);
    locker.unlock();

    editor->acceptCurrentState();
    editor->m_Settings->notifyAllChanged();
    return true;
}

void StreamProfileManager::removeHost(const QString& hostUuid)
{
    QMutexLocker locker(&m_Mutex);
    const bool wasLocallyDirty = m_LocalSettingsDirty;
    const QString prefix = QStringLiteral("streamProfiles/hosts/%1/").arg(hostKey(hostUuid));
    QSettings settings(m_SnapshotPath, QSettings::IniFormat);
    QVariantMap originalValues;
    for (const QString& key : settings.allKeys()) {
        if (key.startsWith(prefix)) {
            originalValues.insert(key, settings.value(key));
        }
    }
    if (originalValues.isEmpty()) return;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.remove(hostKey(hostUuid));
    settings.sync();
    m_LocalSettingsDirty = true;
    emit localSettingsDirtyChanged();
    if (!publishSnapshot()) {
        QSettings rollback(m_SnapshotPath, QSettings::IniFormat);
        for (const QString& key : rollback.allKeys()) {
            if (key.startsWith(prefix)) rollback.remove(key);
        }
        for (auto it = originalValues.constBegin(); it != originalValues.constEnd(); ++it) {
            rollback.setValue(it.key(), it.value());
        }
        rollback.sync();
        m_LocalSettingsDirty = wasLocallyDirty;
        emit localSettingsDirtyChanged();
        updateInstanceStatus();
        return;
    }
    locker.unlock();
    emit profilesChanged(hostUuid);
}

StreamProfileEditor::StreamProfileEditor(StreamProfileManager* manager,
                                         const QString& hostUuid,
                                         const QString& profileId,
                                         const QString& name,
                                         bool newProfile,
                                         bool templateMode,
                                         QObject* parent)
    : QObject(parent),
      m_Manager(manager),
      m_Settings(new StreamingPreferences(this)),
      m_OriginalSettings(new StreamingPreferences(this)),
      m_HostUuid(hostUuid),
      m_ProfileId(profileId),
      m_Name(name),
      m_Dirty(false),
      m_NewProfile(newProfile),
      m_TemplateMode(templateMode)
{
    connect(m_Manager, &StreamProfileManager::lastErrorChanged,
            this, &StreamProfileEditor::lastErrorChanged);
#define TRACK(signal) connect(m_Settings, &StreamingPreferences::signal, this, &StreamProfileEditor::updateDirty)
    TRACK(displayModeChanged); TRACK(bitrateChanged); TRACK(unlockBitrateChanged);
    TRACK(autoAdjustBitrateChanged); TRACK(enableVsyncChanged); TRACK(gameOptimizationsChanged);
    TRACK(playAudioOnHostChanged); TRACK(multiControllerChanged); TRACK(quitAppAfterChanged);
    TRACK(absoluteMouseModeChanged); TRACK(absoluteTouchModeChanged); TRACK(audioConfigChanged);
    TRACK(videoCodecConfigChanged); TRACK(enableHdrChanged); TRACK(enableYUV444Changed);
    TRACK(videoDecoderSelectionChanged); TRACK(windowModeChanged); TRACK(framePacingChanged);
    TRACK(enableVrrChanged); TRACK(vrrSmoothnessChanged);
    TRACK(connectionWarningsChanged); TRACK(configurationWarningsChanged); TRACK(gamepadMouseChanged);
    TRACK(showPerformanceOverlayChanged); TRACK(mouseButtonsChanged); TRACK(muteOnFocusLossChanged);
    TRACK(backgroundGamepadChanged); TRACK(reverseScrollDirectionChanged); TRACK(swapFaceButtonsChanged);
    TRACK(captureSysKeysModeChanged); TRACK(keepAwakeChanged);
#undef TRACK
}

QString StreamProfileEditor::lastError() const
{
    return m_Manager->lastError();
}

void StreamProfileEditor::setName(const QString& name)
{
    if (m_Name == name) {
        return;
    }
    m_Name = name;
    emit nameChanged();
    updateDirty();
}

void StreamProfileEditor::updateDirty()
{
    const bool dirtyNow = m_NewProfile ||
                          m_Name != m_OriginalName ||
                          !m_Settings->streamSettingsEqual(*m_OriginalSettings);
    if (dirtyNow != m_Dirty) {
        m_Dirty = dirtyNow;
        emit dirtyChanged();
    }
}

void StreamProfileEditor::acceptCurrentState()
{
    m_OriginalSettings->copyStreamSettingsFrom(*m_Settings);
    m_OriginalName = m_Name;
    const bool wasDirty = m_Dirty;
    m_Dirty = m_NewProfile;
    if (wasDirty != m_Dirty) {
        emit dirtyChanged();
    }
}

bool StreamProfileEditor::canDelete() const
{
    return !m_TemplateMode && !m_NewProfile &&
           m_Manager->profileCount(m_HostUuid) > 1;
}

bool StreamProfileEditor::save(bool overrideExternal)
{
    if (!m_Manager->saveEditor(this, overrideExternal)) {
        return false;
    }
    emit profileIdChanged();
    emit newProfileChanged();
    emit nameChanged();
    acceptCurrentState();
    emit canDeleteChanged();
    emit saved();
    return true;
}

bool StreamProfileEditor::copy()
{
    if (!m_Manager->copyEditor(this)) {
        return false;
    }
    emit profileIdChanged();
    emit nameChanged();
    acceptCurrentState();
    emit canDeleteChanged();
    emit saved();
    return true;
}

bool StreamProfileEditor::remove()
{
    if (!m_Manager->deleteEditor(this)) {
        return false;
    }
    emit removed();
    return true;
}

bool StreamProfileEditor::setAsDefault()
{
    return m_Manager->setEditorAsDefault(this);
}

void StreamProfileEditor::resetToStock()
{
    m_Settings->resetToStock();
    m_Settings->notifyAllChanged();
    updateDirty();
}

void StreamProfileEditor::discardChanges()
{
    m_Settings->copyStreamSettingsFrom(*m_OriginalSettings);
    m_Settings->notifyAllChanged();
    m_Name = m_OriginalName;
    m_NewProfile = false;
    emit nameChanged();
    emit newProfileChanged();
    updateDirty();
}

bool StreamProfileEditor::switchToProfile(const QString& profileId)
{
    if (!m_Manager->switchEditorProfile(this, profileId)) {
        return false;
    }

    emit profileIdChanged();
    emit nameChanged();
    emit newProfileChanged();
    emit canDeleteChanged();
    return true;
}
bool StreamProfileEditor::beginNewProfile()
{
    if (!m_Manager->beginEditorProfile(this)) {
        return false;
    }

    emit profileIdChanged();
    emit nameChanged();
    emit newProfileChanged();
    emit canDeleteChanged();
    return true;
}
