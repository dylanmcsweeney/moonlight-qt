#include <QFileInfo>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest>

#include "displaybounds.h"
#include "logfile.h"
#include "streaming/input/steamcontroller.h"

class LifecycleBatch3Tests : public QObject
{
    Q_OBJECT

private slots:
    void displayBoundsMustDifferFromFullBounds();
    void privateLogFileUsesUniqueProcessName();
    void existingLogFileIsNotOpenedOrTruncated();
    void knownSteamControllerProductsAreRecognized();
};

void LifecycleBatch3Tests::displayBoundsMustDifferFromFullBounds()
{
    struct Display {
        QRect full;
        QRect usable;
        bool querySucceeded;
    };

    const QVector<Display> fullBoundsOnly = {
        {QRect(0, 0, 1920, 1080), QRect(0, 0, 1920, 1080), true},
        {QRect(1920, 0, 2560, 1440), QRect(1920, 0, 2560, 1440), true},
    };
    auto hasUsableBounds = [](const QVector<Display>& displays) {
        return WMUtils::hasAnyDisplayUsableBounds(displays.size(),
                [&displays](int index, QRect* full, QRect* usable) {
            const Display& display = displays.at(index);
            if (!display.querySucceeded) {
                return false;
            }
            *full = display.full;
            *usable = display.usable;
            return true;
        });
    };

    QVERIFY(!hasUsableBounds(fullBoundsOnly));

    QVector<Display> oneDisplayReservesTaskbar = fullBoundsOnly;
    oneDisplayReservesTaskbar[1].usable.setHeight(1400);
    QVERIFY(hasUsableBounds(oneDisplayReservesTaskbar));

    QVector<Display> failedQueries = fullBoundsOnly;
    failedQueries[0].querySucceeded = false;
    failedQueries[1].querySucceeded = false;
    QVERIFY(!hasUsableBounds(failedQueries));
}

void LifecycleBatch3Tests::privateLogFileUsesUniqueProcessName()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    auto first = Utils::createPrivateLogFile(directory.path(), 1234, 100);
    auto second = Utils::createPrivateLogFile(directory.path(), 1234, 101);
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(first->fileName() != second->fileName());
    QVERIFY(QFileInfo(first->fileName()).fileName().startsWith("Moonlight-1234-"));
    QVERIFY(QFileInfo(first->fileName()).fileName().endsWith(".log"));

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    const QFileDevice::Permissions permissions = QFileInfo(first->fileName()).permissions();
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
#if defined(Q_OS_UNIX)
    QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                             QFileDevice::ExeGroup | QFileDevice::ReadOther |
                             QFileDevice::WriteOther | QFileDevice::ExeOther)));
#endif
#endif
}

void LifecycleBatch3Tests::existingLogFileIsNotOpenedOrTruncated()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString existingPath = QDir(directory.path()).filePath("Moonlight-4321-77.log");
    QFile existing(existingPath);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write("keep this log"), qint64(13));
    existing.close();

    QVERIFY(!Utils::createPrivateLogFile(directory.path(), 4321, 77));

    QFile preserved(existingPath);
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), QByteArray("keep this log"));
}

void LifecycleBatch3Tests::knownSteamControllerProductsAreRecognized()
{
    constexpr uint16_t steamControllerProducts[] = {
        0x1101, 0x1102, 0x1105, 0x1106, 0x1142,
        0x1201, 0x1202, 0x1205, 0x1302, 0x1303,
        0x1304, 0x1305,
    };
    for (uint16_t productId : steamControllerProducts) {
        QVERIFY2(GamepadUtils::isKnownSteamController(0x28de, productId),
                 qPrintable(QString("Steam controller product 0x%1 was not recognized")
                            .arg(productId, 4, 16, QLatin1Char('0'))));
    }

    QVERIFY(!GamepadUtils::isKnownSteamController(0x046d, 0x1101));
    QVERIFY(!GamepadUtils::isKnownSteamController(0x28de, 0x9999));
}

QTEST_GUILESS_MAIN(LifecycleBatch3Tests)
#include "tst_lifecyclebatch3.moc"
