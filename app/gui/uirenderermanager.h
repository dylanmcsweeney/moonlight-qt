#pragma once

#include <QObject>
#include <QPointer>

#include <atomic>

class QQuickWindow;
class StreamingPreferences;

class UiRendererManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool rendererReady READ rendererReady NOTIFY rendererChanged)
    Q_PROPERTY(QString actualBackend READ actualBackend NOTIFY rendererChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY rendererChanged)
    Q_PROPERTY(bool softwareRenderer READ softwareRenderer NOTIFY rendererChanged)

public:
    explicit UiRendererManager(StreamingPreferences* preferences,
                               QObject* parent = nullptr);

    static void configureGraphicsBackend(
        const StreamingPreferences* preferences);

    void setWindow(QQuickWindow* window);

    bool rendererReady() const;
    QString actualBackend() const;
    QString adapterName() const;
    bool softwareRenderer() const;

    Q_INVOKABLE bool restartApplication();
    Q_INVOKABLE bool restartWithGraphicsConfiguration(
        int backend, int d3d11SwapchainMode);

signals:
    void rendererChanged();
    void softwareD3D12Detected(const QString& adapterName);

private:
    void inspectRenderer(QQuickWindow* window);
    void updateRenderer(QString backend, QString adapterName,
                        bool softwareRenderer, bool isD3D12);

    StreamingPreferences* m_Preferences;
    QPointer<QQuickWindow> m_Window;
    std::atomic_bool m_InspectionComplete;
    bool m_RendererReady;
    QString m_ActualBackend;
    QString m_AdapterName;
    bool m_SoftwareRenderer;
};
