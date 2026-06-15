#include "streamprofilemanager.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

namespace {

const char* const ROOT_GROUP = "streamProfiles";
const char* const TEMPLATE_GROUP = "template";
const char* const HOSTS_GROUP = "hosts";
const char* const PROFILE_ORDER = "profileOrder";
const char* const ACTIVE_PROFILE = "activeProfile";
const char* const NEXT_PROFILE_NUMBER = "nextProfileNumber";
const char* const PROFILE_NAME = "name";

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
    initializeTemplate();
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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    if (!settings.childGroups().contains(TEMPLATE_GROUP)) {
        settings.beginGroup(TEMPLATE_GROUP);
        StreamingPreferences stock;
        stock.saveStreamSettings(settings);
        settings.endGroup();
    }
    settings.endGroup();
}

void StreamProfileManager::loadTemplate(StreamingPreferences& preferences)
{
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(TEMPLATE_GROUP);
    preferences.resetToStock();
    preferences.loadStreamSettings(settings);
    settings.endGroup();
    settings.endGroup();
}

void StreamProfileManager::saveTemplate(const StreamingPreferences& preferences)
{
    QSettings settings;
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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));

    QStringList order = settings.value(PROFILE_ORDER).toStringList();
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
}

QVariantList StreamProfileManager::profiles(const QString& hostUuid)
{
    ensureHost(hostUuid);

    QMutexLocker locker(&m_Mutex);
    QSettings settings;
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
    QSettings settings;
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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    const QStringList order = settings.value(PROFILE_ORDER).toStringList();
    if (!order.contains(profileId)) {
        return false;
    }
    settings.setValue(ACTIVE_PROFILE, profileId);
    settings.sync();
    locker.unlock();
    emit profilesChanged(hostUuid);
    return true;
}

void StreamProfileManager::loadProfile(const QString& hostUuid, const QString& profileId,
                                       QString& name, StreamingPreferences& preferences)
{
    QSettings settings;
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
    QSettings settings;
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
        QSettings settings;
        settings.beginGroup(ROOT_GROUP);
        settings.beginGroup(HOSTS_GROUP);
        settings.beginGroup(hostKey(hostUuid));
        const int number = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
        editor->m_Name = tr("Profile %1").arg(number);
        loadTemplate(*editor->m_Settings);
    }
    else {
        if (id.isEmpty()) {
            QSettings settings;
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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(hostUuid));
    return settings.value(PROFILE_ORDER).toStringList().count();
}

bool StreamProfileManager::saveEditor(StreamProfileEditor* editor)
{
    QMutexLocker locker(&m_Mutex);
    if (editor->m_TemplateMode) {
        saveTemplate(*editor->m_Settings);
        return true;
    }
    if (editor->m_Name.trimmed().isEmpty()) {
        return false;
    }

    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    QStringList order = settings.value(PROFILE_ORDER).toStringList();

    if (editor->m_NewProfile) {
        editor->m_ProfileId = createProfileId();
        order.append(editor->m_ProfileId);
        settings.setValue(PROFILE_ORDER, order);
        settings.setValue(NEXT_PROFILE_NUMBER,
                          settings.value(NEXT_PROFILE_NUMBER, 1).toInt() + 1);
        editor->m_NewProfile = false;
    }

    saveProfile(editor->m_HostUuid, editor->m_ProfileId,
                editor->m_Name.trimmed(), *editor->m_Settings);
    settings.setValue(ACTIVE_PROFILE, editor->m_ProfileId);
    settings.sync();
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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    QStringList order = settings.value(PROFILE_ORDER).toStringList();
    const int number = settings.value(NEXT_PROFILE_NUMBER, 1).toInt();
    const QString id = createProfileId();
    const QString name = tr("Profile %1").arg(number);
    order.append(id);
    settings.setValue(PROFILE_ORDER, order);
    settings.setValue(NEXT_PROFILE_NUMBER, number + 1);
    settings.setValue(ACTIVE_PROFILE, id);
    saveProfile(editor->m_HostUuid, id, name, *editor->m_Settings);

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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    QStringList order = settings.value(PROFILE_ORDER).toStringList();
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
    saveTemplate(*editor->m_Settings);
    return true;
}

bool StreamProfileManager::switchEditorProfile(
    StreamProfileEditor* editor, const QString& profileId)
{
    if (editor->m_TemplateMode || editor->m_Dirty || profileId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_Mutex);
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.beginGroup(hostKey(editor->m_HostUuid));
    const QStringList order = settings.value(PROFILE_ORDER).toStringList();
    if (!order.contains(profileId)) {
        return false;
    }

    editor->m_ProfileId = profileId;
    editor->m_NewProfile = false;
    loadProfile(editor->m_HostUuid, profileId, editor->m_Name,
                *editor->m_Settings);
    settings.setValue(ACTIVE_PROFILE, profileId);
    settings.sync();
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
    QSettings settings;
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
    QSettings settings;
    settings.beginGroup(ROOT_GROUP);
    settings.beginGroup(HOSTS_GROUP);
    settings.remove(hostKey(hostUuid));
    settings.sync();
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
#define TRACK(signal) connect(m_Settings, &StreamingPreferences::signal, this, &StreamProfileEditor::updateDirty)
    TRACK(displayModeChanged); TRACK(bitrateChanged); TRACK(unlockBitrateChanged);
    TRACK(autoAdjustBitrateChanged); TRACK(enableVsyncChanged); TRACK(gameOptimizationsChanged);
    TRACK(playAudioOnHostChanged); TRACK(multiControllerChanged); TRACK(quitAppAfterChanged);
    TRACK(absoluteMouseModeChanged); TRACK(absoluteTouchModeChanged); TRACK(audioConfigChanged);
    TRACK(videoCodecConfigChanged); TRACK(enableHdrChanged); TRACK(enableYUV444Changed);
    TRACK(videoDecoderSelectionChanged); TRACK(windowModeChanged); TRACK(framePacingChanged);
    TRACK(connectionWarningsChanged); TRACK(configurationWarningsChanged); TRACK(gamepadMouseChanged);
    TRACK(showPerformanceOverlayChanged); TRACK(mouseButtonsChanged); TRACK(muteOnFocusLossChanged);
    TRACK(backgroundGamepadChanged); TRACK(reverseScrollDirectionChanged); TRACK(swapFaceButtonsChanged);
    TRACK(captureSysKeysModeChanged); TRACK(keepAwakeChanged);
#undef TRACK
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

bool StreamProfileEditor::save()
{
    if (!m_Manager->saveEditor(this)) {
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
