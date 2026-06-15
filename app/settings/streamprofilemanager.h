#pragma once

#include "streamingpreferences.h"

#include <QMutex>
#include <QObject>
#include <QVariantList>

class StreamProfileManager;

class StreamProfileEditor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(StreamingPreferences* settings READ settings CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString hostUuid READ hostUuid CONSTANT)
    Q_PROPERTY(QString profileId READ profileId NOTIFY profileIdChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
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
    bool isNewProfile() const { return m_NewProfile; }
    bool templateMode() const { return m_TemplateMode; }
    bool canDelete() const;

    Q_INVOKABLE bool save();
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

public:
    static StreamProfileManager* get();

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

private:
    friend class StreamProfileEditor;

    explicit StreamProfileManager(QObject* parent = nullptr);

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
    bool saveEditor(StreamProfileEditor* editor);
    bool copyEditor(StreamProfileEditor* editor);
    bool deleteEditor(StreamProfileEditor* editor);
    bool setEditorAsDefault(StreamProfileEditor* editor);
    bool switchEditorProfile(StreamProfileEditor* editor,
                             const QString& profileId);
    bool beginEditorProfile(StreamProfileEditor* editor);
    int profileCount(const QString& hostUuid);

    QMutex m_Mutex;
};
