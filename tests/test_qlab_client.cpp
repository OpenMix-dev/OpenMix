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
};

QTEST_MAIN(TestQLabClient)
#include "test_qlab_client.moc"
