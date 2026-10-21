#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <QUuid>
#include <memory>

class QLockFile;

// Crash recovery policy for the Qt Quick UI renderer. The backend values are
// kept in sync with StreamingPreferences::UIGraphicsBackend (0 through 3).
// State lives next to the QSettings directory, in its own directory, and is
// never written into QSettings or the stream profile store.
class UiStartupRecovery : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int effectiveBackend READ effectiveBackend NOTIFY stateChanged)
    Q_PROPERTY(bool recoveryActive READ recoveryActive NOTIFY stateChanged)
    Q_PROPERTY(bool safeUiActive READ safeUiActive CONSTANT)
    Q_PROPERTY(QString warning READ warning NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)

public:
    // Exposed for the translation unit's JSON encoding helpers; not part of
    // the runtime integration contract.
    struct State;

    enum Backend {
        Automatic = 0,
        Direct3D12 = 1,
        Direct3D11 = 2,
        OpenGL = 3
    };
    Q_ENUM(Backend)

    // explicitEnvironmentBackend is -1 when no QSG_RHI_BACKEND override was
    // supplied, or one of the Backend values when it was explicitly selected.
    explicit UiStartupRecovery(const QString& settingsDirectory,
                               int requestedBackend,
                               bool graphical,
                               bool safeUi = false,
                               int explicitEnvironmentBackend = -1,
                               bool explicitRetry = false,
                               QObject* parent = nullptr);
    ~UiStartupRecovery() override;

    int effectiveBackend() const;
    bool recoveryActive() const;
    bool safeUiActive() const;
    QString warning() const;
    QString statusMessage() const;
    QString attemptId() const;

    // Call as startup advances. The elapsed duration and stage are logged and
    // saved in the journal for diagnosing a later abandoned attempt.
    Q_INVOKABLE void stage(const QString& name);

    // Replace the requested backend in this attempt with the renderer API
    // that Qt actually selected (notably when the saved preference is Auto).
    Q_INVOKABLE void recordActualBackend(int backend);

    // A launch is healthy only after the UI reports real rendered frames and
    // a responsive grace period. Calling this removes the pending attempt but
    // deliberately leaves a recovery latch in place.
    Q_INVOKABLE void markHealthy();

    // Use for a normal early exit before the healthy point. It unregisters
    // this attempt so a deliberate shutdown is not treated as a crash.
    Q_INVOKABLE void finishCleanly();

    // Explicitly retry the saved backend. A crash during the retry is detected
    // from the pending attempt and re-latches recovery on the next launch.
    Q_INVOKABLE bool retryPreferredBackend();

    // Call after a user saves Direct3D 11 as their preference.
    Q_INVOKABLE bool clearRecoveryForSavedBackend(int backend);

    // Restore the latch if the process could not be restarted after an
    // explicit retry was requested.
    Q_INVOKABLE bool relatchRecovery();

signals:
    void stateChanged();

private:
    bool initializeJournal(bool graphical);
    bool loadState(State& state, QString* error = nullptr) const;
    bool saveState(const State& state);
    void failSafe(const QString& message);
    void updateCurrentAttempt(const QString& stageName);

    QString m_SettingsDirectory;
    QString m_StateDirectory;
    QString m_StatePath;
    QString m_GlobalLockPath;
    QString m_AttemptId;
    QString m_Warning;
    QString m_StatusMessage;
    int m_RequestedBackend;
    int m_EnvironmentBackend;
    int m_EffectiveBackend;
    int m_AttemptBackend;
    bool m_Graphical;
    bool m_SafeUiActive;
    bool m_ExplicitRetry;
    bool m_RecoveryActive;
    bool m_Initialized;
    bool m_AttemptFinished;
    QElapsedTimer m_Elapsed;
    std::unique_ptr<QLockFile> m_AttemptLock;
};
