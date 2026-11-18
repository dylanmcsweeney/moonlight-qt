#include "streaming/session.h"

#include <cstdlib>

// These dependencies are outside the keyboard path under test. Make an
// unexpected command fail the fixture instead of silently ignoring it.
[[noreturn]] static void unexpected(const char* command)
{
    qFatal("Unexpected %s", command);
    std::abort();
}

Session* Session::s_ActiveSession = nullptr;

void Session::toggleFullscreen() { unexpected("fullscreen command"); }
void Session::setShouldExit(bool) { unexpected("quit command"); }
void Overlay::OverlayManager::setOverlayState(Overlay::OverlayType, bool) { unexpected("overlay command"); }
bool Overlay::OverlayManager::isOverlayEnabled(Overlay::OverlayType) { unexpected("overlay query"); }
int LiSendUtf8TextEvent(const char*, unsigned int) { unexpected("clipboard command"); }

SdlInputHandler::SdlInputHandler(StreamingPreferences&, int, int)
    : m_SpecialKeyCombos{}
{
}

SdlInputHandler::~SdlInputHandler() = default;
bool SdlInputHandler::isCaptureActive() { unexpected("pointer capture query"); }
void SdlInputHandler::setCaptureActive(bool) { unexpected("pointer capture command"); }
bool SdlInputHandler::isSystemKeyCaptureActive() { unexpected("system key capture query"); }
void SdlInputHandler::updateKeyboardGrabState() { unexpected("keyboard grab command"); }
void SdlInputHandler::updatePointerRegionLock() { unexpected("pointer lock command"); }

StreamingPreferences::StreamingPreferences(QObject* parent) : QObject(parent), m_QmlEngine(nullptr)
{
}

// The moc code for StreamingPreferences references its invokable methods.
int StreamingPreferences::getDefaultBitrate(int, int, int, bool) { unexpected("bitrate query"); }
int StreamingPreferences::getDefaultPyroWaveBitrate(int, int, int, bool, bool) { unexpected("PyroWave bitrate query"); }
bool StreamingPreferences::save() { unexpected("settings write"); }
void StreamingPreferences::resetToStock() { unexpected("settings reset"); }
bool StreamingPreferences::retranslate() { unexpected("translation request"); }
void StreamingPreferences::openDiagnosticsFolder() { unexpected("diagnostics request"); }
void StreamingPreferences::exportLatestDiagnostics() { unexpected("diagnostics export"); }
QVariantList StreamingPreferences::getFpsChoices(const QVariantList&) const { unexpected("frame rate query"); }
