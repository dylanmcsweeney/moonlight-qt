#pragma once

#include "streamingpreferences.h"

#include <QMutex>
#include <QObject>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

class StreamProfileManager;

class StreamProfileEditor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(StreamingPreferences* settings READ settings CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString hostUuid READ hostUuid CONSTANT)
    Q_PROPERTY(QString profileId READ profileId NOTIFY profileIdChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool newProfile READ isNewProfile NOTIFY newProfileChanged)
    Q_PROPERTY(bool templateMode READ templateMode CONSTANT)
    Q_PROPERTY(bool canDelete READ canDelete NOTIFY canDeleteChanged)

public:
    StreamingPreferences* settings() const { return m_Settings; }
    QString name() const { return m_Name; }
    void setName(const QString& name);
    QString hostUuid() const { return m_HostUuid; }
    QString profileId() const { return m_ProfileId; }
    bool dirty() const { return m_Dirty; }
    QString lastError() const;
    bool isNewProfile() const { return m_NewProfile; }
    bool templateMode() const { return m_TemplateMode; }
    bool canDelete() const;

    Q_INVOKABLE bool save(bool overrideExternal = false);
    Q_INVOKABLE bool copy();
    Q_INVOKABLE bool remove();
    Q_INVOKABLE bool setAsDefault();
    Q_INVOKABLE void resetToStock();
    Q_INVOKABLE void discardChanges();
    Q_INVOKABLE bool switchToProfile(const QString& profileId);
    Q_INVOKABLE bool beginNewProfile();

signals:
    void nameChanged();
    void profileIdChanged();
    void dirtyChanged();
    void newProfileChanged();
    void canDeleteChanged();
    void lastErrorChanged();
    void saved();
    void removed();

private:
    friend class StreamProfileManager;

    StreamProfileEditor(StreamProfileManager* manager,
                        const QString& hostUuid,
                        const QString& profileId,
                        const QString& name,
                        bool newProfile,
                        bool templateMode,
                        QObject* parent = nullptr);

    void updateDirty();
    void acceptCurrentState();

    StreamProfileManager* m_Manager;
    StreamingPreferences* m_Settings;
    StreamingPreferences* m_OriginalSettings;
    QString m_HostUuid;
    QString m_ProfileId;
    QString m_Name;
    QString m_OriginalName;
    bool m_Dirty;
    bool m_NewProfile;
    bool m_TemplateMode;
};

class StreamProfileManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int instanceNumber READ instanceNumber CONSTANT)
    Q_PROPERTY(qulonglong sharedRevision READ sharedRevision NOTIFY sharedRevisionChanged)
    Q_PROPERTY(bool localSettingsDirty READ localSettingsDirty NOTIFY localSettingsDirtyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    static StreamProfileManager* get();

    int instanceNumber() const { return m_InstanceNumber; }
    qulonglong sharedRevision() const { return m_AcknowledgedRevision; }
    bool localSettingsDirty() const { return m_LocalSettingsDirty; }
    QString lastError() const { return m_LastError; }
    bool isReady() const { return m_SnapshotReady; }
    Q_INVOKABLE QVariantMap checkForExternalChanges();
    Q_INVOKABLE bool acknowledgeExternalChanges();
    Q_INVOKABLE QVariantMap reloadSnapshot(const QString& hostUuid = QString(),
                                           bool discardLocalChanges = false);
    Q_INVOKABLE bool discardLocalChanges();
    Q_INVOKABLE bool saveLocalSnapshot();
    Q_INVOKABLE bool overrideExternalChanges();
    bool loadApplicationPreferences(StreamingPreferences& preferences);
    bool saveApplicationPreferences(const StreamingPreferences& preferences);

    Q_INVOKABLE QVariantList profiles(const QString& hostUuid);
    Q_INVOKABLE QString activeProfileId(const QString& hostUuid);
    Q_INVOKABLE QString activeProfileName(const QString& hostUuid);
    Q_INVOKABLE bool activateProfile(const QString& hostUuid, const QString& profileId);
    Q_INVOKABLE StreamProfileEditor* createEditor(const QString& hostUuid,
                                                  const QString& profileId = QString(),
                                                  bool newProfile = false);
    Q_INVOKABLE StreamProfileEditor* createTemplateEditor();

    void ensureHost(const QString& hostUuid);
    void ensureExistingHost(const QString& hostUuid);
    void removeHost(const QString& hostUuid);
    StreamingPreferences* createActiveSettings(const QString& hostUuid, QObject* parent = nullptr);

signals:
    void profilesChanged(QString hostUuid);
    void sharedRevisionChanged();
    void localSettingsDirtyChanged();
    void lastErrorChanged();

private:
    friend class StreamProfileEditor;
    friend class StreamProfileTests;

    explicit StreamProfileManager(QObject* parent = nullptr);
    ~StreamProfileManager() override;

    QString hostKey(const QString& hostUuid) const;
    QString createProfileId() const;
    void ensureHost(const QString& hostUuid, bool useStockDefaults);
    void initializeTemplate();
    void loadTemplate(StreamingPreferences& preferences);
    void saveTemplate(const StreamingPreferences& preferences);
    void loadProfile(const QString& hostUuid, const QString& profileId,
                     QString& name, StreamingPreferences& preferences);
    void saveProfile(const QString& hostUuid, const QString& profileId,
                     const QString& name, const StreamingPreferences& preferences);
    bool saveEditor(StreamProfileEditor* editor, bool overrideExternal);
    bool copyEditor(StreamProfileEditor* editor);
    bool deleteEditor(StreamProfileEditor* editor);
    bool setEditorAsDefault(StreamProfileEditor* editor);
    bool switchEditorProfile(StreamProfileEditor* editor,
                             const QString& profileId);
    bool beginEditorProfile(StreamProfileEditor* editor);
    int profileCount(const QString& hostUuid);
    bool publishSnapshot(bool force = false);
    bool initializeSnapshot();
    void registerInstance();
    void unregisterInstance();
    void writeInstanceStatus();
    void updateInstanceStatus();
    QString sharedStateDirectory() const;
    QString lockFilePath() const;
    bool loadSharedIntoSnapshot(qulonglong* revision = nullptr);
    void setLastError(const QString& message);

    QMutex m_Mutex;
    QTemporaryDir m_SnapshotDirectory;
    QString m_SnapshotPath;
    QString m_InstanceId;
    int m_InstanceNumber = 1;
    qulonglong m_SnapshotRevision = 0;
    qulonglong m_AcknowledgedRevision = 0;
    qulonglong m_LastObservedRevision = 0;
    bool m_LocalSettingsDirty = false;
    bool m_SnapshotReady = false;
    QString m_LastError;
};
