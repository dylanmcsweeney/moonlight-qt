#include "streamingpreferences.h"
#include "utils.h"
#include "streaming/vrrratepolicy.h"

#include <QSettings>
#include <QTranslator>
#include <QCoreApplication>
#include <QLocale>
#include <QReadWriteLock>
#include <QVariantMap>
#include <QtMath>

#include <QtDebug>

#include <vector>

#define SER_STREAMSETTINGS "streamsettings"
#define SER_WIDTH "width"
#define SER_HEIGHT "height"
#define SER_FPS "fps"
#define SER_BITRATE "bitrate"
#define SER_UNLOCK_BITRATE "unlockbitrate"
#define SER_AUTOADJUSTBITRATE "autoadjustbitrate"
#define SER_FULLSCREEN "fullscreen"
#define SER_VSYNC "vsync"
#define SER_ENABLEVRR "enablevrr"
#define SER_GAMEOPTS "gameopts"
#define SER_HOSTAUDIO "hostaudio"
#define SER_MULTICONT "multicontroller"
#define SER_AUDIOCFG "audiocfg"
#define SER_VIDEOCFG "videocfg"
#define SER_HDR "hdr"
#define SER_YUV444 "yuv444"
#define SER_VIDEODEC "videodec"
#define SER_WINDOWMODE "windowmode"
#define SER_MDNS "mdns"
#define SER_QUITAPPAFTER "quitAppAfter"
#define SER_ABSMOUSEMODE "mouseacceleration"
#define SER_ABSTOUCHMODE "abstouchmode"
#define SER_STARTWINDOWED "startwindowed"
#define SER_FRAMEPACING "framepacing"
#define SER_CONNWARNINGS "connwarnings"
#define SER_CONFWARNINGS "confwarnings"
#define SER_UIDISPLAYMODE "uidisplaymode"
#define SER_UIFRAMEPACINGMODE "uiframepacingmode"
#define SER_UIGRAPHICSBACKEND "uigraphicsbackend"
#define SER_UID3D11SWAPCHAINMODE "uid3d11swapchainmode"
#define SER_RICHPRESENCE "richpresence"
#define SER_GAMEPADMOUSE "gamepadmouse"
#define SER_DEFAULTVER "defaultver"
#define SER_PACKETSIZE "packetsize"
#define SER_DETECTNETBLOCKING "detectnetblocking"
#define SER_SHOWPERFOVERLAY "showperfoverlay"
#define SER_SWAPMOUSEBUTTONS "swapmousebuttons"
#define SER_MUTEONFOCUSLOSS "muteonfocusloss"
#define SER_BACKGROUNDGAMEPAD "backgroundgamepad"
#define SER_REVERSESCROLL "reversescroll"
#define SER_SWAPFACEBUTTONS "swapfacebuttons"
#define SER_UISWAPFACEBUTTONS "uiswapfacebuttons"
#define SER_SHOWPCPROFILECONTROLS "showpcprofilecontrols"
#define SER_CAPTURESYSKEYS "capturesyskeys"
#define SER_KEEPAWAKE "keepawake"
#define SER_LANGUAGE "language"
#define SER_RENDERER "renderer"

static StreamingPreferences* s_GlobalPrefs;

Q_GLOBAL_STATIC(QReadWriteLock, s_GlobalPrefsLock)

StreamingPreferences::StreamingPreferences(QQmlEngine *qmlEngine)
    : m_QmlEngine(qmlEngine)
{
    reload();
}

StreamingPreferences::StreamingPreferences(QObject* parent)
    : QObject(parent),
      m_QmlEngine(nullptr)
{
    resetToStock();
}

StreamingPreferences* StreamingPreferences::get(QQmlEngine *qmlEngine)
{
    {
        QReadLocker readGuard(s_GlobalPrefsLock);

        // If we have a preference object and it's associated with a QML engine or
        // if the caller didn't specify a QML engine, return the existing object.
        if (s_GlobalPrefs && (s_GlobalPrefs->m_QmlEngine || !qmlEngine)) {
            // The lifetime logic here relies on the QML engine also being a singleton.
            Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            return s_GlobalPrefs;
        }
    }

    {
        QWriteLocker writeGuard(s_GlobalPrefsLock);

        // If we already have an preference object but the QML engine is now available,
        // associate the QML engine with the preferences.
        if (s_GlobalPrefs) {
            if (!s_GlobalPrefs->m_QmlEngine) {
                s_GlobalPrefs->m_QmlEngine = qmlEngine;
            }
            else {
                // We could reach this codepath if another thread raced with us
                // and created the object while we were outside the pref lock.
                Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            }
        }
        else {
            s_GlobalPrefs = new StreamingPreferences(qmlEngine);
        }

        return s_GlobalPrefs;
    }
}

void StreamingPreferences::reload()
{
    QSettings settings;
    resetToStock();
    loadLegacySettings(settings);
}

void StreamingPreferences::resetToStock()
{
#ifdef Q_OS_DARWIN
    recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
#else
    // Wayland doesn't support modesetting, so use fullscreen desktop mode
    // unless we have a slow GPU (which can take advantage of wp_viewporter
    // to reduce GPU load with lower resolution video streams).
    if (WMUtils::isRunningWayland() && !WMUtils::isGpuSlow()) {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
    }
    else {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN;
    }
#endif

    width = 1280;
    height = 720;
    fps = 60;
    enableYUV444 = false;
    bitrateKbps = getDefaultBitrate(width, height, fps, enableYUV444);
    unlockBitrate = false;
    autoAdjustBitrate = true;
    enableVsync = true;
    gameOptimizations = true;
    playAudioOnHost = false;
    multiController = true;
    enableMdns = true;
    quitAppAfter = false;
    absoluteMouseMode = false;
    absoluteTouchMode = true;
    framePacing = false;
    enableVrr = false;
    connectionWarnings = true;
    configurationWarnings = true;
    richPresence = true;
    gamepadMouse = true;
    detectNetworkBlocking = true;
    showPerformanceOverlay = false;
    packetSize = 0;
    swapMouseButtons = false;
    muteOnFocusLoss = false;
    backgroundGamepad = false;
    reverseScrollDirection = false;
    swapFaceButtons = false;
    uiSwapFaceButtons = false;
    showPcProfileControls = true;
    keepAwake = true;
    enableHdr = false;
    captureSysKeysMode = CaptureSysKeysMode::CSK_OFF;
    audioConfig = AudioConfig::AC_STEREO;
    videoCodecConfig = VideoCodecConfig::VCC_AUTO;
    videoDecoderSelection = VideoDecoderSelection::VDS_AUTO;
    rendererSelection = RendererSelection::RS_AUTO;
    windowMode = recommendedFullScreenMode;
    uiDisplayMode = UIDisplayMode::UI_WINDOWED;
    uiFramePacingMode = UIFramePacingMode::UI_FRAME_PACING_DISABLED;
#ifdef Q_OS_WIN
    uiGraphicsBackend = UIGraphicsBackend::UI_GRAPHICS_D3D12;
#else
    uiGraphicsBackend = UIGraphicsBackend::UI_GRAPHICS_AUTOMATIC;
#endif
    uiD3D11SwapchainMode = UID3D11SwapchainMode::UI_D3D11_FLIP;
    language = Language::LANG_AUTO;
}

void StreamingPreferences::loadLegacySettings(QSettings& settings)
{
    enableMdns = settings.value(SER_MDNS, enableMdns).toBool();
    detectNetworkBlocking = settings.value(SER_DETECTNETBLOCKING, detectNetworkBlocking).toBool();
    richPresence = settings.value(SER_RICHPRESENCE, richPresence).toBool();
    uiDisplayMode = static_cast<UIDisplayMode>(settings.value(SER_UIDISPLAYMODE,
                                               static_cast<int>(settings.value(SER_STARTWINDOWED, true).toBool() ? UIDisplayMode::UI_WINDOWED
                                                                                                                 : UIDisplayMode::UI_MAXIMIZED)).toInt());
    uiFramePacingMode = static_cast<UIFramePacingMode>(
        settings.value(SER_UIFRAMEPACINGMODE,
                       static_cast<int>(uiFramePacingMode)).toInt());
    if (uiFramePacingMode < UI_FRAME_PACING_DISABLED ||
            uiFramePacingMode > UI_FRAME_PACING_UNBOUNDED) {
        uiFramePacingMode = UI_FRAME_PACING_DISABLED;
    }
    uiGraphicsBackend = static_cast<UIGraphicsBackend>(
        settings.value(SER_UIGRAPHICSBACKEND,
                       static_cast<int>(uiGraphicsBackend)).toInt());
    if (uiGraphicsBackend < UI_GRAPHICS_AUTOMATIC ||
            uiGraphicsBackend > UI_GRAPHICS_OPENGL) {
#ifdef Q_OS_WIN
        uiGraphicsBackend = UI_GRAPHICS_D3D12;
#else
        uiGraphicsBackend = UI_GRAPHICS_AUTOMATIC;
#endif
    }
    uiD3D11SwapchainMode = static_cast<UID3D11SwapchainMode>(
        settings.value(SER_UID3D11SWAPCHAINMODE,
                       static_cast<int>(uiD3D11SwapchainMode)).toInt());
    if (uiD3D11SwapchainMode < UI_D3D11_FLIP ||
            uiD3D11SwapchainMode > UI_D3D11_LEGACY) {
        uiD3D11SwapchainMode = UI_D3D11_FLIP;
    }
    language = static_cast<Language>(settings.value(SER_LANGUAGE, static_cast<int>(language)).toInt());
    uiSwapFaceButtons = settings.value(SER_UISWAPFACEBUTTONS,
                                       settings.value(SER_SWAPFACEBUTTONS, false)).toBool();
    showPcProfileControls =
        settings.value(SER_SHOWPCPROFILECONTROLS,
                       showPcProfileControls).toBool();
}

bool StreamingPreferences::retranslate()
{
    static QTranslator* translator = nullptr;

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (m_QmlEngine != nullptr) {
        // Dynamic retranslation is not supported until Qt 5.10
        return false;
    }
#endif

    QTranslator* newTranslator = new QTranslator();
    QString languageSuffix = getSuffixFromLanguage(language);

    // Remove the old translator, even if we can't load a new one.
    // Otherwise we'll be stuck with the old translated values instead
    // of defaulting to English.
    if (translator != nullptr) {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    if (newTranslator->load(QString(":/languages/qml_") + languageSuffix)) {
        qInfo() << "Successfully loaded translation for" << languageSuffix;

        translator = newTranslator;
        QCoreApplication::installTranslator(translator);
    }
    else {
        qInfo() << "No translation available for" << languageSuffix;
        delete newTranslator;
    }

    if (m_QmlEngine != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        // This is a dynamic retranslation from the settings page.
        // We have to kick the QML engine into reloading our text.
        m_QmlEngine->retranslate();
#else
        // Unreachable below Qt 5.10 due to the check above
        Q_ASSERT(false);
#endif
    }
    else {
        // This is a translation from a non-QML context, which means
        // it is probably app startup. There's nothing to refresh.
    }

    return true;
}

QString StreamingPreferences::getSuffixFromLanguage(StreamingPreferences::Language lang)
{
    switch (lang)
    {
    case LANG_DE:
        return "de";
    case LANG_EN:
        return "en";
    case LANG_FR:
        return "fr";
    case LANG_ZH_CN:
        return "zh_CN";
    case LANG_NB_NO:
        return "nb_NO";
    case LANG_RU:
        return "ru";
    case LANG_ES:
        return "es";
    case LANG_JA:
        return "ja";
    case LANG_VI:
        return "vi";
    case LANG_TH:
        return "th";
    case LANG_KO:
        return "ko";
    case LANG_HU:
        return "hu";
    case LANG_NL:
        return "nl";
    case LANG_SV:
        return "sv";
    case LANG_TR:
        return "tr";
    case LANG_UK:
        return "uk";
    case LANG_ZH_TW:
        return "zh_TW";
    case LANG_PT:
        return "pt";
    case LANG_PT_BR:
        return "pt_BR";
    case LANG_EL:
        return "el";
    case LANG_IT:
        return "it";
    case LANG_HI:
        return "hi";
    case LANG_PL:
        return "pl";
    case LANG_CS:
        return "cs";
    case LANG_HE:
        return "he";
    case LANG_CKB:
        return "ckb";
    case LANG_LT:
        return "lt";
    case LANG_ET:
        return "et";
    case LANG_BG:
        return "bg";
    case LANG_EO:
        return "eo";
    case LANG_TA:
        return "ta";
    case LANG_AUTO:
    default:
        return QLocale::system().name();
    }
}

void StreamingPreferences::save()
{
    QSettings settings;

    settings.setValue(SER_MDNS, enableMdns);
    settings.setValue(SER_DETECTNETBLOCKING, detectNetworkBlocking);
    settings.setValue(SER_RICHPRESENCE, richPresence);
    settings.setValue(SER_UIDISPLAYMODE, static_cast<int>(uiDisplayMode));
    settings.setValue(SER_UIFRAMEPACINGMODE, static_cast<int>(uiFramePacingMode));
    settings.setValue(SER_UIGRAPHICSBACKEND, static_cast<int>(uiGraphicsBackend));
    settings.setValue(SER_UID3D11SWAPCHAINMODE, static_cast<int>(uiD3D11SwapchainMode));
    settings.setValue(SER_LANGUAGE, static_cast<int>(language));
    settings.setValue(SER_UISWAPFACEBUTTONS, uiSwapFaceButtons);
    settings.setValue(SER_SHOWPCPROFILECONTROLS, showPcProfileControls);
}

void StreamingPreferences::loadStreamSettings(QSettings& settings)
{
    width = settings.value(SER_WIDTH, width).toInt();
    height = settings.value(SER_HEIGHT, height).toInt();
    fps = settings.value(SER_FPS, fps).toInt();
    enableYUV444 = settings.value(SER_YUV444, enableYUV444).toBool();
    bitrateKbps = settings.value(SER_BITRATE, getDefaultBitrate(width, height, fps, enableYUV444)).toInt();
    unlockBitrate = settings.value(SER_UNLOCK_BITRATE, unlockBitrate).toBool();
    autoAdjustBitrate = settings.value(SER_AUTOADJUSTBITRATE, autoAdjustBitrate).toBool();
    enableVsync = settings.value(SER_VSYNC, enableVsync).toBool();
    gameOptimizations = settings.value(SER_GAMEOPTS, gameOptimizations).toBool();
    playAudioOnHost = settings.value(SER_HOSTAUDIO, playAudioOnHost).toBool();
    multiController = settings.value(SER_MULTICONT, multiController).toBool();
    quitAppAfter = settings.value(SER_QUITAPPAFTER, quitAppAfter).toBool();
    absoluteMouseMode = settings.value(SER_ABSMOUSEMODE, absoluteMouseMode).toBool();
    absoluteTouchMode = settings.value(SER_ABSTOUCHMODE, absoluteTouchMode).toBool();
    framePacing = settings.value(SER_FRAMEPACING, framePacing).toBool();
    enableVrr = settings.value(SER_ENABLEVRR, enableVrr).toBool();
    connectionWarnings = settings.value(SER_CONNWARNINGS, connectionWarnings).toBool();
    configurationWarnings = settings.value(SER_CONFWARNINGS, configurationWarnings).toBool();
    gamepadMouse = settings.value(SER_GAMEPADMOUSE, gamepadMouse).toBool();
    showPerformanceOverlay = settings.value(SER_SHOWPERFOVERLAY, showPerformanceOverlay).toBool();
    packetSize = settings.value(SER_PACKETSIZE, packetSize).toInt();
    swapMouseButtons = settings.value(SER_SWAPMOUSEBUTTONS, swapMouseButtons).toBool();
    muteOnFocusLoss = settings.value(SER_MUTEONFOCUSLOSS, muteOnFocusLoss).toBool();
    backgroundGamepad = settings.value(SER_BACKGROUNDGAMEPAD, backgroundGamepad).toBool();
    reverseScrollDirection = settings.value(SER_REVERSESCROLL, reverseScrollDirection).toBool();
    swapFaceButtons = settings.value(SER_SWAPFACEBUTTONS, swapFaceButtons).toBool();
    keepAwake = settings.value(SER_KEEPAWAKE, keepAwake).toBool();
    enableHdr = settings.value(SER_HDR, enableHdr).toBool();
    captureSysKeysMode = static_cast<CaptureSysKeysMode>(settings.value(SER_CAPTURESYSKEYS, static_cast<int>(captureSysKeysMode)).toInt());
    audioConfig = static_cast<AudioConfig>(settings.value(SER_AUDIOCFG, static_cast<int>(audioConfig)).toInt());
    videoCodecConfig = static_cast<VideoCodecConfig>(settings.value(SER_VIDEOCFG, static_cast<int>(videoCodecConfig)).toInt());
    videoDecoderSelection = static_cast<VideoDecoderSelection>(settings.value(SER_VIDEODEC, static_cast<int>(videoDecoderSelection)).toInt());
    windowMode = static_cast<WindowMode>(settings.value(SER_WINDOWMODE,
                                                        static_cast<int>(settings.value(SER_FULLSCREEN, true).toBool() ?
                                                                             recommendedFullScreenMode : WindowMode::WM_WINDOWED)).toInt());

    if (videoCodecConfig == VCC_FORCE_HEVC_HDR_DEPRECATED) {
        videoCodecConfig = VCC_AUTO;
        enableHdr = true;
    }

    if (!streamSettingsValid()) {
        resetToStock();
    }
}

void StreamingPreferences::saveStreamSettings(QSettings& settings) const
{
    settings.setValue(SER_WIDTH, width);
    settings.setValue(SER_HEIGHT, height);
    settings.setValue(SER_FPS, fps);
    settings.setValue(SER_BITRATE, bitrateKbps);
    settings.setValue(SER_UNLOCK_BITRATE, unlockBitrate);
    settings.setValue(SER_AUTOADJUSTBITRATE, autoAdjustBitrate);
    settings.setValue(SER_VSYNC, enableVsync);
    settings.setValue(SER_ENABLEVRR, enableVrr);
    settings.setValue(SER_GAMEOPTS, gameOptimizations);
    settings.setValue(SER_HOSTAUDIO, playAudioOnHost);
    settings.setValue(SER_MULTICONT, multiController);
    settings.setValue(SER_QUITAPPAFTER, quitAppAfter);
    settings.setValue(SER_ABSMOUSEMODE, absoluteMouseMode);
    settings.setValue(SER_ABSTOUCHMODE, absoluteTouchMode);
    settings.setValue(SER_FRAMEPACING, framePacing);
    settings.setValue(SER_CONNWARNINGS, connectionWarnings);
    settings.setValue(SER_CONFWARNINGS, configurationWarnings);
    settings.setValue(SER_GAMEPADMOUSE, gamepadMouse);
    settings.setValue(SER_PACKETSIZE, packetSize);
    settings.setValue(SER_SHOWPERFOVERLAY, showPerformanceOverlay);
    settings.setValue(SER_AUDIOCFG, static_cast<int>(audioConfig));
    settings.setValue(SER_HDR, enableHdr);
    settings.setValue(SER_YUV444, enableYUV444);
    settings.setValue(SER_VIDEOCFG, static_cast<int>(videoCodecConfig));
    settings.setValue(SER_VIDEODEC, static_cast<int>(videoDecoderSelection));
    settings.setValue(SER_RENDERER, static_cast<int>(rendererSelection));
    settings.setValue(SER_WINDOWMODE, static_cast<int>(windowMode));
    settings.setValue(SER_SWAPMOUSEBUTTONS, swapMouseButtons);
    settings.setValue(SER_MUTEONFOCUSLOSS, muteOnFocusLoss);
    settings.setValue(SER_BACKGROUNDGAMEPAD, backgroundGamepad);
    settings.setValue(SER_REVERSESCROLL, reverseScrollDirection);
    settings.setValue(SER_SWAPFACEBUTTONS, swapFaceButtons);
    settings.setValue(SER_CAPTURESYSKEYS, captureSysKeysMode);
    settings.setValue(SER_KEEPAWAKE, keepAwake);
}

QVariantList StreamingPreferences::getFpsChoices(const QVariantList& refreshRates) const
{
    std::vector<int> rates;
    rates.reserve(refreshRates.size());
    for (const QVariant& value : refreshRates) {
        bool ok = false;
        const int refreshHz = value.toInt(&ok);
        if (ok && refreshHz > 0) {
            rates.push_back(refreshHz);
        }
    }

    const std::vector<VrrFpsChoice> choices = VrrRatePolicy::buildChoices(
        rates, fps, enableVsync && enableVrr);
    QVariantList result;
    for (const VrrFpsChoice& choice : choices) {
        QVariantMap item;
        item.insert("video_fps", QString::number(choice.fps));
        item.insert("is_custom", choice.kind == VrrFpsChoiceKind::Custom);

        switch (choice.kind) {
        case VrrFpsChoiceKind::Fixed:
            item.insert("kind", "fixed");
            break;
        case VrrFpsChoiceKind::Vrr:
            item.insert("kind", "vrr");
            break;
        case VrrFpsChoiceKind::LowLatencyVrr:
            item.insert("kind", "low-latency-vrr");
            break;
        case VrrFpsChoiceKind::Custom:
            item.insert("kind", "custom");
            break;
        }

        result.append(item);
    }

    return result;
}

void StreamingPreferences::copyStreamSettingsFrom(const StreamingPreferences& other)
{
#define COPY_PREF(name) name = other.name
    COPY_PREF(width); COPY_PREF(height); COPY_PREF(fps); COPY_PREF(bitrateKbps);
    COPY_PREF(unlockBitrate); COPY_PREF(autoAdjustBitrate); COPY_PREF(enableVsync);
    COPY_PREF(gameOptimizations); COPY_PREF(playAudioOnHost); COPY_PREF(multiController);
    COPY_PREF(quitAppAfter); COPY_PREF(absoluteMouseMode); COPY_PREF(absoluteTouchMode);
    COPY_PREF(framePacing); COPY_PREF(enableVrr);
    COPY_PREF(connectionWarnings); COPY_PREF(configurationWarnings);
    COPY_PREF(gamepadMouse); COPY_PREF(showPerformanceOverlay); COPY_PREF(packetSize);
    COPY_PREF(swapMouseButtons); COPY_PREF(muteOnFocusLoss); COPY_PREF(backgroundGamepad);
    COPY_PREF(reverseScrollDirection); COPY_PREF(swapFaceButtons); COPY_PREF(keepAwake);
    COPY_PREF(audioConfig); COPY_PREF(videoCodecConfig); COPY_PREF(enableHdr);
    COPY_PREF(enableYUV444); COPY_PREF(videoDecoderSelection); COPY_PREF(windowMode);
    COPY_PREF(captureSysKeysMode);
#undef COPY_PREF
}

bool StreamingPreferences::streamSettingsEqual(const StreamingPreferences& other) const
{
#define SAME_PREF(name) name == other.name
    return SAME_PREF(width) && SAME_PREF(height) && SAME_PREF(fps) &&
           SAME_PREF(bitrateKbps) && SAME_PREF(unlockBitrate) &&
           SAME_PREF(autoAdjustBitrate) && SAME_PREF(enableVsync) &&
           SAME_PREF(gameOptimizations) && SAME_PREF(playAudioOnHost) &&
           SAME_PREF(multiController) && SAME_PREF(quitAppAfter) &&
           SAME_PREF(absoluteMouseMode) && SAME_PREF(absoluteTouchMode) &&
           SAME_PREF(framePacing) && SAME_PREF(enableVrr) &&
           SAME_PREF(connectionWarnings) &&
           SAME_PREF(configurationWarnings) && SAME_PREF(gamepadMouse) &&
           SAME_PREF(showPerformanceOverlay) && SAME_PREF(packetSize) &&
           SAME_PREF(swapMouseButtons) && SAME_PREF(muteOnFocusLoss) &&
           SAME_PREF(backgroundGamepad) && SAME_PREF(reverseScrollDirection) &&
           SAME_PREF(swapFaceButtons) && SAME_PREF(keepAwake) &&
           SAME_PREF(audioConfig) && SAME_PREF(videoCodecConfig) &&
           SAME_PREF(enableHdr) && SAME_PREF(enableYUV444) &&
           SAME_PREF(videoDecoderSelection) && SAME_PREF(windowMode) &&
           SAME_PREF(captureSysKeysMode);
#undef SAME_PREF
}

bool StreamingPreferences::streamSettingsValid() const
{
    return width > 0 && height > 0 && fps > 0 && bitrateKbps > 0 &&
           packetSize >= 0 &&
           audioConfig >= AC_STEREO && audioConfig <= AC_71_SURROUND &&
           videoCodecConfig >= VCC_AUTO && videoCodecConfig <= VCC_FORCE_AV1 &&
           videoDecoderSelection >= VDS_AUTO &&
           videoDecoderSelection <= VDS_FORCE_SOFTWARE &&
           windowMode >= WM_FULLSCREEN && windowMode <= WM_WINDOWED &&
           captureSysKeysMode >= CSK_OFF && captureSysKeysMode <= CSK_ALWAYS;
}

void StreamingPreferences::notifyAllChanged()
{
    emit displayModeChanged();
    emit bitrateChanged();
    emit unlockBitrateChanged();
    emit autoAdjustBitrateChanged();
    emit enableVsyncChanged();
    emit gameOptimizationsChanged();
    emit playAudioOnHostChanged();
    emit multiControllerChanged();
    emit quitAppAfterChanged();
    emit absoluteMouseModeChanged();
    emit absoluteTouchModeChanged();
    emit audioConfigChanged();
    emit videoCodecConfigChanged();
    emit enableHdrChanged();
    emit enableYUV444Changed();
    emit videoDecoderSelectionChanged();
    emit windowModeChanged();
    emit framePacingChanged();
    emit enableVrrChanged();
    emit connectionWarningsChanged();
    emit configurationWarningsChanged();
    emit gamepadMouseChanged();
    emit showPerformanceOverlayChanged();
    emit mouseButtonsChanged();
    emit muteOnFocusLossChanged();
    emit backgroundGamepadChanged();
    emit reverseScrollDirectionChanged();
    emit swapFaceButtonsChanged();
    emit captureSysKeysModeChanged();
    emit keepAwakeChanged();
    emit uiFramePacingModeChanged();
    emit uiGraphicsBackendChanged();
    emit uiD3D11SwapchainModeChanged();
}

int StreamingPreferences::getDefaultBitrate(int width, int height, int fps, bool yuv444)
{
    // Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
    // bitrate increase for frame rate once we get to values that high.
    float frameRateFactor = (fps <= 60 ? fps : (qSqrt(fps / 60.f) * 60.f)) / 30.f;

    // TODO: Collect some empirical data to see if these defaults make sense.
    // We're just using the values that the Shield used, as we have for years.
    static const struct resTable {
        int pixels;
        int factor;
    } resTable[] {
        { 640 * 360, 1 },
        { 854 * 480, 2 },
        { 1280 * 720, 5 },
        { 1920 * 1080, 10 },
        { 2560 * 1440, 20 },
        { 3840 * 2160, 40 },
        { -1, -1 },
    };

    // Calculate the resolution factor by linear interpolation of the resolution table
    float resolutionFactor;
    int pixels = width * height;
    for (int i = 0;; i++) {
        if (pixels == resTable[i].pixels) {
            // We can bail immediately for exact matches
            resolutionFactor = resTable[i].factor;
            break;
        }
        else if (pixels < resTable[i].pixels) {
            if (i == 0) {
                // Never go below the lowest resolution entry
                resolutionFactor = resTable[i].factor;
            }
            else {
                // Interpolate between the entry greater than the chosen resolution (i) and the entry less than the chosen resolution (i-1)
                resolutionFactor = ((float)(pixels - resTable[i-1].pixels) / (resTable[i].pixels - resTable[i-1].pixels)) * (resTable[i].factor - resTable[i-1].factor) + resTable[i-1].factor;
            }
            break;
        }
        else if (resTable[i].pixels == -1) {
            // Never go above the highest resolution entry
            resolutionFactor = resTable[i-1].factor;
            break;
        }
    }

    if (yuv444) {
        // This is rough estimation based on the fact that 4:4:4 doubles the amount of raw YUV data compared to 4:2:0
        resolutionFactor *= 2;
    }

    return qRound(resolutionFactor * frameRateFactor) * 1000;
}
