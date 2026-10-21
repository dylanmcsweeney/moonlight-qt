#pragma once

#include <QObject>
#include <QPointer>
#include <QElapsedTimer>

#include <atomic>

class QQuickWindow;
class StreamingPreferences;
class UiStartupRecovery;
class QTimer;

class UiRendererManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool rendererReady READ rendererReady NOTIFY rendererChanged)
    Q_PROPERTY(QString actualBackend READ actualBackend NOTIFY rendererChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY rendererChanged)
    Q_PROPERTY(bool softwareRenderer READ softwareRenderer NOTIFY rendererChanged)
    Q_PROPERTY(int savedBackend READ savedBackend NOTIFY rendererChanged)
    Q_PROPERTY(QString recoveryError READ recoveryError NOTIFY recoveryErrorChanged)

public:
    explicit UiRendererManager(StreamingPreferences* preferences,
                               UiStartupRecovery* startupRecovery,
                               QObject* parent = nullptr);

    static void configureGraphicsBackend(
        const StreamingPreferences* preferences);

    void setWindow(QQuickWindow* window);

    bool rendererReady() const;
    QString actualBackend() const;
    QString adapterName() const;
    bool softwareRenderer() const;
    int savedBackend() const;
    QString recoveryError() const;

    Q_INVOKABLE bool restartApplication();
    Q_INVOKABLE bool restartWithGraphicsConfiguration(
        int backend, int d3d11SwapchainMode);
    Q_INVOKABLE bool keepUsingD3D11();
    Q_INVOKABLE bool retryPreferredBackend();

signals:
    void rendererChanged();
    void softwareD3D12Detected(const QString& adapterName);
    void recoveryErrorChanged();

private:
    void inspectRenderer(QQuickWindow* window);
    void updateRenderer(QString backend, QString adapterName,
                        bool softwareRenderer, bool isD3D12);

    StreamingPreferences* m_Preferences;
    UiStartupRecovery* m_StartupRecovery;
    QPointer<QQuickWindow> m_Window;
    std::atomic_bool m_InspectionComplete;
    bool m_RendererReady;
    QString m_ActualBackend;
    QString m_AdapterName;
    bool m_SoftwareRenderer;
    bool m_HasPresentedFrame;
    QString m_RecoveryError;
    QElapsedTimer m_LastUiHeartbeat;
    QElapsedTimer m_ResponsiveSince;
    QTimer* m_StartupHeartbeatTimer;
};
