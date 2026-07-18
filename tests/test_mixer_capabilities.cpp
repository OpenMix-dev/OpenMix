#include "protocol/MixerCapabilities.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestMixerCapabilities : public QObject {
    Q_OBJECT

  private slots:
    void dcaCounts_byProtocolId() {
        // the console-selection feature keys the whole app's DCA count off these
        QCOMPARE(MixerCapabilities::forProtocolId("x32").dcaCount, 8);
        QCOMPARE(MixerCapabilities::forProtocolId("m32").dcaCount, 8);
        QCOMPARE(MixerCapabilities::forProtocolId("wing").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("sq7").dcaCount, 8);
        // GLD MIDI Protocol V1.4: DCA 1 to 16 = CH 10..1F
        QCOMPARE(MixerCapabilities::forProtocolId("gld80").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("avantis").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("dlive").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("tf5").dcaCount, 8);
        QCOMPARE(MixerCapabilities::forProtocolId("ql5").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("cl5").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("dm7").dcaCount, 24);
        QCOMPARE(MixerCapabilities::forProtocolId("sd12").dcaCount, 12);
    }

    void quSeries_capabilities() {
        // Qu shares the SQ MIDI-over-TCP path but runs a 32-channel DSP
        struct Case {
            const char* id;
            ConsoleType type;
            const char* name;
        };
        const Case cases[] = {{"qu16", ConsoleType::Qu16, "Allen & Heath Qu-16"},
                              {"qu24", ConsoleType::Qu24, "Allen & Heath Qu-24"},
                              {"qu32", ConsoleType::Qu32, "Allen & Heath Qu-32"}};
        for (const Case& c : cases) {
            const MixerCapabilities caps = MixerCapabilities::forProtocolId(c.id);
            QCOMPARE(caps.type, c.type);
            QCOMPARE(caps.displayName, QString(c.name));
            QCOMPARE(caps.manufacturer, Manufacturer::AllenHeath);
            QCOMPARE(caps.protocol, ProtocolType::MidiTcp);
            QCOMPARE(caps.defaultPort, 51325);
            QCOMPARE(caps.inputChannels, 32);
            QCOMPARE(caps.dcaCount, 4); // Qu MIDI Protocol V1.9+: DCA Groups 1 to 4
            QCOMPARE(caps.scenes, 100); // ... and Scene 1 to 100
            QCOMPARE(caps.mixBuses, 12);
        }
        // bare "qu" resolves to the base Qu-16
        QCOMPARE(MixerCapabilities::forProtocolId("qu").type, ConsoleType::Qu16);
    }

    void unknownId_fallsBackToDefaults() {
        const MixerCapabilities caps = MixerCapabilities::forProtocolId("bogus");
        QCOMPARE(caps.dcaCount, 8);
        QCOMPARE(caps.inputChannels, 32);
        QCOMPARE(caps.mixBuses, 16);
    }

    void lookupIsCaseInsensitive() {
        QCOMPARE(MixerCapabilities::forProtocolId("WING").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("Cl5").dcaCount, 16);
    }

    void allSupportedIds_roundTripThroughLookup() {
        const QVector<MixerCapabilities> all = MixerCapabilities::allSupported();
        QVERIFY(!all.isEmpty());
        for (const MixerCapabilities& caps : all) {
            const MixerCapabilities looked = MixerCapabilities::forProtocolId(caps.protocolId);
            QCOMPARE(looked.type, caps.type);
            QCOMPARE(looked.dcaCount, caps.dcaCount);
        }
    }
};

QTEST_MAIN(TestMixerCapabilities)
#include "test_mixer_capabilities.moc"
