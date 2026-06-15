#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#endif

class FrameMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal measuredFps READ measuredFps NOTIFY measuredFpsChanged)
    Q_PROPERTY(qreal p95FrameTimeMs READ p95FrameTimeMs NOTIFY frameTimesChanged)
    Q_PROPERTY(qreal worstFrameTimeMs READ worstFrameTimeMs
               NOTIFY frameTimesChanged)
    Q_PROPERTY(bool continuous READ continuous WRITE setContinuous
               NOTIFY continuousChanged)

public:
    explicit FrameMonitor(QObject* parent = nullptr)
        : QObject(parent),
          m_Window(nullptr),
          m_FrameCount(0),
          m_MeasuredFps(0.0),
          m_P95FrameTimeMs(0.0),
          m_WorstFrameTimeMs(0.0),
          m_MeasurementStartNs(0),
          m_LastFrameNs(0),
          m_RendererReported(false),
          m_Continuous(false)
    {
        m_ReportTimer.setInterval(1000);
        connect(&m_ReportTimer, &QTimer::timeout,
                this, &FrameMonitor::reportMeasurement);
    }

    void setWindow(QQuickWindow* window)
    {
        m_Window = window;
        connect(m_Window, &QQuickWindow::frameSwapped, this, [this]() {
            const qint64 nowNs = m_Clock.nsecsElapsed();
            if (m_LastFrameNs != 0) {
                m_FrameIntervalsNs.append(nowNs - m_LastFrameNs);
            }
            m_LastFrameNs = nowNs;
            ++m_FrameCount;

            if (m_Continuous) {
                m_Window->update();
            }
        });
        connect(m_Window, &QQuickWindow::beforeRendering, this,
                &FrameMonitor::reportRenderer, Qt::DirectConnection);

        m_Clock.start();
        m_MeasurementStartNs = m_Clock.nsecsElapsed();
        m_ReportTimer.start();
        if (m_Continuous) {
            m_Window->update();
        }
    }

    qreal measuredFps() const
    {
        return m_MeasuredFps;
    }

    qreal p95FrameTimeMs() const
    {
        return m_P95FrameTimeMs;
    }

    qreal worstFrameTimeMs() const
    {
        return m_WorstFrameTimeMs;
    }

    bool continuous() const
    {
        return m_Continuous;
    }

    void setContinuous(bool continuous)
    {
        if (m_Continuous == continuous) {
            return;
        }

        m_Continuous = continuous;
        m_FrameCount = 0;
        m_FrameIntervalsNs.clear();
        m_MeasurementStartNs = m_Clock.nsecsElapsed();
        m_LastFrameNs = 0;
        emit continuousChanged();

        if (m_Continuous && m_Window) {
            m_Window->update();
        }
    }

    Q_INVOKABLE void resetMeasurements()
    {
        m_FrameCount = 0;
        m_FrameIntervalsNs.clear();
        m_MeasurementStartNs = m_Clock.nsecsElapsed();
        m_LastFrameNs = 0;
    }

signals:
    void measuredFpsChanged();
    void frameTimesChanged();
    void continuousChanged();

private slots:
    void reportRenderer()
    {
        if (m_RendererReported) {
            return;
        }
        m_RendererReported = true;

        QSGRendererInterface* renderer = m_Window->rendererInterface();
        const QSGRendererInterface::GraphicsApi api = renderer->graphicsApi();

#ifdef Q_OS_WIN
        IDXGIAdapter1* adapter = nullptr;
        if (api == QSGRendererInterface::Direct3D11) {
            auto* device = static_cast<ID3D11Device*>(renderer->getResource(
                m_Window, QSGRendererInterface::DeviceResource));
            IDXGIDevice* dxgiDevice = nullptr;
            if (device && SUCCEEDED(device->QueryInterface(
                    __uuidof(IDXGIDevice),
                    reinterpret_cast<void**>(&dxgiDevice)))) {
                IDXGIAdapter* baseAdapter = nullptr;
                if (SUCCEEDED(dxgiDevice->GetAdapter(&baseAdapter))) {
                    baseAdapter->QueryInterface(
                        __uuidof(IDXGIAdapter1),
                        reinterpret_cast<void**>(&adapter));
                    baseAdapter->Release();
                }
                dxgiDevice->Release();
            }
        }
        else if (api == QSGRendererInterface::Direct3D12) {
            auto* device = static_cast<ID3D12Device*>(renderer->getResource(
                m_Window, QSGRendererInterface::DeviceResource));
            IDXGIFactory1* factory = nullptr;
            if (device && SUCCEEDED(CreateDXGIFactory1(
                    __uuidof(IDXGIFactory1),
                    reinterpret_cast<void**>(&factory)))) {
                const LUID deviceLuid = device->GetAdapterLuid();
                for (UINT index = 0; ; ++index) {
                    IDXGIAdapter1* candidate = nullptr;
                    if (factory->EnumAdapters1(index, &candidate) ==
                            DXGI_ERROR_NOT_FOUND) {
                        break;
                    }

                    DXGI_ADAPTER_DESC1 description = {};
                    candidate->GetDesc1(&description);
                    if (description.AdapterLuid.HighPart ==
                                deviceLuid.HighPart &&
                            description.AdapterLuid.LowPart ==
                                deviceLuid.LowPart) {
                        adapter = candidate;
                        break;
                    }
                    candidate->Release();
                }
                factory->Release();
            }
        }

        if (adapter) {
            DXGI_ADAPTER_DESC1 description = {};
            adapter->GetDesc1(&description);
            const QByteArray adapterName =
                QString::fromWCharArray(description.Description).toUtf8();
            fprintf(stderr, "Renderer adapter: %s software: %s\n",
                    adapterName.constData(),
                    (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ?
                        "true" : "false");
            fflush(stderr);
            adapter->Release();
            return;
        }
#endif

        fprintf(stderr, "Renderer graphics API: %d\n",
                static_cast<int>(api));
        fflush(stderr);
    }

    void reportMeasurement()
    {
        const qint64 nowNs = m_Clock.nsecsElapsed();
        const qint64 elapsedNs = nowNs - m_MeasurementStartNs;
        const qreal measuredFps =
            elapsedNs > 0 ?
                static_cast<qreal>(m_FrameCount) * 1000000000.0 / elapsedNs :
                0.0;

        qreal p95FrameTimeMs = 0.0;
        qreal worstFrameTimeMs = 0.0;
        if (!m_FrameIntervalsNs.isEmpty()) {
            std::sort(m_FrameIntervalsNs.begin(), m_FrameIntervalsNs.end());
            const qsizetype p95Index = qBound<qsizetype>(
                0,
                static_cast<qsizetype>(
                    std::ceil(m_FrameIntervalsNs.size() * 0.95)) - 1,
                m_FrameIntervalsNs.size() - 1);
            p95FrameTimeMs =
                static_cast<qreal>(m_FrameIntervalsNs.at(p95Index)) / 1000000.0;
            worstFrameTimeMs =
                static_cast<qreal>(m_FrameIntervalsNs.constLast()) / 1000000.0;
        }

        m_FrameCount = 0;
        m_FrameIntervalsNs.clear();
        m_MeasurementStartNs = nowNs;

        if (!qFuzzyCompare(m_MeasuredFps + 1.0, measuredFps + 1.0)) {
            m_MeasuredFps = measuredFps;
            emit measuredFpsChanged();
        }

        if (!qFuzzyCompare(m_P95FrameTimeMs + 1.0,
                           p95FrameTimeMs + 1.0) ||
                !qFuzzyCompare(m_WorstFrameTimeMs + 1.0,
                               worstFrameTimeMs + 1.0)) {
            m_P95FrameTimeMs = p95FrameTimeMs;
            m_WorstFrameTimeMs = worstFrameTimeMs;
            emit frameTimesChanged();
        }
    }

private:
    QQuickWindow* m_Window;
    QTimer m_ReportTimer;
    QElapsedTimer m_Clock;
    QVector<qint64> m_FrameIntervalsNs;
    quint64 m_FrameCount;
    qreal m_MeasuredFps;
    qreal m_P95FrameTimeMs;
    qreal m_WorstFrameTimeMs;
    qint64 m_MeasurementStartNs;
    qint64 m_LastFrameNs;
    bool m_RendererReported;
    bool m_Continuous;
};

static void configureRenderLoop(const QString& renderLoop)
{
    if (renderLoop != QStringLiteral("default")) {
        qputenv("QSG_RENDER_LOOP", renderLoop.toUtf8());
    }
}

static void configureGraphicsApi(const QString& graphicsApi)
{
    if (graphicsApi == QStringLiteral("software")) {
        qputenv("QT_QUICK_BACKEND", "software");
    }
    else if (graphicsApi != QStringLiteral("default")) {
        qputenv("QSG_RHI_BACKEND", graphicsApi.toUtf8());
    }
}

static void disableVBlankVirtualization()
{
#ifdef Q_OS_WIN
    auto disableVBlankVirtualization =
        reinterpret_cast<HRESULT (WINAPI*)()>(
            GetProcAddress(GetModuleHandleW(L"dxgi.dll"),
                           "DXGIDisableVBlankVirtualization"));
    if (disableVBlankVirtualization) {
        const HRESULT result = disableVBlankVirtualization();
        qInfo("DXGIDisableVBlankVirtualization() returned 0x%08lx",
              static_cast<unsigned long>(result));
    }
    else {
        qWarning("DXGIDisableVBlankVirtualization() is unavailable");
    }
#endif
}

static QString earlyOptionValue(int argc,
                                char* argv[],
                                const QString& optionName,
                                const QString& defaultValue)
{
    const QString optionPrefix = optionName + QLatin1Char('=');
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument.startsWith(optionPrefix)) {
            return argument.mid(optionPrefix.size());
        }
        if (argument == optionName && i + 1 < argc) {
            return QString::fromLocal8Bit(argv[i + 1]);
        }
    }

    return defaultValue;
}

static bool earlyOptionIsSet(int argc,
                             char* argv[],
                             const QString& optionName)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == optionName) {
            return true;
        }
    }

    return false;
}

int main(int argc, char* argv[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Moonlight Qt Quick GUI performance diagnostic"));
    parser.addHelpOption();

    const QCommandLineOption renderLoopOption(
        QStringLiteral("render-loop"),
        QStringLiteral("Qt Quick render loop: default, basic, or threaded."),
        QStringLiteral("loop"),
        QStringLiteral("default"));
    const QCommandLineOption graphicsApiOption(
        QStringLiteral("graphics-api"),
        QStringLiteral(
            "Graphics API: default, d3d11, d3d12, opengl, vulkan, or software."),
        QStringLiteral("api"),
        QStringLiteral("default"));
    const QCommandLineOption stageOption(
        QStringLiteral("stage"),
        QStringLiteral(
            "Initial stage: solid, material, shell, grid, or settings."),
        QStringLiteral("stage"),
        QStringLiteral("solid"));
    const QCommandLineOption continuousOption(
        QStringLiteral("continuous"),
        QStringLiteral("Continuously request frames."));
    const QCommandLineOption benchmarkOption(
        QStringLiteral("benchmark"),
        QStringLiteral("Enable the deterministic animation benchmark."));
    const QCommandLineOption d3dLegacySwapchainOption(
        QStringLiteral("d3d-legacy-swapchain"),
        QStringLiteral(
            "Use Qt's legacy D3D discard swapchain instead of flip-discard."));
    const QCommandLineOption d3dMaxFrameLatencyOption(
        QStringLiteral("d3d-max-frame-latency"),
        QStringLiteral("Set Qt's D3D maximum frame latency."),
        QStringLiteral("frames"));
    const QCommandLineOption disableVBlankOption(
        QStringLiteral("disable-vblank-virtualization"),
        QStringLiteral(
            "Call DXGIDisableVBlankVirtualization() before creating a window."));
    const QCommandLineOption disableRedirectionSurfaceOption(
        QStringLiteral("disable-redirection-surface"),
        QStringLiteral(
            "Disable the Windows DWM redirection bitmap (D3D only)."));
    const QCommandLineOption alphaBufferOption(
        QStringLiteral("alpha-buffer"),
        QStringLiteral(
            "Request an alpha-capable window surface (uses DirectComposition "
            "for D3D)."));
    const QCommandLineOption smokeTestOption(
        QStringLiteral("smoke-test"),
        QStringLiteral("Exit automatically after loading the selected stage."));

    parser.addOption(renderLoopOption);
    parser.addOption(graphicsApiOption);
    parser.addOption(stageOption);
    parser.addOption(continuousOption);
    parser.addOption(benchmarkOption);
    parser.addOption(d3dLegacySwapchainOption);
    parser.addOption(d3dMaxFrameLatencyOption);
    parser.addOption(disableVBlankOption);
    parser.addOption(disableRedirectionSurfaceOption);
    parser.addOption(alphaBufferOption);
    parser.addOption(smokeTestOption);

    // Read these two options directly because Qt chooses the render loop and
    // graphics backend while constructing QGuiApplication.
    const QString renderLoop = earlyOptionValue(
        argc, argv, QStringLiteral("--render-loop"), QStringLiteral("default"))
                                   .toLower();
    const QString graphicsApi = earlyOptionValue(
        argc, argv, QStringLiteral("--graphics-api"), QStringLiteral("default"))
                                    .toLower();
    const QString d3dMaxFrameLatency = earlyOptionValue(
        argc, argv, QStringLiteral("--d3d-max-frame-latency"), QString());
    configureRenderLoop(renderLoop);
    configureGraphicsApi(graphicsApi);
    if (earlyOptionIsSet(
            argc, argv, QStringLiteral("--d3d-legacy-swapchain"))) {
        qputenv("QT_D3D_NO_FLIP", "1");
    }
    if (!d3dMaxFrameLatency.isEmpty()) {
        qputenv("QT_D3D_MAX_FRAME_LATENCY", d3dMaxFrameLatency.toUtf8());
    }
    if (earlyOptionIsSet(
            argc, argv, QStringLiteral("--disable-redirection-surface"))) {
        qputenv("QT_QPA_DISABLE_REDIRECTION_SURFACE", "1");
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(
        QStringLiteral("Moonlight GUI Performance Diagnostic"));
    parser.process(app);

    if (parser.isSet(disableVBlankOption)) {
        disableVBlankVirtualization();
    }

    QQuickStyle::setStyle(QStringLiteral("Material"));
    if (parser.isSet(alphaBufferOption)) {
        QQuickWindow::setDefaultAlphaBuffer(true);
    }

    const QStringList stages = {
        QStringLiteral("solid"),
        QStringLiteral("material"),
        QStringLiteral("shell"),
        QStringLiteral("grid"),
        QStringLiteral("settings")
    };
    int initialStage = stages.indexOf(parser.value(stageOption).toLower());
    if (initialStage < 0) {
        qCritical("Unknown stage: %s",
                  qPrintable(parser.value(stageOption)));
        return 2;
    }

    FrameMonitor frameMonitor;
    frameMonitor.setContinuous(parser.isSet(continuousOption));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("FrameMonitor"), &frameMonitor);
    engine.rootContext()->setContextProperty(
        QStringLiteral("InitialStage"), initialStage);
    engine.rootContext()->setContextProperty(
        QStringLiteral("RenderLoopName"), renderLoop);
    engine.rootContext()->setContextProperty(
        QStringLiteral("GraphicsApiName"), graphicsApi);
    engine.rootContext()->setContextProperty(
        QStringLiteral("VBlankVirtualizationDisabled"),
        parser.isSet(disableVBlankOption));
    engine.rootContext()->setContextProperty(
        QStringLiteral("RedirectionSurfaceDisabled"),
        parser.isSet(disableRedirectionSurfaceOption));
    engine.rootContext()->setContextProperty(
        QStringLiteral("AlphaBufferEnabled"),
        parser.isSet(alphaBufferOption));
    engine.rootContext()->setContextProperty(
        QStringLiteral("InitialBenchmark"),
        parser.isSet(benchmarkOption));
    engine.rootContext()->setContextProperty(
        QStringLiteral("D3DLegacySwapchain"),
        parser.isSet(d3dLegacySwapchainOption));
    engine.rootContext()->setContextProperty(
        QStringLiteral("D3DMaxFrameLatency"),
        parser.value(d3dMaxFrameLatencyOption));

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        qCritical("The diagnostic root object is not a QQuickWindow");
        return 1;
    }

    frameMonitor.setWindow(window);

    if (parser.isSet(smokeTestOption)) {
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
    }

    return app.exec();
}

#include "main.moc"
