#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <cstdlib>

#include "../../app/settings/uistartuprecovery.h"

class UiStartupRecoveryTest : public QObject
{
    Q_OBJECT

private:
    int launchCrashChild(const QString& mode, const QString& directory)
    {
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {mode, directory});
        if (!child.waitForStarted(5000) || !child.waitForFinished(10000)) {
            return -1;
        }
        return child.exitCode();
    }

private slots:
    void deadDx12AttemptLatchesRecovery();
    void automaticAttemptUsesRendererBackend();
    void liveAttemptIsNotMisclassified();
    void cleanExitDoesNotLatchRecovery();
    void cleanMarkerSurvivesJournalLockFailure();
    void safeUiPreservesUnconsumedFailure();
    void safeUiLeavesMalformedJournalUntouched();
    void explicitRetryCrashRelatches();
    void explicitEnvironmentCannotBypassRecovery();
    void savedD3D11CannotClearExplicitD3D12Failure();
    void corruptStateUsesSafeFallback();
    void malformedAttemptIdIsRejected();
    void journalIoFailureIsVisibleAndSafe();
};

void UiStartupRecoveryTest::deadDx12AttemptLatchesRecovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(launchCrashChild(QStringLiteral("--crash-dx12"), directory.path()), 71);

    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Direct3D12,
                               true);
    QVERIFY2(recovery.recoveryActive(), qPrintable(recovery.warning()));
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    QVERIFY(recovery.warning().isEmpty());
    QVERIFY(recovery.statusMessage().contains(QStringLiteral("saved graphics preference")));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::automaticAttemptUsesRendererBackend()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(launchCrashChild(QStringLiteral("--crash-auto-dx12"), directory.path()), 74);

    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Automatic,
                               true);
    QVERIFY(recovery.recoveryActive());
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::liveAttemptIsNotMisclassified()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    UiStartupRecovery first(directory.path(), UiStartupRecovery::Direct3D12,
                            true);
    QVERIFY(first.attemptId().size() > 10);
    UiStartupRecovery second(directory.path(), UiStartupRecovery::Direct3D12,
                             true);
    QVERIFY(!second.recoveryActive());
    QCOMPARE(second.effectiveBackend(), int(UiStartupRecovery::Direct3D12));
    second.finishCleanly();
    first.finishCleanly();
}

void UiStartupRecoveryTest::cleanExitDoesNotLatchRecovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    {
        UiStartupRecovery attempt(directory.path(), UiStartupRecovery::Direct3D12,
                                  true);
        attempt.stage(QStringLiteral("renderer-configured"));
        attempt.finishCleanly();
    }
    UiStartupRecovery next(directory.path(), UiStartupRecovery::Direct3D12,
                           true);
    QVERIFY(!next.recoveryActive());
    QCOMPARE(next.effectiveBackend(), int(UiStartupRecovery::Direct3D12));
    next.finishCleanly();
}

void UiStartupRecoveryTest::cleanMarkerSurvivesJournalLockFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    UiStartupRecovery attempt(directory.path(), UiStartupRecovery::Direct3D12,
                              true);
    QLockFile otherWriter(QDir(directory.path()).absoluteFilePath(
        QStringLiteral("ui-startup-recovery/journal.lock")));
    QVERIFY(otherWriter.tryLock(0));

    attempt.finishCleanly();
    QVERIFY(QFileInfo::exists(QDir(directory.path()).absoluteFilePath(
        QStringLiteral("ui-startup-recovery/attempt-%1.clean")
            .arg(attempt.attemptId()))));
    otherWriter.unlock();

    UiStartupRecovery next(directory.path(), UiStartupRecovery::Direct3D12,
                           true);
    QVERIFY(!next.recoveryActive());
    QCOMPARE(next.effectiveBackend(), int(UiStartupRecovery::Direct3D12));
    next.finishCleanly();
}

void UiStartupRecoveryTest::safeUiPreservesUnconsumedFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(launchCrashChild(QStringLiteral("--crash-dx12"), directory.path()), 71);
    {
        UiStartupRecovery safe(directory.path(), UiStartupRecovery::Direct3D12,
                               true, true);
        QVERIFY(safe.safeUiActive());
        QVERIFY(!safe.recoveryActive());
        QCOMPARE(safe.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
        safe.finishCleanly();
    }

    UiStartupRecovery normal(directory.path(), UiStartupRecovery::Direct3D12,
                             true);
    QVERIFY(normal.recoveryActive());
    QCOMPARE(normal.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    normal.finishCleanly();
}

void UiStartupRecoveryTest::safeUiLeavesMalformedJournalUntouched()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString stateDirectory = QDir(directory.path()).absoluteFilePath(
        QStringLiteral("ui-startup-recovery"));
    QVERIFY(QDir().mkpath(stateDirectory));
    const QString statePath = QDir(stateDirectory).absoluteFilePath(
        QStringLiteral("state.json"));
    const QByteArray original("{ malformed");
    QFile state(statePath);
    QVERIFY(state.open(QIODevice::WriteOnly));
    QCOMPARE(state.write(original), qint64(original.size()));
    state.close();

    UiStartupRecovery safe(directory.path(), UiStartupRecovery::Direct3D12,
                           true, true);
    QVERIFY(!safe.warning().isEmpty());
    QCOMPARE(safe.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    safe.finishCleanly();

    QFile preserved(statePath);
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), original);
}

void UiStartupRecoveryTest::explicitRetryCrashRelatches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(launchCrashChild(QStringLiteral("--crash-dx12"), directory.path()), 71);
    QCOMPARE(launchCrashChild(QStringLiteral("--retry-crash"), directory.path()), 72);

    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Direct3D12,
                               true);
    QVERIFY(recovery.recoveryActive());
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::explicitEnvironmentCannotBypassRecovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(launchCrashChild(QStringLiteral("--crash-dx12"), directory.path()), 71);
    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Direct3D12,
                               true, false, UiStartupRecovery::Direct3D12);
    QVERIFY(recovery.recoveryActive());
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::savedD3D11CannotClearExplicitD3D12Failure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(launchCrashChild(QStringLiteral("--crash-dx12"), directory.path()), 71);

    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Direct3D11,
                               true, false, UiStartupRecovery::Direct3D12);
    QVERIFY(recovery.recoveryActive());
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::corruptStateUsesSafeFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString stateDirectory = QDir(directory.path()).absoluteFilePath(
        QStringLiteral("ui-startup-recovery"));
    QVERIFY(QDir().mkpath(stateDirectory));
    QFile state(QDir(stateDirectory).absoluteFilePath(QStringLiteral("state.json")));
    QVERIFY(state.open(QIODevice::WriteOnly));
    QCOMPARE(state.write("{ malformed"), qint64(11));
    state.close();

    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Direct3D12,
                               true);
    QVERIFY(!recovery.warning().isEmpty());
    QVERIFY(recovery.recoveryActive());
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::malformedAttemptIdIsRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString stateDirectory = QDir(directory.path()).absoluteFilePath(
        QStringLiteral("ui-startup-recovery"));
    QVERIFY(QDir().mkpath(stateDirectory));
    const QJsonObject attempt{
        {QStringLiteral("id"), QStringLiteral("../../outside")},
        {QStringLiteral("backend"), int(UiStartupRecovery::Direct3D12)},
        {QStringLiteral("stage"), QStringLiteral("renderer-initialization")},
        {QStringLiteral("startedAt"), QStringLiteral("2026-10-21T00:00:00.000Z")}
    };
    const QJsonObject root{
        {QStringLiteral("version"), 1},
        {QStringLiteral("recoveryActive"), false},
        {QStringLiteral("failedBackend"), int(UiStartupRecovery::Automatic)},
        {QStringLiteral("failedAttempt"), QString()},
        {QStringLiteral("attempts"), QJsonArray{attempt}}
    };
    QFile state(QDir(stateDirectory).absoluteFilePath(QStringLiteral("state.json")));
    QVERIFY(state.open(QIODevice::WriteOnly));
    const QByteArray original = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QCOMPARE(state.write(original), qint64(original.size()));
    state.close();

    UiStartupRecovery recovery(directory.path(), UiStartupRecovery::Direct3D12,
                               true);
    QVERIFY(!recovery.warning().isEmpty());
    QVERIFY(recovery.recoveryActive());
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
    recovery.finishCleanly();
}

void UiStartupRecoveryTest::journalIoFailureIsVisibleAndSafe()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsFile = QDir(directory.path()).absoluteFilePath(
        QStringLiteral("settings-file"));
    QFile file(settingsFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a directory");
    file.close();

    UiStartupRecovery recovery(settingsFile, UiStartupRecovery::Direct3D12,
                               true);
    QVERIFY(!recovery.warning().isEmpty());
#ifdef Q_OS_WIN
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D11));
#else
    QCOMPARE(recovery.effectiveBackend(), int(UiStartupRecovery::Direct3D12));
#endif
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc == 3 && QByteArray(argv[1]) == "--crash-dx12") {
        UiStartupRecovery recovery(QString::fromLocal8Bit(argv[2]),
                                   UiStartupRecovery::Direct3D12, true);
        recovery.stage(QStringLiteral("renderer-initialization"));
        std::_Exit(71);
    }
    if (argc == 3 && QByteArray(argv[1]) == "--retry-crash") {
        UiStartupRecovery recovery(QString::fromLocal8Bit(argv[2]),
                                   UiStartupRecovery::Direct3D12, true,
                                   false, -1, true);
        if (recovery.effectiveBackend() != UiStartupRecovery::Direct3D12 ||
                recovery.recoveryActive()) {
            std::_Exit(73);
        }
        std::_Exit(72);
    }
    if (argc == 3 && QByteArray(argv[1]) == "--crash-auto-dx12") {
        UiStartupRecovery recovery(QString::fromLocal8Bit(argv[2]),
                                   UiStartupRecovery::Automatic, true);
        recovery.recordActualBackend(UiStartupRecovery::Direct3D12);
        std::_Exit(74);
    }
    UiStartupRecoveryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_uistartuprecovery.moc"
