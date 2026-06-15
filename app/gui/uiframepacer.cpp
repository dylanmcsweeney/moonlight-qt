#include "uiframepacer.h"

#include "settings/streamingpreferences.h"

#include <QCoreApplication>
#include <QEvent>
#include <QQuickWindow>
#include <QScreen>
#include <QtMath>

#include <chrono>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
constexpr qint64 NanosecondsPerSecond = 1000000000LL;
constexpr qint64 MeasurementIntervalNs = NanosecondsPerSecond;
constexpr qreal MaximumDisplayRefreshRate = 240.0;
}

UiFramePacer::UiFramePacer(StreamingPreferences* preferences, QObject* parent)
    : QObject(parent),
      m_Preferences(preferences),
      m_NextDeadlineNs(0),
      m_LastSwapNs(0),
      m_MeasurementStartNs(0),
      m_FramesSinceMeasurement(0),
      m_MeasuredFps(0.0),
      m_ConsecutiveFastFrames(0),
      m_InternalRateLimit(false),
      m_InteractiveMoveResize(false),
      m_Active(false)
{
    Q_ASSERT(m_Preferences);

    QCoreApplication::instance()->installNativeEventFilter(this);

    m_FrameTimer.setSingleShot(true);
    m_FrameTimer.setTimerType(Qt::PreciseTimer);

    connect(&m_FrameTimer, &UiFrameTimer::timeout,
            this, &UiFramePacer::requestFrame);
    connect(m_Preferences, &StreamingPreferences::uiFramePacingModeChanged,
            this, &UiFramePacer::handleModeChanged);
}

UiFramePacer::~UiFramePacer()
{
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

void UiFramePacer::setWindow(QQuickWindow* window)
{
    if (m_Window == window) {
        return;
    }

    stop();

    if (m_Window) {
        m_Window->removeEventFilter(this);
        disconnect(m_Window, nullptr, this, nullptr);
    }

    connectScreen(nullptr);
    m_Window = window;

    if (!m_Window) {
        return;
    }

    m_Window->installEventFilter(this);
    connect(m_Window, &QQuickWindow::frameSwapped,
            this, &UiFramePacer::handleFrameSwapped);
    connect(m_Window, &QWindow::visibleChanged,
            this, &UiFramePacer::reevaluate);
    connect(m_Window, &QWindow::visibilityChanged,
            this, &UiFramePacer::reevaluate);
    connect(m_Window, &QWindow::screenChanged,
            this, &UiFramePacer::handleScreenChanged);
    connect(m_Window, &QObject::destroyed, this, [this]() {
        m_Window = nullptr;
        connectScreen(nullptr);
        stop();
    });

    connectScreen(m_Window->screen());
    reevaluate();
}

qreal UiFramePacer::measuredFps() const
{
    return m_MeasuredFps;
}

qreal UiFramePacer::targetFps() const
{
    return configuredTargetFps();
}

bool UiFramePacer::active() const
{
    return m_Active;
}

bool UiFramePacer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_Window &&
            (event->type() == QEvent::Expose ||
             event->type() == QEvent::Show ||
             event->type() == QEvent::Hide ||
             event->type() == QEvent::WindowStateChange)) {
        QMetaObject::invokeMethod(this, "reevaluate", Qt::QueuedConnection);
    }

    return QObject::eventFilter(watched, event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool UiFramePacer::nativeEventFilter(const QByteArray& eventType,
                                     void* message,
                                     qintptr* result)
#else
bool UiFramePacer::nativeEventFilter(const QByteArray& eventType,
                                     void* message,
                                     long* result)
#endif
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

#ifdef Q_OS_WIN
    const MSG* nativeMessage = static_cast<const MSG*>(message);
    if ((nativeMessage->message == WM_ENTERSIZEMOVE ||
         nativeMessage->message == WM_EXITSIZEMOVE) &&
            m_Window &&
            nativeMessage->hwnd ==
                reinterpret_cast<HWND>(m_Window->winId())) {
        if (nativeMessage->message == WM_ENTERSIZEMOVE) {
            if (m_InteractiveMoveResize) {
                return false;
            }

            m_InteractiveMoveResize = true;
            stop();
        }
        else if (m_InteractiveMoveResize) {
            m_InteractiveMoveResize = false;
            QMetaObject::invokeMethod(
                this, "reevaluate", Qt::QueuedConnection);
        }
    }
#else
    Q_UNUSED(message);
#endif

    return false;
}

void UiFramePacer::handleFrameSwapped()
{
    if (!m_Active || !shouldRun()) {
        reevaluate();
        return;
    }

    const qint64 nowNs = m_Clock.nsecsElapsed();
    updateMeasurement(nowNs);

    if (m_Preferences->uiFramePacingMode ==
            StreamingPreferences::UI_FRAME_PACING_UNBOUNDED) {
        m_Window->update();
    }
    else if (!m_InternalRateLimit) {
        const qreal fps = configuredTargetFps();
        const qreal displayFps =
            m_Screen && m_Screen->refreshRate() > 0.0 ?
                m_Screen->refreshRate() : 60.0;

        // At or above the display rate, request the next frame immediately and
        // let a blocking Present(), V-Sync, or an external limiter pace us.
        // Waiting a full interval here can miss the next VBlank and halve the
        // effective presentation rate.
        if (fps >= displayFps * 0.95) {
            if (m_LastSwapNs != 0) {
                const qint64 targetIntervalNs = qMax<qint64>(
                    1, qRound64(
                        static_cast<qreal>(NanosecondsPerSecond) / fps));
                const qint64 actualIntervalNs = nowNs - m_LastSwapNs;

                if (actualIntervalNs * 4 < targetIntervalNs * 3) {
                    ++m_ConsecutiveFastFrames;
                }
                else {
                    m_ConsecutiveFastFrames = 0;
                }
            }

            m_LastSwapNs = nowNs;

            // Permit a few fast startup frames before concluding that
            // presentation is unthrottled. Once detected, stay internally
            // limited until the mode, screen, or window lifecycle changes.
            if (m_ConsecutiveFastFrames < 3) {
                m_Window->update();
                return;
            }

            m_InternalRateLimit = true;
        }
    }

    scheduleNextFrame();
}

void UiFramePacer::requestFrame()
{
    if (!m_Active || !shouldRun()) {
        reevaluate();
        return;
    }

    m_Window->update();
}

void UiFramePacer::handleModeChanged()
{
    emit targetFpsChanged();

    stop();
    reevaluate();
}

void UiFramePacer::handleScreenChanged(QScreen* screen)
{
    connectScreen(screen);
    emit targetFpsChanged();

    stop();
    reevaluate();
}

void UiFramePacer::reevaluate()
{
    if (shouldRun()) {
        start();
    }
    else {
        stop();
    }
}

bool UiFramePacer::shouldRun() const
{
    if (!m_Window ||
            m_InteractiveMoveResize ||
            m_Preferences->uiFramePacingMode ==
                StreamingPreferences::UI_FRAME_PACING_DISABLED) {
        return false;
    }

    return m_Window->isVisible() &&
           m_Window->isExposed() &&
           m_Window->visibility() != QWindow::Hidden &&
           m_Window->visibility() != QWindow::Minimized;
}

void UiFramePacer::start()
{
    if (m_Active) {
        return;
    }

    m_Clock.restart();
    m_NextDeadlineNs = 0;
    m_LastSwapNs = 0;
    m_MeasurementStartNs = 0;
    m_FramesSinceMeasurement = 0;
    m_ConsecutiveFastFrames = 0;
    m_InternalRateLimit = false;
    setMeasuredFps(0.0);
    m_Active = true;
    emit activeChanged();

    m_Window->update();
}

void UiFramePacer::stop()
{
    m_FrameTimer.stop();
    m_NextDeadlineNs = 0;
    m_LastSwapNs = 0;
    m_MeasurementStartNs = 0;
    m_FramesSinceMeasurement = 0;
    m_ConsecutiveFastFrames = 0;
    m_InternalRateLimit = false;
    setMeasuredFps(0.0);

    if (m_Active) {
        m_Active = false;
        emit activeChanged();
    }
}

void UiFramePacer::scheduleNextFrame()
{
    const qreal fps = configuredTargetFps();
    if (fps <= 0.0) {
        stop();
        return;
    }

    const qint64 intervalNs = qMax<qint64>(
        1, qRound64(static_cast<qreal>(NanosecondsPerSecond) / fps));
    const qint64 nowNs = m_Clock.nsecsElapsed();

    if (m_NextDeadlineNs == 0) {
        m_NextDeadlineNs = nowNs;
    }

    m_NextDeadlineNs += intervalNs;
    if (m_NextDeadlineNs <= nowNs) {
        const qint64 missedIntervals =
            ((nowNs - m_NextDeadlineNs) / intervalNs) + 1;
        m_NextDeadlineNs += missedIntervals * intervalNs;
    }

    const qint64 delayNs = m_NextDeadlineNs - nowNs;

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    m_FrameTimer.setInterval(std::chrono::nanoseconds(delayNs));
    m_FrameTimer.start();
#else
    const int delayMs = qMax(
        1, static_cast<int>((delayNs + 999999LL) / 1000000LL));
    m_FrameTimer.start(delayMs);
#endif
}

void UiFramePacer::updateMeasurement(qint64 nowNs)
{
    if (m_MeasurementStartNs == 0) {
        m_MeasurementStartNs = nowNs;
        m_FramesSinceMeasurement = 0;
    }

    ++m_FramesSinceMeasurement;

    const qint64 elapsedNs = nowNs - m_MeasurementStartNs;
    if (elapsedNs < MeasurementIntervalNs) {
        return;
    }

    setMeasuredFps(
        static_cast<qreal>(m_FramesSinceMeasurement) *
        static_cast<qreal>(NanosecondsPerSecond) /
        static_cast<qreal>(elapsedNs));

    m_MeasurementStartNs = nowNs;
    m_FramesSinceMeasurement = 0;
}

void UiFramePacer::setMeasuredFps(qreal measuredFps)
{
    if (qAbs(m_MeasuredFps - measuredFps) < 0.05) {
        return;
    }

    m_MeasuredFps = measuredFps;
    emit measuredFpsChanged();
}

void UiFramePacer::connectScreen(QScreen* screen)
{
    disconnect(m_ScreenRefreshConnection);
    m_Screen = screen;

    if (m_Screen) {
        m_ScreenRefreshConnection =
            connect(m_Screen, &QScreen::refreshRateChanged, this, [this]() {
                emit targetFpsChanged();
                stop();
                reevaluate();
            });
    }
}

qreal UiFramePacer::configuredTargetFps() const
{
    switch (m_Preferences->uiFramePacingMode) {
    case StreamingPreferences::UI_FRAME_PACING_MATCH_DISPLAY:
        return qBound<qreal>(
            1.0,
            m_Screen && m_Screen->refreshRate() > 0.0 ?
                m_Screen->refreshRate() : 60.0,
            MaximumDisplayRefreshRate);
    case StreamingPreferences::UI_FRAME_PACING_60:
        return 60.0;
    case StreamingPreferences::UI_FRAME_PACING_90:
        return 90.0;
    case StreamingPreferences::UI_FRAME_PACING_120:
        return 120.0;
    case StreamingPreferences::UI_FRAME_PACING_144:
        return 144.0;
    case StreamingPreferences::UI_FRAME_PACING_DISABLED:
    case StreamingPreferences::UI_FRAME_PACING_UNBOUNDED:
    default:
        return 0.0;
    }
}
