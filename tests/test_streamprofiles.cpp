#include <QtTest>

#include "cli/commandlineparser.h"
#include "settings/buildidentity.h"
#include "settings/streamprofilemanager.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>
#include <QFileInfo>
#include <QLockFile>
#include <QTemporaryDir>

#include <memory>

static bool bumpExternalSettingsRevision(QSettings& settings)
{
    const QString lockPath = QFileInfo(settings.fileName()).absolutePath() +
                             QStringLiteral("/moonlight-settings.lock");
    QLockFile lock(lockPath);
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(1000)) return false;
    settings.sync();
    const qulonglong revision = settings.value(
        QStringLiteral("settingsCoordination/revision"), 0).toULongLong();
    settings.setValue(QStringLiteral("settingsCoordination/revision"), revision + 1);
    settings.setValue(QStringLiteral("settingsCoordination/changedBy"), QStringLiteral("test-writer"));
    settings.setValue(QStringLiteral("settingsCoordination/instanceNumber"), 2);
    settings.setValue(QStringLiteral("settingsCoordination/changedAt"), QStringLiteral("test-time"));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

static bool reloadTestSnapshot(StreamProfileManager* manager)
{
    return manager->reloadSnapshot(QString(), true)
        .value(QStringLiteral("error")).toString().isEmpty();
}

class StreamProfileTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void singletonSurvivesQmlEngineShutdown();
    void profileLifecycle();
    void editorProfileSwitching();
    void vrrChangesMarkProfileDirty();
    void settingsCoordinationDetectsReloadAndOwnership();
    void settingsCoordinationIndependentSnapshots();
    void settingsCoordinationReloadNotifiesAndResavesDeletedProfile();
    void settingsCoordinationLockFailureDoesNotAdvanceRevision();
    void serializationAndCorruptData();
    void cliOverridesAreTemporary();
    void uiFramePacingPreference();
    void uiGraphicsPreference();
    void uiProfileControlsPreference();
    void versionIdentity();

private:
    QTemporaryDir m_SettingsDir;
};

void StreamProfileTests::initTestCase()
{
    QVERIFY(m_SettingsDir.isValid());

    QCoreApplication::setOrganizationName(QStringLiteral("Moonlight Profile Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("Moonlight"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       m_SettingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       m_SettingsDir.path());

    QSettings settings;
    settings.clear();
    settings.setValue(QStringLiteral("width"), 4242);
    settings.sync();
}

void StreamProfileTests::singletonSurvivesQmlEngineShutdown()
{
    qmlRegisterSingletonType<StreamProfileManager>(
        "StreamProfileManagerTest", 1, 0, "StreamProfileManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            return StreamProfileManager::get();
        });

    StreamProfileManager* manager = StreamProfileManager::get();
    QCOMPARE(QQmlEngine::objectOwnership(manager), QQmlEngine::CppOwnership);

    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(
            "import QtQml 2.0\n"
            "import StreamProfileManagerTest 1.0\n"
            "QtObject {\n"
            "    property var profileList: StreamProfileManager.profiles(\"ownership-host\")\n"
            "}\n",
            QUrl(QStringLiteral("qrc:/singleton-ownership-test.qml")));

        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
    }

    QCOMPARE(StreamProfileManager::get(), manager);
    QCOMPARE(QQmlEngine::objectOwnership(manager), QQmlEngine::CppOwnership);
    QCOMPARE(manager->profiles(QStringLiteral("ownership-host")).count(), 1);
}

void StreamProfileTests::profileLifecycle()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    const QString host1 = QStringLiteral("host-one");
    const QString host2 = QStringLiteral("host-two");

    QCOMPARE(StreamingPreferences::get()->width, 1280);

    QVariantList profiles = manager->profiles(host1);
    QCOMPARE(profiles.count(), 1);
    QCOMPARE(profiles.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Profile 1"));

    std::unique_ptr<StreamingPreferences> initial(
        manager->createActiveSettings(host1));
    QCOMPARE(initial->width, 1280);
    QCOMPARE(initial->height, 720);

    std::unique_ptr<StreamProfileEditor> templateEditor(
        manager->createTemplateEditor());
    templateEditor->settings()->width = 1920;
    templateEditor->settings()->height = 1080;
    templateEditor->settings()->displayModeChanged();
    QVERIFY(templateEditor->dirty());
    QVERIFY(templateEditor->save());

    std::unique_ptr<StreamingPreferences> existingHost(
        manager->createActiveSettings(host1));
    QCOMPARE(existingHost->width, 1280);

    const QString migratedHost = QStringLiteral("persisted-before-profiles");
    manager->ensureExistingHost(migratedHost);
    std::unique_ptr<StreamingPreferences> migrated(
        manager->createActiveSettings(migratedHost));
    QCOMPARE(migrated->width, 1280);
    QCOMPARE(migrated->height, 720);

    std::unique_ptr<StreamingPreferences> newHost(
        manager->createActiveSettings(host2));
    QCOMPARE(newHost->width, 1920);
    QCOMPARE(newHost->height, 1080);

    std::unique_ptr<StreamProfileEditor> draft(
        manager->createEditor(host1, QString(), true));
    QCOMPARE(draft->name(), QStringLiteral("Profile 2"));
    QCOMPARE(draft->settings()->width, 1920);
    QVERIFY(draft->isNewProfile());
    QVERIFY(draft->save());
    QVERIFY(!draft->isNewProfile());
    QCOMPARE(manager->profiles(host1).count(), 2);
    QCOMPARE(manager->activeProfileId(host1), draft->profileId());

    std::unique_ptr<StreamingPreferences> snapshot(
        manager->createActiveSettings(host1));
    QCOMPARE(snapshot->width, 1920);

    draft->settings()->width = 2560;
    draft->settings()->displayModeChanged();
    QVERIFY(draft->save());
    QCOMPARE(snapshot->width, 1920);
    std::unique_ptr<StreamingPreferences> updated(
        manager->createActiveSettings(host1));
    QCOMPARE(updated->width, 2560);

    QVERIFY(draft->copy());
    QCOMPARE(draft->name(), QStringLiteral("Profile 3"));
    QCOMPARE(manager->profiles(host1).count(), 3);
    QVERIFY(draft->canDelete());
    QVERIFY(draft->remove());
    QCOMPARE(manager->profiles(host1).count(), 2);

    std::unique_ptr<StreamProfileEditor> remaining(
        manager->createEditor(host1, manager->activeProfileId(host1), false));
    QVERIFY(remaining->remove());
    QCOMPARE(manager->profiles(host1).count(), 1);

    std::unique_ptr<StreamProfileEditor> last(
        manager->createEditor(host1, manager->activeProfileId(host1), false));
    QVERIFY(!last->canDelete());
    QVERIFY(!last->remove());

    last->settings()->width = 3440;
    last->settings()->displayModeChanged();
    QVERIFY(last->save());
    QVERIFY(last->setAsDefault());

    std::unique_ptr<StreamProfileEditor> newDraft(
        manager->createEditor(host1, QString(), true));
    QCOMPARE(newDraft->name(), QStringLiteral("Profile 4"));
    QCOMPARE(newDraft->settings()->width, 3440);
    newDraft->resetToStock();
    QCOMPARE(newDraft->settings()->width, 1280);

    manager->removeHost(host2);
    QCOMPARE(manager->profiles(host2).count(), 1);
    std::unique_ptr<StreamingPreferences> recreated(
        manager->createActiveSettings(host2));
    QCOMPARE(recreated->width, 3440);

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("width")).toInt(), 4242);
    QVERIFY(QFileInfo::exists(settings.fileName()));
}

void StreamProfileTests::versionIdentity()
{
    QCOMPARE(BuildIdentity::buildLabel(),
             QStringLiteral("beta (mastershogo's version)"));
    QCOMPARE(BuildIdentity::displayVersion(),
             QStringLiteral(VERSION_STR) +
                 QStringLiteral("-beta (mastershogo's version)"));
    QVERIFY(BuildIdentity::isCustomBuild());

    QCOMPARE(BuildIdentity::displayVersion(QStringLiteral("7.0.0.0"),
                                           QStringLiteral("dev (Contributor)")),
             QStringLiteral("7.0.0.0-dev (Contributor)"));
    QCOMPARE(BuildIdentity::displayVersion(QStringLiteral("7.0.1.0"),
                                           QStringLiteral("beta (Contributor)")),
             QStringLiteral("7.0.1.0-beta (Contributor)"));
    QCOMPARE(BuildIdentity::displayVersion(QStringLiteral("7.0.2.0"),
                                           QStringLiteral("(Contributor)")),
             QStringLiteral("7.0.2.0 (Contributor)"));
    QCOMPARE(BuildIdentity::displayVersion(QStringLiteral("1.0.0.0"),
                                           QString()),
             QStringLiteral("1.0.0.0"));
}

void StreamProfileTests::editorProfileSwitching()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    const QString host = QStringLiteral("editor-switch-host");
    const QString firstId = manager->activeProfileId(host);

    std::unique_ptr<StreamProfileEditor> editor(
        manager->createEditor(host, firstId, false));
    editor->settings()->width = 1600;
    emit editor->settings()->displayModeChanged();
    QVERIFY(editor->save());

    std::unique_ptr<StreamProfileEditor> second(
        manager->createEditor(host, QString(), true));
    second->settings()->width = 1920;
    emit second->settings()->displayModeChanged();
    QVERIFY(second->save());
    const QString secondId = second->profileId();
    QCOMPARE(manager->activeProfileId(host), secondId);

    second->settings()->width = 2560;
    emit second->settings()->displayModeChanged();
    QVERIFY(second->dirty());
    QVERIFY(!second->switchToProfile(firstId));
    QCOMPARE(manager->activeProfileId(host), secondId);

    second->discardChanges();
    QVERIFY(second->switchToProfile(firstId));
    QCOMPARE(second->profileId(), firstId);
    QCOMPARE(second->settings()->width, 1600);
    QCOMPARE(manager->activeProfileId(host), firstId);

    QVERIFY(second->beginNewProfile());
    QVERIFY(second->isNewProfile());
    QVERIFY(second->dirty());
    QCOMPARE(second->name(), QStringLiteral("Profile 3"));
    QCOMPARE(manager->activeProfileId(host), firstId);

    second->discardChanges();
    QVERIFY(second->switchToProfile(secondId));
    QCOMPARE(second->settings()->width, 1920);
    QCOMPARE(manager->activeProfileId(host), secondId);
}

void StreamProfileTests::vrrChangesMarkProfileDirty()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    const QString host = QStringLiteral("vrr-dirty-host");
    std::unique_ptr<StreamProfileEditor> editor(
        manager->createEditor(host, manager->activeProfileId(host), false));
    QVERIFY(!editor->dirty());

    QVERIFY(editor->settings()->setProperty("enableVrr", true));
    QVERIFY(editor->dirty());
    QVERIFY(editor->settings()->setProperty("enableVrr", false));
    QVERIFY(!editor->dirty());
    QVERIFY(editor->settings()->setProperty("enableVrr", true));
    QVERIFY(editor->dirty());
    QVERIFY(editor->save());
    QVERIFY(!editor->dirty());

    QVERIFY(editor->settings()->setProperty("vrrSmoothness", true));
    QVERIFY(editor->dirty());
    QVERIFY(editor->settings()->setProperty("vrrSmoothness", false));
    QVERIFY(!editor->dirty());
    QVERIFY(editor->settings()->setProperty("vrrSmoothness", true));
    QVERIFY(editor->dirty());
    QVERIFY(editor->save());
    QVERIFY(!editor->dirty());

    QVERIFY(editor->settings()->setProperty("fps", 120));
    QVERIFY(editor->dirty());
    QVERIFY(editor->save());

    std::unique_ptr<StreamingPreferences> saved(manager->createActiveSettings(host));
    QVERIFY(saved->enableVrr);
    QVERIFY(saved->vrrSmoothness);
    QCOMPARE(saved->fps, 120);
}

void StreamProfileTests::settingsCoordinationDetectsReloadAndOwnership()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    QVERIFY(manager->isReady());
    const QString host = QStringLiteral("coordination-host");
    QVERIFY(!manager->profiles(host).isEmpty());
    const QString profileId = manager->activeProfileId(host);
    QVERIFY(!profileId.isEmpty());

    QSettings shared;
    qulonglong revision = shared.value(QStringLiteral("settingsCoordination/revision"), 0).toULongLong();
    shared.setValue(QStringLiteral("settingsCoordination/revision"), revision + 1);
    shared.setValue(QStringLiteral("settingsCoordination/changedBy"), QStringLiteral("external-instance"));
    shared.setValue(QStringLiteral("settingsCoordination/instanceNumber"), 2);
    shared.setValue(QStringLiteral("settingsCoordination/changedAt"), QStringLiteral("test-time"));
    QVERIFY(bumpExternalSettingsRevision(shared));

    QVariantMap external = manager->checkForExternalChanges();
    QVERIFY2(external.value(QStringLiteral("error")).toString().isEmpty(),
             qPrintable(external.value(QStringLiteral("error")).toString()));
    QVERIFY(external.value(QStringLiteral("changed")).toBool());
    QCOMPARE(external.value(QStringLiteral("changedBy")).toString(), QStringLiteral("test-writer"));
    // A writer can advance the revision after the dialog data was captured.
    // An Ignore choice based on the stale observation must not acknowledge it.
    QVERIFY(bumpExternalSettingsRevision(shared));
    QVERIFY(!manager->acknowledgeExternalChanges());
    external = manager->checkForExternalChanges();
    QVERIFY(external.value(QStringLiteral("changed")).toBool());
    QVERIFY(manager->acknowledgeExternalChanges());
    QVERIFY(!manager->checkForExternalChanges().value(QStringLiteral("changed")).toBool());
    QVERIFY(manager->localSettingsDirty());

    // Ignore keeps the old profile image. Saving that acknowledged local
    // snapshot takes ownership and advances the shared revision.
    QVERIFY(manager->saveLocalSnapshot());
    QVERIFY(!manager->localSettingsDirty());
    shared.sync();
    revision = shared.value(QStringLiteral("settingsCoordination/revision")).toULongLong();
    QVERIFY(revision >= external.value(QStringLiteral("revision")).toULongLong() + 1);

    // A profile edit made externally is detected by stable profile ID and
    // actual serialized settings, then Reload adopts it.
    const QString profilePath = QStringLiteral("streamProfiles/hosts/%1/profiles/%2/fps")
                                    .arg(QString::fromLatin1(host.toUtf8().toHex()), profileId);
    shared.setValue(profilePath, 144);
    QVERIFY(bumpExternalSettingsRevision(shared));
    external = manager->checkForExternalChanges();
    QVERIFY(external.value(QStringLiteral("changed")).toBool());
    const QVariantMap reload = manager->reloadSnapshot(host);
    QVERIFY2(reload.value(QStringLiteral("error")).toString().isEmpty(),
             qPrintable(reload.value(QStringLiteral("error")).toString()));
    QCOMPARE(reload.value(QStringLiteral("oldActiveProfileId")).toString(), profileId);
    QCOMPARE(reload.value(QStringLiteral("activeProfileId")).toString(), profileId);
    QVERIFY(reload.value(QStringLiteral("activeSettingsChanged")).toBool());
    std::unique_ptr<StreamingPreferences> adopted(manager->createActiveSettings(host));
    QCOMPARE(adopted->fps, 144);

    // Explicit Override is allowed to replace the last shared revision.
    QVERIFY(bumpExternalSettingsRevision(shared));
    QVERIFY(manager->checkForExternalChanges().value(QStringLiteral("changed")).toBool());
    QVERIFY(manager->overrideExternalChanges());
    shared.sync();
    QCOMPARE(shared.value(QStringLiteral("settingsCoordination/revision")).toULongLong(), revision + 3);
    QVERIFY(!manager->checkForExternalChanges().value(QStringLiteral("changed")).toBool());
}

void StreamProfileTests::settingsCoordinationIndependentSnapshots()
{
    StreamProfileManager* instanceA = StreamProfileManager::get();
    const QString host = QStringLiteral("two-instance-host");
    QVERIFY(!instanceA->profiles(host).isEmpty());
    const QString profileId = instanceA->activeProfileId(host);

    // A second manager has its own startup snapshot while sharing the same
    // QSettings store and cross-process lock, matching two live app instances.
    StreamProfileManager instanceB;
    QVERIFY(instanceB.isReady());
    QVERIFY(instanceB.instanceNumber() != instanceA->instanceNumber());

    std::unique_ptr<StreamProfileEditor> editorA(
        instanceA->createEditor(host, profileId, false));
    editorA->settings()->fps = 90;
    emit editorA->settings()->displayModeChanged();
    QVERIFY(editorA->save());

    const QVariantMap external = instanceB.checkForExternalChanges();
    QVERIFY(external.value(QStringLiteral("changed")).toBool());
    QCOMPARE(external.value(QStringLiteral("instanceNumber")).toInt(),
             instanceA->instanceNumber());
    QVERIFY(instanceB.acknowledgeExternalChanges());

    // Ignore preserved B's old snapshot. Its later explicit save takes
    // ownership and writes the full local profile snapshot.
    std::unique_ptr<StreamProfileEditor> editorB(
        instanceB.createEditor(host, profileId, false));
    editorB->settings()->fps = 120;
    emit editorB->settings()->displayModeChanged();
    QVERIFY(editorB->save());
    QVERIFY(instanceA->checkForExternalChanges().value(QStringLiteral("changed")).toBool());

    QSettings shared;
    const QString fpsPath = QStringLiteral("streamProfiles/hosts/%1/profiles/%2/fps")
                                .arg(QString::fromLatin1(host.toUtf8().toHex()), profileId);
    shared.sync();
    QCOMPARE(shared.value(fpsPath).toInt(), 120);
}

void StreamProfileTests::settingsCoordinationReloadNotifiesAndResavesDeletedProfile()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    QVERIFY(manager->isReady());
    const QVariantMap initialReload = manager->reloadSnapshot(QString(), true);
    QVERIFY2(initialReload.value(QStringLiteral("error")).toString().isEmpty(),
             qPrintable(initialReload.value(QStringLiteral("error")).toString()));
    const QString host = QStringLiteral("reload-cache-host");
    QVERIFY(!manager->profiles(host).isEmpty());
    const QString profileId = manager->activeProfileId(host);
    QVERIFY(!profileId.isEmpty());

    std::unique_ptr<StreamProfileEditor> editor(
        manager->createEditor(host, profileId, false));
    QVERIFY(editor);
    editor->settings()->fps = 96;
    emit editor->settings()->displayModeChanged();

    QSignalSpy profileChanges(manager, &StreamProfileManager::profilesChanged);
    QSettings shared;
    shared.sync();
    const QString hostPath = QStringLiteral("streamProfiles/hosts/%1/")
                                 .arg(QString::fromLatin1(host.toUtf8().toHex()));
    QStringList sharedOrder = shared.value(hostPath + QStringLiteral("profileOrder")).toStringList();
    QVERIFY(sharedOrder.contains(profileId));
    sharedOrder.removeAll(profileId);
    shared.setValue(hostPath + QStringLiteral("profileOrder"), sharedOrder);
    shared.remove(hostPath + QStringLiteral("profiles/") + profileId);
    shared.setValue(QStringLiteral("streamProfiles/template/fps"), 77);
    QVERIFY(bumpExternalSettingsRevision(shared));

    const QVariantMap reload = manager->reloadSnapshot(host);
    QVERIFY2(reload.value(QStringLiteral("error")).toString().isEmpty(),
             qPrintable(reload.value(QStringLiteral("error")).toString()));
    QVERIFY(reload.value(QStringLiteral("templateSettingsChanged")).toBool());
    QVERIFY(reload.value(QStringLiteral("activeSettingsChanged")).toBool());
    QVERIFY(profileChanges.count() > 0);
    bool notifiedHost = false;
    for (const QList<QVariant>& arguments : profileChanges) {
        if (!arguments.isEmpty() && arguments.first().toString() == host) {
            notifiedHost = true;
            break;
        }
    }
    QVERIFY(notifiedHost);

    // Saving the still-open editor is an explicit user action. It restores
    // the deleted stable ID to the order instead of writing an orphan record.
    QVERIFY2(editor->save(), qPrintable(editor->lastError()));
    shared.sync();
    sharedOrder = shared.value(hostPath + QStringLiteral("profileOrder")).toStringList();
    QVERIFY(sharedOrder.contains(profileId));
    QVERIFY(shared.value(hostPath + QStringLiteral("profiles/") + profileId +
                         QStringLiteral("/fps")).isValid());
}

void StreamProfileTests::settingsCoordinationLockFailureDoesNotAdvanceRevision()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    QVERIFY(manager->isReady());
    QSettings shared;
    qulonglong revision = shared.value(QStringLiteral("settingsCoordination/revision"), 0).toULongLong();
    QVERIFY(bumpExternalSettingsRevision(shared));
    QVERIFY(manager->checkForExternalChanges().value(QStringLiteral("changed")).toBool());
    QVERIFY(manager->acknowledgeExternalChanges());

    const QString lockPath = QFileInfo(shared.fileName()).absolutePath() +
                             QStringLiteral("/moonlight-settings.lock");
    QLockFile heldLock(lockPath);
    heldLock.setStaleLockTime(30000);
    QVERIFY(heldLock.tryLock(100));
    StreamingPreferences preferences;
    preferences.reload();
    preferences.uiFramePacingMode = StreamingPreferences::UI_FRAME_PACING_UNBOUNDED;
    QVERIFY(!preferences.save());
    heldLock.unlock();

    shared.sync();
    QCOMPARE(shared.value(QStringLiteral("settingsCoordination/revision")).toULongLong(), revision + 1);
    QVERIFY(manager->localSettingsDirty());

    // The failed save already staged this value locally. An identical retry
    // must still publish the pending snapshot instead of treating it as a no-op.
    QVERIFY(preferences.save());
    shared.sync();
    QCOMPARE(shared.value(QStringLiteral("settingsCoordination/revision")).toULongLong(), revision + 2);
    QVERIFY(!manager->localSettingsDirty());
}

void StreamProfileTests::uiFramePacingPreference()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    QSettings settings;
    settings.setValue(
        QStringLiteral("uiframepacingmode"),
        static_cast<int>(StreamingPreferences::UI_FRAME_PACING_144));
    settings.sync();
    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));

    StreamingPreferences preferences;
    preferences.reload();
    QCOMPARE(preferences.uiFramePacingMode,
             StreamingPreferences::UI_FRAME_PACING_144);

    preferences.uiFramePacingMode =
        StreamingPreferences::UI_FRAME_PACING_UNBOUNDED;
    preferences.save();
    settings.sync();
    QCOMPARE(
        settings.value(QStringLiteral("uiframepacingmode")).toInt(),
        static_cast<int>(StreamingPreferences::UI_FRAME_PACING_UNBOUNDED));

    settings.setValue(QStringLiteral("uiframepacingmode"), 999);
    settings.sync();
    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));
    preferences.reload();
    QCOMPARE(preferences.uiFramePacingMode,
             StreamingPreferences::UI_FRAME_PACING_DISABLED);
}

void StreamProfileTests::uiProfileControlsPreference()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    QSettings settings;
    settings.remove(QStringLiteral("showpcprofilecontrols"));
    settings.sync();
    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));

    StreamingPreferences preferences;
    preferences.reload();
    QVERIFY(preferences.showPcProfileControls);

    preferences.showPcProfileControls = false;
    preferences.save();
    settings.sync();
    QCOMPARE(
        settings.value(QStringLiteral("showpcprofilecontrols")).toBool(),
        false);

    StreamingPreferences reloaded;
    reloaded.reload();
    QVERIFY(!reloaded.showPcProfileControls);
}

void StreamProfileTests::uiGraphicsPreference()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    QSettings settings;
    settings.remove(QStringLiteral("uigraphicsbackend"));
    settings.sync();
    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));

    StreamingPreferences defaults;
    defaults.reload();
    QCOMPARE(defaults.uiGraphicsBackend,
             StreamingPreferences::UI_GRAPHICS_AUTOMATIC);

    settings.setValue(
        QStringLiteral("uigraphicsbackend"),
        static_cast<int>(StreamingPreferences::UI_GRAPHICS_OPENGL));
    settings.setValue(
        QStringLiteral("uid3d11swapchainmode"),
        static_cast<int>(StreamingPreferences::UI_D3D11_LEGACY));
    settings.sync();
    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));

    StreamingPreferences preferences;
    preferences.reload();
    QCOMPARE(preferences.uiGraphicsBackend,
             StreamingPreferences::UI_GRAPHICS_OPENGL);
    QCOMPARE(preferences.uiD3D11SwapchainMode,
             StreamingPreferences::UI_D3D11_LEGACY);

    preferences.uiGraphicsBackend =
        StreamingPreferences::UI_GRAPHICS_D3D11;
    preferences.uiD3D11SwapchainMode =
        StreamingPreferences::UI_D3D11_FLIP;
    preferences.save();
    settings.sync();
    QCOMPARE(
        settings.value(QStringLiteral("uigraphicsbackend")).toInt(),
        static_cast<int>(StreamingPreferences::UI_GRAPHICS_D3D11));
    QCOMPARE(
        settings.value(QStringLiteral("uid3d11swapchainmode")).toInt(),
        static_cast<int>(StreamingPreferences::UI_D3D11_FLIP));

    settings.setValue(QStringLiteral("uigraphicsbackend"), 999);
    settings.setValue(QStringLiteral("uid3d11swapchainmode"), 999);
    settings.sync();
    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));
    preferences.reload();
    QCOMPARE(preferences.uiGraphicsBackend,
             StreamingPreferences::UI_GRAPHICS_AUTOMATIC);
    QCOMPARE(preferences.uiD3D11SwapchainMode,
             StreamingPreferences::UI_D3D11_FLIP);
}

void StreamProfileTests::serializationAndCorruptData()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    const QString host = QStringLiteral("serialization-host");

    std::unique_ptr<StreamProfileEditor> editor(
        manager->createEditor(host, manager->activeProfileId(host), false));
    StreamingPreferences* preferences = editor->settings();
    preferences->width = 3840;
    preferences->height = 2160;
    preferences->fps = 144;
    preferences->bitrateKbps = 120000;
    preferences->unlockBitrate = true;
    preferences->autoAdjustBitrate = false;
    preferences->enableVsync = false;
    preferences->gameOptimizations = false;
    preferences->playAudioOnHost = true;
    preferences->multiController = false;
    preferences->quitAppAfter = true;
    preferences->absoluteMouseMode = true;
    preferences->absoluteTouchMode = false;
    preferences->framePacing = true;
    preferences->enableVrr = true;
    preferences->vrrSmoothness = true;
    preferences->connectionWarnings = false;
    preferences->configurationWarnings = false;
    preferences->gamepadMouse = false;
    preferences->showPerformanceOverlay = true;
    preferences->packetSize = 1392;
    preferences->swapMouseButtons = true;
    preferences->muteOnFocusLoss = true;
    preferences->backgroundGamepad = true;
    preferences->reverseScrollDirection = true;
    preferences->swapFaceButtons = true;
    preferences->keepAwake = false;
    preferences->audioConfig = StreamingPreferences::AC_71_SURROUND;
    preferences->videoCodecConfig = StreamingPreferences::VCC_FORCE_AV1;
    preferences->enableHdr = true;
    preferences->enableYUV444 = true;
    preferences->videoDecoderSelection = StreamingPreferences::VDS_FORCE_HARDWARE;
    preferences->windowMode = StreamingPreferences::WM_WINDOWED;
    preferences->captureSysKeysMode = StreamingPreferences::CSK_ALWAYS;
    emit preferences->displayModeChanged();
    QVERIFY(editor->save());

    std::unique_ptr<StreamingPreferences> roundTrip(
        manager->createActiveSettings(host));
    QVERIFY(preferences->streamSettingsEqual(*roundTrip));
    QVERIFY(roundTrip->vrrSmoothness);

    QSettings settings;
    const QString hostKey = QString::fromLatin1(host.toUtf8().toHex());
    const QString profileRoot = QStringLiteral("streamProfiles/hosts/%1/profiles/%2")
        .arg(hostKey, editor->profileId());
    settings.setValue(profileRoot + QStringLiteral("/width"), -1);
    settings.setValue(QStringLiteral("streamProfiles/hosts/%1/activeProfile").arg(hostKey),
                      QStringLiteral("missing-profile"));
    settings.sync();

    QVERIFY(bumpExternalSettingsRevision(settings));
    QVERIFY(reloadTestSnapshot(manager));

    QCOMPARE(manager->activeProfileId(host), editor->profileId());
    std::unique_ptr<StreamingPreferences> recovered(
        manager->createActiveSettings(host));
    QCOMPARE(recovered->width, 1280);
    QCOMPARE(recovered->height, 720);
    QCOMPARE(recovered->videoCodecConfig, StreamingPreferences::VCC_AUTO);
}

void StreamProfileTests::cliOverridesAreTemporary()
{
    StreamProfileManager* manager = StreamProfileManager::get();
    const QString host = QStringLiteral("cli-host");
    std::unique_ptr<StreamProfileEditor> editor(
        manager->createEditor(host, manager->activeProfileId(host), false));
    editor->settings()->width = 2560;
    editor->settings()->height = 1440;
    editor->settings()->fps = 60;
    emit editor->settings()->displayModeChanged();
    QVERIFY(editor->save());

    std::unique_ptr<StreamingPreferences> session(
        manager->createActiveSettings(host));
    StreamCommandLineParser parser;
    parser.parse({QStringLiteral("moonlight"), QStringLiteral("stream"),
                  QStringLiteral("cli-host"), QStringLiteral("Desktop"),
                  QStringLiteral("--resolution"), QStringLiteral("1920x1080"),
                  QStringLiteral("--fps"), QStringLiteral("120"),
                  QStringLiteral("--no-vsync")},
                 session.get());

    QCOMPARE(session->width, 1920);
    QCOMPARE(session->height, 1080);
    QCOMPARE(session->fps, 120);
    QVERIFY(!session->enableVsync);

    std::unique_ptr<StreamingPreferences> persisted(
        manager->createActiveSettings(host));
    QCOMPARE(persisted->width, 2560);
    QCOMPARE(persisted->height, 1440);
    QCOMPARE(persisted->fps, 60);
    QVERIFY(persisted->enableVsync);
}

QTEST_GUILESS_MAIN(StreamProfileTests)
#include "test_streamprofiles.moc"
