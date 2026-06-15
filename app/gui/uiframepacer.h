#pragma once

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QChronoTimer>
using UiFrameTimer = QChronoTimer;
#else
#include <QTimer>
using UiFrameTimer = QTimer;
#endif

class QQuickWindow;
class QScreen;
class StreamingPreferences;

class UiFramePacer : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

    Q_PROPERTY(qreal measuredFps READ measuredFps NOTIFY measuredFpsChanged)
    Q_PROPERTY(qreal targetFps READ targetFps NOTIFY targetFpsChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit UiFramePacer(StreamingPreferences* preferences,
                          QObject* parent = nullptr);
    ~UiFramePacer() override;

    void setWindow(QQuickWindow* window);

    qreal measuredFps() const;
    qreal targetFps() const;
    bool active() const;

signals:
    void measuredFpsChanged();
    void targetFpsChanged();
    void activeChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray& eventType,
                           void* message,
                           qintptr* result) override;
#else
    bool nativeEventFilter(const QByteArray& eventType,
                           void* message,
                           long* result) override;
#endif

private slots:
    void handleFrameSwapped();
    void requestFrame();
    void handleModeChanged();
    void handleScreenChanged(QScreen* screen);
    void reevaluate();

private:
    bool shouldRun() const;
    void start();
    void stop();
    void scheduleNextFrame();
    void updateMeasurement(qint64 nowNs);
    void setMeasuredFps(qreal measuredFps);
    void connectScreen(QScreen* screen);
    qreal configuredTargetFps() const;

    StreamingPreferences* m_Preferences;
    QPointer<QQuickWindow> m_Window;
    QPointer<QScreen> m_Screen;
    QMetaObject::Connection m_ScreenRefreshConnection;

    UiFrameTimer m_FrameTimer;

    QElapsedTimer m_Clock;
    qint64 m_NextDeadlineNs;
    qint64 m_LastSwapNs;
    qint64 m_MeasurementStartNs;
    quint64 m_FramesSinceMeasurement;
    qreal m_MeasuredFps;
    int m_ConsecutiveFastFrames;
    bool m_InternalRateLimit;
    bool m_InteractiveMoveResize;
    bool m_Active;
};
