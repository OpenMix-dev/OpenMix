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
        QCOMPARE(MixerCapabilities::forProtocolId("wing").dcaCount, 24);
        QCOMPARE(MixerCapabilities::forProtocolId("sq7").dcaCount, 8);
        QCOMPARE(MixerCapabilities::forProtocolId("gld80").dcaCount, 8);
        QCOMPARE(MixerCapabilities::forProtocolId("avantis").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("dlive").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("tf5").dcaCount, 8);
        QCOMPARE(MixerCapabilities::forProtocolId("ql5").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("cl5").dcaCount, 16);
        QCOMPARE(MixerCapabilities::forProtocolId("dm7").dcaCount, 24);
        QCOMPARE(MixerCapabilities::forProtocolId("sd12").dcaCount, 12);
    }

    void unknownId_fallsBackToDefaults() {
        const MixerCapabilities caps = MixerCapabilities::forProtocolId("bogus");
        QCOMPARE(caps.dcaCount, 8);
        QCOMPARE(caps.inputChannels, 32);
        QCOMPARE(caps.mixBuses, 16);
    }

    void lookupIsCaseInsensitive() {
        QCOMPARE(MixerCapabilities::forProtocolId("WING").dcaCount, 24);
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
