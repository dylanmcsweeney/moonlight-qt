#include "uirenderermanager.h"

#include "settings/streamingpreferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtDebug>

#include <utility>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#endif

namespace {
QString graphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software:
        return QStringLiteral("Software");
    case QSGRendererInterface::OpenVG:
        return QStringLiteral("OpenVG");
    case QSGRendererInterface::OpenGL:
        return QStringLiteral("OpenGL");
    case QSGRendererInterface::Direct3D11:
        return QStringLiteral("Direct3D 11");
    case QSGRendererInterface::Vulkan:
        return QStringLiteral("Vulkan");
    case QSGRendererInterface::Metal:
        return QStringLiteral("Metal");
    case QSGRendererInterface::Null:
        return QStringLiteral("Null");
    case QSGRendererInterface::Direct3D12:
        return QStringLiteral("Direct3D 12");
    case QSGRendererInterface::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

#ifdef Q_OS_WIN
bool usesD3DWindowSurface(const StreamingPreferences* preferences)
{
    // Explicit environment overrides take precedence over Moonlight's
    // preference. Qt's automatic Windows backend is Direct3D 11.
    const QByteArray quickBackend = qgetenv("QT_QUICK_BACKEND");
    if (quickBackend.compare("software", Qt::CaseInsensitive) == 0) {
        return false;
    }

    const QByteArray rhiBackend = qgetenv("QSG_RHI_BACKEND");
    if (!rhiBackend.isEmpty()) {
        return rhiBackend.compare("d3d11", Qt::CaseInsensitive) == 0 ||
               rhiBackend.compare("d3d12", Qt::CaseInsensitive) == 0;
    }

    return preferences->uiGraphicsBackend !=
        StreamingPreferences::UI_GRAPHICS_OPENGL;
}

bool inspectD3D11Adapter(ID3D11Device* device, QString& name,
                         bool& software)
{
    IDXGIDevice* dxgiDevice = nullptr;
    if (!device ||
            FAILED(device->QueryInterface(__uuidof(IDXGIDevice),
                                          reinterpret_cast<void**>(&dxgiDevice)))) {
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    const HRESULT getAdapterResult = dxgiDevice->GetAdapter(&adapter);
    dxgiDevice->Release();
    if (FAILED(getAdapterResult)) {
        return false;
    }

    IDXGIAdapter1* adapter1 = nullptr;
    const HRESULT queryResult =
        adapter->QueryInterface(__uuidof(IDXGIAdapter1),
                                reinterpret_cast<void**>(&adapter1));
    adapter->Release();
    if (FAILED(queryResult)) {
        return false;
    }

    DXGI_ADAPTER_DESC1 description = {};
    const HRESULT descriptionResult = adapter1->GetDesc1(&description);
    adapter1->Release();
    if (FAILED(descriptionResult)) {
        return false;
    }

    name = QString::fromWCharArray(description.Description);
    software = (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    return true;
}

bool inspectD3D12Adapter(ID3D12Device* device, QString& name,
                         bool& software)
{
    if (!device) {
        return false;
    }

    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(
            __uuidof(IDXGIFactory1),
            reinterpret_cast<void**>(&factory)))) {
        return false;
    }

    const LUID deviceLuid = device->GetAdapterLuid();
    bool found = false;
    for (UINT index = 0; ; ++index) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (!adapter) {
            continue;
        }

        DXGI_ADAPTER_DESC1 description = {};
        if (SUCCEEDED(adapter->GetDesc1(&description)) &&
                description.AdapterLuid.HighPart == deviceLuid.HighPart &&
                description.AdapterLuid.LowPart == deviceLuid.LowPart) {
            name = QString::fromWCharArray(description.Description);
            software =
                (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            found = true;
            adapter->Release();
            break;
        }

        adapter->Release();
    }

    if (!found) {
        IDXGIFactory4* factory4 = nullptr;
        if (SUCCEEDED(factory->QueryInterface(
                __uuidof(IDXGIFactory4),
                reinterpret_cast<void**>(&factory4)))) {
            IDXGIAdapter1* warpAdapter = nullptr;
            if (SUCCEEDED(factory4->EnumWarpAdapter(
                    __uuidof(IDXGIAdapter1),
                    reinterpret_cast<void**>(&warpAdapter)))) {
                DXGI_ADAPTER_DESC1 description = {};
                if (SUCCEEDED(warpAdapter->GetDesc1(&description)) &&
                        description.AdapterLuid.HighPart ==
                            deviceLuid.HighPart &&
                        description.AdapterLuid.LowPart ==
                            deviceLuid.LowPart) {
                    name = QString::fromWCharArray(
                        description.Description);
                    software = true;
                    found = true;
                }
                warpAdapter->Release();
            }
            factory4->Release();
        }
    }

    factory->Release();
    return found;
}
#endif
}

UiRendererManager::UiRendererManager(StreamingPreferences* preferences,
                                     QObject* parent)
    : QObject(parent),
      m_Preferences(preferences),
      m_InspectionComplete(false),
      m_RendererReady(false),
      m_SoftwareRenderer(false)
{
    Q_ASSERT(m_Preferences);
}

void UiRendererManager::configureGraphicsBackend(
    const StreamingPreferences* preferences)
{
    Q_ASSERT(preferences);

#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        switch (preferences->uiGraphicsBackend) {
        case StreamingPreferences::UI_GRAPHICS_D3D12:
            QQuickWindow::setGraphicsApi(
                QSGRendererInterface::Direct3D12);
            break;
        case StreamingPreferences::UI_GRAPHICS_D3D11:
            QQuickWindow::setGraphicsApi(
                QSGRendererInterface::Direct3D11);
            break;
        case StreamingPreferences::UI_GRAPHICS_OPENGL:
            QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
            break;
        case StreamingPreferences::UI_GRAPHICS_AUTOMATIC:
        default:
            break;
        }
    }
    else {
        qInfo() << "Using externally selected Qt Quick graphics backend:"
                << qgetenv("QSG_RHI_BACKEND");
    }

    const bool d3d11Requested =
        preferences->uiGraphicsBackend ==
            StreamingPreferences::UI_GRAPHICS_D3D11 ||
        qgetenv("QSG_RHI_BACKEND").compare("d3d11",
                                           Qt::CaseInsensitive) == 0;
    if (d3d11Requested &&
            preferences->uiD3D11SwapchainMode ==
                StreamingPreferences::UI_D3D11_LEGACY &&
            !qEnvironmentVariableIsSet("QT_D3D_NO_FLIP")) {
        qputenv("QT_D3D_NO_FLIP", "1");
    }

    if (usesD3DWindowSurface(preferences)) {
        // A normal Qt D3D window uses an HWND swap chain with
        // DXGI_SCALING_NONE. During an interactive resize, the HWND and swap
        // chain can briefly have different sizes. DXGI fills newly exposed
        // space with black, while DWM may expose stale white pixels from its
        // separate HWND redirection bitmap.
        //
        // Requesting an alpha-capable Qt Quick surface makes Qt use a
        // DirectComposition swap chain. Composition swap chains use
        // DXGI_SCALING_STRETCH, so DWM stretches the last complete frame over
        // temporary size mismatches instead of showing black. The QML window
        // remains opaque; alpha support is only the Qt API trigger for this
        // DirectComposition path.
        QQuickWindow::setDefaultAlphaBuffer(true);

        // DirectComposition supplies the window content directly to DWM, so
        // the traditional HWND redirection bitmap is redundant. Suppressing
        // it prevents its unpainted/stale white contents from flashing during
        // startup and interactive shrinking.
        if (!qEnvironmentVariableIsSet(
                "QT_QPA_DISABLE_REDIRECTION_SURFACE")) {
            qputenv("QT_QPA_DISABLE_REDIRECTION_SURFACE", "1");
        }

        qInfo() << "Using DirectComposition Qt Quick window without a DWM"
                   " redirection bitmap";
    }
#else
    Q_UNUSED(preferences);
#endif
}

void UiRendererManager::setWindow(QQuickWindow* window)
{
    if (m_Window == window) {
        return;
    }

    if (m_Window) {
        disconnect(m_Window, nullptr, this, nullptr);
    }

    m_Window = window;
    m_InspectionComplete.store(false);
    if (!m_Window) {
        return;
    }

    connect(m_Window, &QQuickWindow::beforeRendering, this,
            [this, window]() { inspectRenderer(window); },
            Qt::DirectConnection);
    connect(m_Window, &QObject::destroyed, this, [this]() {
        m_Window = nullptr;
    });

    m_Window->update();
}

bool UiRendererManager::rendererReady() const
{
    return m_RendererReady;
}

QString UiRendererManager::actualBackend() const
{
    return m_ActualBackend;
}

QString UiRendererManager::adapterName() const
{
    return m_AdapterName;
}

bool UiRendererManager::softwareRenderer() const
{
    return m_SoftwareRenderer;
}

bool UiRendererManager::restartApplication()
{
    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments(QCoreApplication::arguments().mid(1));
    process.setWorkingDirectory(QDir::currentPath());

    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QSG_RHI_BACKEND"));
    environment.remove(QStringLiteral("QT_D3D_NO_FLIP"));
    process.setProcessEnvironment(environment);

    if (!process.startDetached()) {
        qWarning() << "Unable to restart application";
        return false;
    }

    QCoreApplication::quit();
    return true;
}

bool UiRendererManager::restartWithGraphicsConfiguration(
    int backend, int d3d11SwapchainMode)
{
    if (backend < StreamingPreferences::UI_GRAPHICS_AUTOMATIC ||
            backend > StreamingPreferences::UI_GRAPHICS_OPENGL ||
            d3d11SwapchainMode < StreamingPreferences::UI_D3D11_FLIP ||
            d3d11SwapchainMode >
                StreamingPreferences::UI_D3D11_LEGACY) {
        return false;
    }

    m_Preferences->uiGraphicsBackend =
        static_cast<StreamingPreferences::UIGraphicsBackend>(backend);
    m_Preferences->uiD3D11SwapchainMode =
        static_cast<StreamingPreferences::UID3D11SwapchainMode>(
            d3d11SwapchainMode);
    m_Preferences->save();
    return restartApplication();
}

void UiRendererManager::inspectRenderer(QQuickWindow* window)
{
    bool expected = false;
    if (!m_InspectionComplete.compare_exchange_strong(expected, true)) {
        return;
    }

    QSGRendererInterface* rendererInterface = window->rendererInterface();
    const QSGRendererInterface::GraphicsApi api =
        rendererInterface->graphicsApi();
    QString adapterName;
    bool softwareRenderer = api == QSGRendererInterface::Software;

#ifdef Q_OS_WIN
    if (api == QSGRendererInterface::Direct3D11) {
        inspectD3D11Adapter(
            static_cast<ID3D11Device*>(
                rendererInterface->getResource(
                    window, QSGRendererInterface::DeviceResource)),
            adapterName, softwareRenderer);
    }
    else if (api == QSGRendererInterface::Direct3D12) {
        inspectD3D12Adapter(
            static_cast<ID3D12Device*>(
                rendererInterface->getResource(
                    window, QSGRendererInterface::DeviceResource)),
            adapterName, softwareRenderer);
    }
#endif

    const QString backend = graphicsApiName(api);
    const bool isD3D12 = api == QSGRendererInterface::Direct3D12;
    QMetaObject::invokeMethod(
        this,
        [this, backend, adapterName, softwareRenderer, isD3D12]() {
            updateRenderer(backend, adapterName, softwareRenderer,
                           isD3D12);
        },
        Qt::QueuedConnection);
}

void UiRendererManager::updateRenderer(QString backend,
                                       QString adapterName,
                                       bool softwareRenderer,
                                       bool isD3D12)
{
    m_RendererReady = true;
    m_ActualBackend = std::move(backend);
    m_AdapterName = std::move(adapterName);
    m_SoftwareRenderer = softwareRenderer;
    emit rendererChanged();

    qInfo() << "Qt Quick renderer:" << m_ActualBackend
            << "adapter:" << m_AdapterName
            << "software:" << m_SoftwareRenderer;

    if (isD3D12 && m_SoftwareRenderer) {
        emit softwareD3D12Detected(m_AdapterName);
    }
}
