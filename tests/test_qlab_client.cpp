#include "app/QLabClient.h"
#include <QSignalSpy>
#include <QStringList>
#include <QtTest/QtTest>
#include <lo/lo.h>

using namespace OpenMix;

namespace {
struct Recorder {
    QStringList paths;
};
int record(const char* path, const char*, lo_arg**, int, lo_message, void* user) {
    static_cast<Recorder*>(user)->paths.append(QString::fromUtf8(path));
    return 0;
}
} // namespace

class TestQLabClient : public QObject {
    Q_OBJECT

  private slots:
    void triggerCueSendsStart() {
        Recorder rec;
        lo_server_thread srv = lo_server_thread_new(nullptr, nullptr);
        QVERIFY(srv);
        lo_server_thread_add_method(srv, nullptr, nullptr, record, &rec);
        lo_server_thread_start(srv);

        QLabClient client;
        client.setTarget("127.0.0.1", lo_server_thread_get_port(srv));
        client.setEnabled(true);

        QSignalSpy sent(&client, &QLabClient::sent);
        client.triggerCue("5"); // pre-roll 0 -> emitted synchronously

        QCOMPARE(sent.count(), 1);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/cue/5/start"));
        QTRY_VERIFY(rec.paths.contains("/cue/5/start")); // UDP receipt is async

        lo_server_thread_stop(srv);
        lo_server_thread_free(srv);
    }

    void disabledClientSendsNothing() {
        QLabClient client;
        client.setEnabled(false);
        QSignalSpy sent(&client, &QLabClient::sent);
        client.triggerCue("3");
        QTest::qWait(50);
        QCOMPARE(sent.count(), 0);
    }

    void workspacePrefixedAddress() {
        Recorder rec;
        lo_server_thread srv = lo_server_thread_new(nullptr, nullptr);
        QVERIFY(srv);
        lo_server_thread_add_method(srv, nullptr, nullptr, record, &rec);
        lo_server_thread_start(srv);

        QLabClient client;
        client.setTarget("127.0.0.1", lo_server_thread_get_port(srv));
        client.setWorkspaceId("abc");
        client.setEnabled(true);
        client.triggerCue("12");

        QTRY_VERIFY(rec.paths.contains("/workspace/abc/cue/12/start"));

        lo_server_thread_stop(srv);
        lo_server_thread_free(srv);
    }

    void passcodeSendsConnectBeforeCommand() {
        Recorder rec;
        lo_server_thread srv = lo_server_thread_new(nullptr, nullptr);
        QVERIFY(srv);
        lo_server_thread_add_method(srv, nullptr, nullptr, record, &rec);
        lo_server_thread_start(srv);

        QLabClient client;
        client.setTarget("127.0.0.1", lo_server_thread_get_port(srv));
        client.setPasscode("secret");
        client.setEnabled(true);
        client.triggerCue("7");

        QTRY_VERIFY(rec.paths.contains("/connect"));
        QTRY_VERIFY(rec.paths.contains("/cue/7/start"));

        // /connect is sent only once per target
        rec.paths.clear();
        client.triggerCue("8");
        QTRY_VERIFY(rec.paths.contains("/cue/8/start"));
        QVERIFY(!rec.paths.contains("/connect"));

        lo_server_thread_stop(srv);
        lo_server_thread_free(srv);
    }

    void suppressBackGatesBack() {
        Recorder rec;
        lo_server_thread srv = lo_server_thread_new(nullptr, nullptr);
        QVERIFY(srv);
        lo_server_thread_add_method(srv, nullptr, nullptr, record, &rec);
        lo_server_thread_start(srv);

        QLabClient client;
        client.setTarget("127.0.0.1", lo_server_thread_get_port(srv));
        client.setEnabled(true);

        client.setSuppressBack(true);
        client.back();
        QTest::qWait(50);
        QVERIFY(!rec.paths.contains("/playhead/previous"));

        client.setSuppressBack(false);
        client.back();
        QTRY_VERIFY(rec.paths.contains("/playhead/previous"));

        lo_server_thread_stop(srv);
        lo_server_thread_free(srv);
    }

    void transportVerbsSendAddresses() {
        QLabClient client;
        client.setTarget("127.0.0.1", 53000);
        client.setEnabled(true);

        QSignalSpy sent(&client, &QLabClient::sent);
        client.panic();
        client.stop();
        client.pause();
        client.resume();

        QCOMPARE(sent.count(), 4);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/panic"));
        QCOMPARE(sent.at(1).at(0).toString(), QString("/stop"));
        QCOMPARE(sent.at(2).at(0).toString(), QString("/pause"));
        QCOMPARE(sent.at(3).at(0).toString(), QString("/resume"));
    }

    void workspacePrefixesTransport() {
        QLabClient client;
        client.setTarget("127.0.0.1", 53000);
        client.setEnabled(true);
        client.setWorkspaceId("ABC");

        QSignalSpy sent(&client, &QLabClient::sent);
        client.panic();
        QCOMPARE(sent.at(0).at(0).toString(), QString("/workspace/ABC/panic"));
    }
};

QTEST_MAIN(TestQLabClient)
#include "test_qlab_client.moc"
