#include "protocol/discovery/ConsoleDiscoveryService.h"
#include "protocol/discovery/probes/AllenHeathProbeStrategy.h"
#include "protocol/discovery/probes/BehringerWingProbeStrategy.h"
#include "protocol/discovery/probes/BehringerX32ProbeStrategy.h"
#include "protocol/discovery/probes/YamahaYsdpProbeStrategy.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {

// builds an OSC message with string arguments, mirroring console replies
QByteArray buildOscReply(const QByteArray& path, const QList<QByteArray>& stringArgs) {
    auto pad = [](QByteArray bytes) {
        bytes.append('\0');
        while (bytes.size() % 4 != 0) {
            bytes.append('\0');
        }
        return bytes;
    };

    QByteArray msg = pad(path);

    QByteArray typeTag = ",";
    for (int i = 0; i < stringArgs.size(); ++i) {
        typeTag.append('s');
    }
    msg.append(pad(typeTag));

    for (const QByteArray& arg : stringArgs) {
        msg.append(pad(arg));
    }

    return msg;
}

// builds a YSDP discovery reply: service name then length-prefixed fields
QByteArray buildYsdpReply(const QByteArray& host, const QByteArray& model,
                          const QByteArray& version) {
    QByteArray reply;
    reply.append(4, '\x00'); // arbitrary header bytes before the service name
    reply.append("_ypa-scp", 8);
    reply.append(2, '\x00'); // 2 bytes between service name and fields

    auto appendField = [&reply](const QByteArray& field) {
        reply.append(static_cast<char>(field.size()));
        reply.append(field);
    };
    appendField(host);
    appendField(model);
    appendField(version);

    return reply;
}

DiscoveredConsole parseWing(const QByteArray& reply) {
    BehringerWingProbeStrategy strategy;
    return strategy.parseRawResponse(reply, QHostAddress("192.168.1.70"), 2222);
}

// builds an Allen & Heath TCP identify response frame:
// [7F 02 00 00 00 00][family][vMaj][vMin][vPatch][rev lo][rev hi]
QByteArray buildAhIdentify(quint8 family, quint8 maj, quint8 min, quint8 patch,
                           quint16 revision = 0) {
    QByteArray frame = QByteArray::fromHex("7f0200000000");
    frame.append(static_cast<char>(family));
    frame.append(static_cast<char>(maj));
    frame.append(static_cast<char>(min));
    frame.append(static_cast<char>(patch));
    frame.append(static_cast<char>(revision & 0xFF));
    frame.append(static_cast<char>((revision >> 8) & 0xFF));
    return frame;
}

DiscoveredConsole parseAh(const QByteArray& frame) {
    AllenHeathProbeStrategy strategy;
    return strategy.parseIdentifyResponse(frame, QHostAddress("192.168.1.80"),
                                          AllenHeathProbeStrategy::SQ_IDENTIFY_PORT);
}

// builds an ACE SysEx identify reply: [F0 00 + 9 header bytes][identity ASCII][F7]
QByteArray buildAceReply(const QByteArray& identity) {
    QByteArray frame = QByteArray::fromHex("f0000100000001000400"); // 10 bytes
    frame.append('\x00');                                           // 11th header byte
    frame.append(identity);
    frame.append('\xf7');
    return frame;
}

// what a console answers a by-name request with: the object's handle at [11..12].
// The identity string only comes back when that handle is read.
QByteArray buildAceHandleReply(const QByteArray& handle) {
    QByteArray frame = QByteArray::fromHex("f0000100000001000200");
    frame.append('\x02');
    frame.append(handle);
    frame.append('\xf7');
    return frame;
}

DiscoveredConsole parseAce(const QByteArray& identity) {
    AllenHeathProbeStrategy strategy;
    return strategy.parseIdentifyResponse(buildAceReply(identity), QHostAddress("192.168.1.81"),
                                          AllenHeathProbeStrategy::ACE_IDENTIFY_PORT);
}

} // namespace

class TestConsoleDiscovery : public QObject {
    Q_OBJECT

  private slots:
    void wingReply_fullsize() {
        const auto console =
            parseWing("WING,192.168.1.70,FOH Wing,wing-fullsize,ABC1234567,3.1.2-50");
        QVERIFY(console.isValid());
        QCOMPARE(console.type, ConsoleType::Wing);
        QCOMPARE(console.port, 2223);
        QCOMPARE(console.modelName, QString("wing-fullsize"));
        QCOMPARE(console.firmwareVersion, QString("3.1.2-50"));
        QCOMPARE(console.displayName, QString("Behringer WING (FOH Wing)"));
        QCOMPARE(console.manufacturer, Manufacturer::Behringer);
    }

    void wingReply_compactAndRack() {
        QCOMPARE(parseWing("WING,10.0.0.5,,wing-compact,X,1.0").type, ConsoleType::Wing);
        QCOMPARE(parseWing("WING,10.0.0.6,,wing-rack,X,1.0").type, ConsoleType::Wing);
    }

    void wingReply_emptyName_usesPlainDisplayName() {
        const auto console = parseWing("WING,10.0.0.5,,wing-compact,X,1.0");
        QCOMPARE(console.displayName, QString("Behringer WING"));
    }

    void wingReply_tooFewFields_isInvalid() {
        QVERIFY(!parseWing("WING,192.168.1.70,Name").isValid());
        QVERIFY(!parseWing("WING,").isValid());
    }

    void wingProbe_isRawOnPort2222() {
        BehringerWingProbeStrategy strategy;
        QCOMPARE(strategy.probePort(), 2222);
        QCOMPARE(strategy.rawProbe(QHostAddress("192.168.1.2")), QByteArray("WING?"));
        QVERIFY(strategy.canParseRawResponse("WING,anything"));
        QVERIFY(!strategy.canParseRawResponse("/xinfo"));
    }

    void x32Reply_identifiesModelFromThirdArg() {
        BehringerX32ProbeStrategy strategy;
        const QVariantList args{"192.168.1.30", "MyDesk", "X32 RACK", "4.06"};
        const auto console =
            strategy.parseResponse("/xinfo", args, QHostAddress("192.168.1.30"), 10023);
        QVERIFY(console.isValid());
        QCOMPARE(console.type, ConsoleType::X32);
        QCOMPARE(console.modelName, QString("X32 RACK"));
        QCOMPARE(console.firmwareVersion, QString("4.06"));
        QCOMPARE(console.port, 10023);
    }

    void x32Reply_detectsM32() {
        BehringerX32ProbeStrategy strategy;
        const QVariantList args{"192.168.1.31", "Monitors", "M32C", "4.11"};
        const auto console =
            strategy.parseResponse("/xinfo", args, QHostAddress("192.168.1.31"), 10023);
        QCOMPARE(console.type, ConsoleType::M32);
    }

    void x32Reply_missingArgs_isInvalid() {
        BehringerX32ProbeStrategy strategy;
        const auto console =
            strategy.parseResponse("/xinfo", {}, QHostAddress("192.168.1.31"), 10023);
        QVERIFY(!console.isValid());
    }

    void yamahaReply_mapsModels() {
        YamahaYsdpProbeStrategy strategy;
        const QHostAddress sender("192.168.1.40");

        struct Case {
            QByteArray model;
            ConsoleType type;
        };
        const QList<Case> cases{{"Yamaha TF5", ConsoleType::TF5},
                                {"Yamaha QL5", ConsoleType::QL5},
                                {"Yamaha CL5", ConsoleType::CL5},
                                {"Yamaha DM7", ConsoleType::DM7},
                                {"DM7C", ConsoleType::DM7}};

        for (const Case& c : cases) {
            const auto console = strategy.parseRawResponse(
                buildYsdpReply("console-host", c.model, "V5.10"), sender, 54330);
            QVERIFY2(console.isValid(), c.model.constData());
            QCOMPARE(console.type, c.type);
            QCOMPARE(console.port, 49280);
            QCOMPARE(console.firmwareVersion, QString("V5.10"));
        }
    }

    void yamahaReply_shortHostField_isInvalid() {
        YamahaYsdpProbeStrategy strategy;
        const auto console = strategy.parseRawResponse(buildYsdpReply("ab", "Yamaha TF5", "V5.10"),
                                                       QHostAddress("192.168.1.40"), 54330);
        QVERIFY(!console.isValid());
    }

    void yamahaReply_unknownModel_isInvalid() {
        YamahaYsdpProbeStrategy strategy;
        const auto console = strategy.parseRawResponse(
            buildYsdpReply("console-host", "Mystery", "V1.0"), QHostAddress("192.168.1.40"), 54330);
        QVERIFY(!console.isValid());
    }

    void yamahaProbe_embedsLocalAddress() {
        YamahaYsdpProbeStrategy strategy;
        const QByteArray probe = strategy.rawProbe(QHostAddress("192.168.1.2"));
        QCOMPARE(probe.size(), 41);
        QVERIFY(probe.startsWith("YSDP"));
        QCOMPARE(static_cast<quint8>(probe.at(8)), quint8(192));
        QCOMPARE(static_cast<quint8>(probe.at(9)), quint8(168));
        QCOMPARE(static_cast<quint8>(probe.at(10)), quint8(1));
        QCOMPARE(static_cast<quint8>(probe.at(11)), quint8(2));
        QVERIFY(probe.contains("_ypa-scp"));
    }

    void service_routesRawWingReply() {
        ConsoleDiscoveryService service;
        service.registerStrategy(std::make_shared<BehringerX32ProbeStrategy>());
        service.registerStrategy(std::make_shared<BehringerWingProbeStrategy>());
        service.registerStrategy(std::make_shared<YamahaYsdpProbeStrategy>());

        int discovered = 0;
        DiscoveredConsole last;
        connect(&service, &ConsoleDiscoveryService::consoleDiscovered,
                [&](const DiscoveredConsole& console) {
                    ++discovered;
                    last = console;
                });

        const QByteArray reply = "WING,192.168.1.70,FOH,wing-fullsize,SN1,3.1";
        service.processDatagram(reply, QHostAddress("192.168.1.70"), 2222);
        QCOMPARE(discovered, 1);
        QCOMPARE(last.type, ConsoleType::Wing);

        // duplicate reply must not re-emit
        service.processDatagram(reply, QHostAddress("192.168.1.70"), 2222);
        QCOMPARE(discovered, 1);
        QCOMPARE(service.discoveredConsoles().size(), 1);
    }

    void service_routesOscXinfoReply() {
        ConsoleDiscoveryService service;
        service.registerStrategy(std::make_shared<BehringerX32ProbeStrategy>());
        service.registerStrategy(std::make_shared<BehringerWingProbeStrategy>());

        int discovered = 0;
        DiscoveredConsole last;
        connect(&service, &ConsoleDiscoveryService::consoleDiscovered,
                [&](const DiscoveredConsole& console) {
                    ++discovered;
                    last = console;
                });

        const QByteArray reply =
            buildOscReply("/xinfo", {"192.168.1.30", "MyDesk", "M32 LIVE", "4.11"});
        service.processDatagram(reply, QHostAddress("192.168.1.30"), 10023);
        QCOMPARE(discovered, 1);
        QCOMPARE(last.type, ConsoleType::M32);
        QCOMPARE(last.modelName, QString("M32 LIVE"));
    }

    void service_ignoresJunkDatagrams() {
        ConsoleDiscoveryService service;
        service.registerStrategy(std::make_shared<BehringerWingProbeStrategy>());
        service.registerStrategy(std::make_shared<YamahaYsdpProbeStrategy>());

        int discovered = 0;
        connect(&service, &ConsoleDiscoveryService::consoleDiscovered,
                [&](const DiscoveredConsole&) { ++discovered; });

        service.processDatagram(QByteArray(), QHostAddress("10.0.0.1"), 1234);
        service.processDatagram("garbage", QHostAddress("10.0.0.1"), 1234);
        service.processDatagram("/unknown/path", QHostAddress("10.0.0.1"), 1234);
        service.processDatagram("WING,short", QHostAddress("10.0.0.1"), 2222);
        QCOMPARE(discovered, 0);
    }

    void ahProbe_sendsFivePerFamilyFinds() {
        AllenHeathProbeStrategy strategy;
        QCOMPARE(strategy.probePort(), 51320);
        const QList<QByteArray> finds = strategy.rawProbes(QHostAddress("192.168.1.2"));
        QCOMPARE(finds.size(), 5);
        QVERIFY(finds.contains(QByteArray("SQ Find")));
        QVERIFY(finds.contains(QByteArray("Qu-567 Find")));
        QVERIFY(finds.contains(QByteArray("GLD Find")));
        QVERIFY(finds.contains(QByteArray("Bridge Find")));
        QVERIFY(finds.contains(QByteArray("TLD Find")));
    }

    void ahIdentify_descriptors() {
        AllenHeathProbeStrategy strategy;
        const QList<TcpIdentify> ids = strategy.tcpIdentifies();
        QCOMPARE(ids.size(), 2);

        // SQ binary identify on 51326
        const TcpIdentify& sq = ids.at(0);
        QVERIFY(sq.isValid());
        QCOMPARE(sq.port, 51326);
        QCOMPARE(sq.request, QByteArray::fromHex("7f0100000000"));

        // ACE SysEx "DR Box Identification" on 51321
        const TcpIdentify& ace = ids.at(1);
        QVERIFY(ace.isValid());
        QCOMPARE(ace.port, 51321);
        QVERIFY(ace.request.startsWith(QByteArray::fromHex("f0000100000001000400")));
        QVERIFY(ace.request.contains("DR Box Identification"));
        QCOMPARE(static_cast<quint8>(ace.request.back()), quint8(0xf7));

        QVERIFY(strategy.matchesReplyPort(51320));
        QVERIFY(!strategy.matchesReplyPort(2222));
    }

    void ahIdentify_mapsSqFamilies() {
        struct Case {
            quint8 family;
            ConsoleType type;
        };
        const QList<Case> cases{
            {1, ConsoleType::SQ5}, {2, ConsoleType::SQ6}, {3, ConsoleType::SQ7}};
        for (const Case& c : cases) {
            const auto console = parseAh(buildAhIdentify(c.family, 1, 6, 2, 40));
            QVERIFY2(console.isValid(), QByteArray::number(c.family).constData());
            QCOMPARE(console.type, c.type);
            QCOMPARE(console.port, 51325); // MIDI-over-TCP control port
            QCOMPARE(console.firmwareVersion, QString("1.6.2"));
            QCOMPARE(console.manufacturer, Manufacturer::AllenHeath);
        }
    }

    void ahIdentify_mapsQuFamilies() {
        struct Case {
            quint8 family;
            ConsoleType type;
        };
        const QList<Case> cases{
            {8, ConsoleType::Qu16}, {9, ConsoleType::Qu24}, {10, ConsoleType::Qu32}};
        for (const Case& c : cases) {
            const auto console = parseAh(buildAhIdentify(c.family, 1, 9, 4, 0));
            QVERIFY2(console.isValid(), QByteArray::number(c.family).constData());
            QCOMPARE(console.type, c.type);
            QCOMPARE(console.port, 51325); // MIDI-over-TCP control port, same as SQ
            QCOMPARE(console.firmwareVersion, QString("1.9.4"));
        }
    }

    void ahIdentify_rejectsMalformedFrames() {
        QVERIFY(!parseAh(QByteArray()).isValid());
        QVERIFY(!parseAh(QByteArray::fromHex("7f0200000000")).isValid());           // no payload
        QVERIFY(!parseAh(QByteArray::fromHex("7f0100000000010602ABCD")).isValid()); // wrong tag
        QByteArray unknownFamily = buildAhIdentify(99, 1, 0, 0);
        QVERIFY(!parseAh(unknownFamily).isValid());
    }

    void aceIdentify_dliveModels() {
        struct Case {
            QByteArray identity;
            QString model;
        };
        const QList<Case> cases{{"TLDDM0Stagebox V1.90 - Rev. 100", "dLive DM0"},
                                {"TLDDM32Stagebox V1.90 - Rev. 100", "dLive DM32"},
                                {"TLDDM64Stagebox V1.90 - Rev. 100", "dLive DM64"},
                                {"TLDDMC32Stagebox V1.90 - Rev. 100", "dLive CDM32"},
                                {"TLDDMC64Stagebox V1.90 - Rev. 100", "dLive CDM64"}};
        for (const Case& c : cases) {
            const auto console = parseAce(c.identity);
            QVERIFY2(console.isValid(), c.identity.constData());
            QCOMPARE(console.type, ConsoleType::DLive);
            QCOMPARE(console.modelName, c.model);
            QCOMPARE(console.port, 51321); // ACE control port
            QVERIFY(console.firmwareVersion.contains("1.90"));
            QVERIFY(console.firmwareVersion.contains("Rev. 100"));
        }
    }

    void aceIdentify_avantis() {
        const auto bridge = parseAce("Bridge Avantis - Rev. 96500");
        QVERIFY(bridge.isValid());
        QCOMPARE(bridge.type, ConsoleType::Avantis);
        QCOMPARE(bridge.modelName, QString("Avantis"));
        QCOMPARE(bridge.port, 51321);

        const auto solo = parseAce("Avantis Solo - Rev. 96500");
        QVERIFY(solo.isValid());
        QCOMPARE(solo.type, ConsoleType::Avantis);
        QCOMPARE(solo.modelName, QString("Avantis Solo"));
    }

    void aceIdentify_rejectsUnknownAndMalformed() {
        QVERIFY(!parseAce("GLD80").isValid()); // GLD not identified via ACE
        QVERIFY(!parseAce("Something Else").isValid());
        // frame too short / wrong prefix
        AllenHeathProbeStrategy strategy;
        QVERIFY(!strategy
                     .parseIdentifyResponse(QByteArray::fromHex("f000"),
                                            QHostAddress("192.168.1.81"),
                                            AllenHeathProbeStrategy::ACE_IDENTIFY_PORT)
                     .isValid());
    }

    void service_twoStageAllenHeathIdentify() {
        // stand up a fake SQ responder on the A&H identify port and drive the
        // full UDP-reply -> TCP-handshake path through real sockets
        QTcpServer server;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 51326), qPrintable(server.errorString()));

        QByteArray requestSeen;
        connect(&server, &QTcpServer::newConnection, [&server, &requestSeen]() {
            QTcpSocket* conn = server.nextPendingConnection();
            connect(conn, &QTcpSocket::readyRead, [conn, &requestSeen]() {
                requestSeen = conn->readAll();
                conn->write(buildAhIdentify(3, 1, 6, 2, 40)); // SQ-7, fw 1.6.2
                conn->flush();
            });
        });

        ConsoleDiscoveryService service;
        service.registerStrategy(std::make_shared<AllenHeathProbeStrategy>());
        service.startScan(2000);

        DiscoveredConsole found;
        int count = 0;
        connect(&service, &ConsoleDiscoveryService::consoleDiscovered,
                [&](const DiscoveredConsole& c) {
                    found = c;
                    ++count;
                });

        // simulate the console's UDP "Find" reply from source port 51320
        service.processDatagram(QByteArray("Ok"), QHostAddress::LocalHost, 51320);

        QTRY_COMPARE_WITH_TIMEOUT(count, 1, 3000);
        QCOMPARE(found.type, ConsoleType::SQ7);
        QCOMPARE(found.port, 51325);
        QCOMPARE(requestSeen, QByteArray::fromHex("7f0100000000"));

        // a second identical reply must not spawn a duplicate probe/console
        service.processDatagram(QByteArray("Ok"), QHostAddress::LocalHost, 51320);
        QTest::qWait(300);
        QCOMPARE(count, 1);
    }

    void service_normalizesV4MappedAddresses() {
        // a dual-stack socket reports an IPv4 sender as ::ffff:a.b.c.d, and that
        // literal string then reaches the driver as the host to connect to
        ConsoleDiscoveryService service;
        service.registerStrategy(std::make_shared<BehringerX32ProbeStrategy>());
        service.startScan(1000);

        DiscoveredConsole found;
        connect(&service, &ConsoleDiscoveryService::consoleDiscovered,
                [&](const DiscoveredConsole& c) { found = c; });

        service.processDatagram(
            buildOscReply("/xinfo", {"192.168.8.199", "MyDesk", "X32 RACK", "4.06"}),
            QHostAddress("::ffff:192.168.8.199"), 10023);

        QTRY_VERIFY_WITH_TIMEOUT(found.isValid(), 2000);
        QCOMPARE(found.address, QHostAddress("192.168.8.199"));
        QCOMPARE(found.address.toString(), QString("192.168.8.199"));
    }

    void service_twoStageAceIdentify() {
        // fake dLive responder on the ACE identify port, sequencing the exchange
        // the way a console does
        QTcpServer server;
        QVERIFY2(server.listen(QHostAddress::LocalHost, AllenHeathProbeStrategy::ACE_IDENTIFY_PORT),
                 qPrintable(server.errorString()));

        const QByteArray handle = QByteArray::fromHex("abcd");
        QByteArray nameRequestSeen;
        QByteArray followUpSeen;
        connect(&server, &QTcpServer::newConnection,
                [&server, &nameRequestSeen, &followUpSeen, handle]() {
                    QTcpSocket* conn = server.nextPendingConnection();
                    connect(conn, &QTcpSocket::readyRead,
                            [conn, &nameRequestSeen, &followUpSeen, handle]() {
                                const QByteArray req = conn->readAll();
                                if (nameRequestSeen.isEmpty()) {
                                    nameRequestSeen = req;
                                    conn->write(buildAceHandleReply(handle));
                                } else {
                                    followUpSeen = req;
                                    conn->write(buildAceReply("TLDDM32Stagebox V1.90 - Rev. 100"));
                                }
                                conn->flush();
                            });
                });

        ConsoleDiscoveryService service;
        service.registerStrategy(std::make_shared<AllenHeathProbeStrategy>());
        service.startScan(2000);

        DiscoveredConsole found;
        int count = 0;
        connect(&service, &ConsoleDiscoveryService::consoleDiscovered,
                [&](const DiscoveredConsole& c) {
                    found = c;
                    ++count;
                });

        service.processDatagram(QByteArray("Ok"), QHostAddress::LocalHost, 51320);

        QTRY_COMPARE_WITH_TIMEOUT(count, 1, 3000);
        QCOMPARE(found.type, ConsoleType::DLive);
        QCOMPARE(found.modelName, QString("dLive DM32"));
        QCOMPARE(found.port, 51321);
        QVERIFY(nameRequestSeen.contains("DR Box Identification"));
        QCOMPARE(followUpSeen, AllenHeathProbeStrategy::buildAceHandleRead(handle));
    }
};

QTEST_MAIN(TestConsoleDiscovery)
#include "test_console_discovery.moc"
