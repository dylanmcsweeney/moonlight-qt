#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "backend/identitymanager.h"
#include "backend/nvaddress.h"
#include "backend/nvcomputer.h"
#include "backend/nvhttp.h"
#include "backend/nvpairingmanager.h"

extern "C" const char* LiGetLaunchUrlQueryParameters(void)
{
    return "";
}

class BackendReliabilityTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_SettingsDirectory;

    bool configureIsolatedSettings()
    {
        if (!m_SettingsDirectory.isValid()) {
            return false;
        }

        QCoreApplication::setOrganizationName(QStringLiteral("MoonlightBackendBatch2Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("BackendReliability"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,
                           QSettings::UserScope,
                           m_SettingsDirectory.path());
        return true;
    }

    static void sendHttpResponseWhenRequestArrives(QTcpServer& server,
                                                    const QByteArray& body,
                                                    int delayMs = 0)
    {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, body, delayMs]() {
            while (server.hasPendingConnections()) {
                QTcpSocket* socket = server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                 [socket, body, delayMs]() {
                    if (socket->property("backendTestReplied").toBool()) {
                        socket->readAll();
                        return;
                    }

                    if (socket->readAll().isEmpty()) {
                        return;
                    }

                    socket->setProperty("backendTestReplied", true);
                    QByteArray headers = "HTTP/1.1 200 OK\r\nContent-Length: " +
                                         QByteArray::number(body.size()) +
                                         "\r\nConnection: close\r\n\r\n";
                    socket->write(headers);
                    if (delayMs == 0) {
                        socket->write(body);
                        socket->disconnectFromHost();
                    }
                    else {
                        QTimer::singleShot(delayMs, socket, [socket, body]() {
                            if (socket->state() != QAbstractSocket::UnconnectedState) {
                                socket->write(body);
                                socket->disconnectFromHost();
                            }
                        });
                    }
                });
            }
        });
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(m_SettingsDirectory.isValid(), "Could not create isolated settings directory");
        QVERIFY(configureIsolatedSettings());
    }

    void missingAppVersionFailsPairingGracefully()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        sendHttpResponseWhenRequestArrives(server,
            QByteArrayLiteral("<root status_code=\"200\"><paired>0</paired></root>"));

        NvComputer computer{};
        computer.activeAddress = NvAddress(QStringLiteral("127.0.0.1"), server.serverPort());
        computer.activeHttpsPort = server.serverPort();
        computer.isNvidiaServerSoftware = true;

        NvPairingManager pairing(&computer);
        QSslCertificate serverCertificate;
        QCOMPARE(pairing.pair(QString(), QStringLiteral("1234"), serverCertificate),
                 NvPairingManager::FAILED);
    }

    void requestSentStopsConnectionTimerForSlowResponse()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        sendHttpResponseWhenRequestArrives(server, QByteArrayLiteral("done"), 5500);

        QNetworkAccessManager nam;
        NvHTTP http(NvAddress(QStringLiteral("127.0.0.1"), server.serverPort()),
                    server.serverPort(), QSslCertificate(), false, &nam);

        const QString response = http.openConnectionToString(http.m_BaseUrlHttp,
                                                              QStringLiteral("slow"),
                                                              QString(),
                                                              10000,
                                                              NvHTTP::NVLL_NONE);
        QCOMPARE(response, QStringLiteral("done"));
    }

    void tlsHandshakeTimeoutIsReportedAsConnectionTimeout()
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
        QSKIP("The TCP/TLS connection timeout is available on Qt 6.3 and newer");
#else
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&server]() {
            while (server.hasPendingConnections()) {
                // Accept TCP and deliberately leave the TLS handshake unanswered.
                server.nextPendingConnection();
            }
        });

        QNetworkAccessManager nam;
        NvHTTP http(NvAddress(QStringLiteral("127.0.0.1"), server.serverPort()),
                    server.serverPort(), QSslCertificate(), false, &nam);

        QElapsedTimer elapsed;
        elapsed.start();
        bool timedOut = false;
        try {
            http.openConnectionToString(http.m_BaseUrlHttps,
                                        QStringLiteral("stall"),
                                        QString(),
                                        15000,
                                        NvHTTP::NVLL_NONE);
        }
        catch (const QtNetworkReplyException& error) {
            QCOMPARE(error.getError(), QNetworkReply::TimeoutError);
            QCOMPARE(QString::fromUtf8(error.getErrorText()), QStringLiteral("Connection timed out"));
            timedOut = true;
        }

        QVERIFY(timedOut);
        QVERIFY(elapsed.elapsed() >= 4500);
        QVERIFY(elapsed.elapsed() < 12000);
#endif
    }
};

QTEST_GUILESS_MAIN(BackendReliabilityTest)

#include "tst_backendreliability.moc"
