#include "protocol/MixerCapabilities.h"
#include "protocol/digico/DiGiCoProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

// DiGiCo publishes no OSC address map: Generic OSC is user-defined, and the
// syntax varies by model and software version. The driver therefore sends the
// operator's patterns and nothing else, so what is worth testing is that a
// pattern is expanded exactly as given and that a missing one sends nothing at
// all - a guessed address would look like it worked and do nothing on the desk.
class TestDiGiCoProtocol : public QObject {
    Q_OBJECT

  private slots:
    void expand_putsTheChannelWhereTheStarIs() {
        // "*" is the console's own convention for the channel number
        QCOMPARE(DiGiCoProtocol::expand("/ch/*/fader", 3), QString("/ch/3/fader"));
        QCOMPARE(DiGiCoProtocol::expand("/MyDevice/MyParameter/*", 12),
                 QString("/MyDevice/MyParameter/12"));
        // a pattern without a star is an address in its own right
        QCOMPARE(DiGiCoProtocol::expand("/snapshot/fire", 4), QString("/snapshot/fire"));
    }

    void expand_withoutAPatternYieldsNothing() {
        // an operation the operator never configured has no address to send to
        QVERIFY(DiGiCoProtocol::expand(QString(), 1).isEmpty());
        QVERIFY(DiGiCoProtocol::expand("", 7).isEmpty());
    }

    void templates_defaultToUnconfigured() {
        // nothing is assumed about the console's syntax until the operator says
        DiGiCoProtocol p(MixerCapabilities::forConsole(ConsoleType::SD12));
        QVERIFY(p.templates().channelFader.isEmpty());
        QVERIFY(p.templates().channelMute.isEmpty());
        QVERIFY(p.templates().sceneRecall.isEmpty());
    }

    void templates_roundTrip() {
        DiGiCoProtocol p(MixerCapabilities::forConsole(ConsoleType::SD12));
        p.setTemplates({"/ch/*/fader", "/ch/*/mute", "/snapshot/fire"});
        QCOMPARE(p.templates().channelFader, QString("/ch/*/fader"));
        QCOMPARE(p.templates().channelMute, QString("/ch/*/mute"));
        QCOMPARE(p.templates().sceneRecall, QString("/snapshot/fire"));
    }

    void unconfigured_sendsNothingRatherThanGuessing() {
        // not connected and not configured: the calls must be inert, not invent
        DiGiCoProtocol p(MixerCapabilities::forConsole(ConsoleType::SD12));
        p.setChannelFaderDb(1, 0.0);
        p.setChannelMute(1, true);
        p.recallScene(1);
        QVERIFY(!p.isConnected());
        QVERIFY(p.getParameter("/ch/1/fader").isNull());
    }

    void capabilities_areGenericOsc() {
        // Generic OSC is UDP, and the port is whatever the operator paired; 9000
        // is only the common default. It is not Allen & Heath's 51321, which this
        // driver used to point at while speaking Yamaha's protocol.
        const auto caps = MixerCapabilities::forConsole(ConsoleType::SD12);
        QCOMPARE(caps.protocol, ProtocolType::OscUdp);
        QCOMPARE(caps.defaultPort, 9000);
        QCOMPARE(caps.manufacturer, Manufacturer::DiGiCo);
    }
};

QTEST_MAIN(TestDiGiCoProtocol)
#include "test_digico_protocol.moc"
