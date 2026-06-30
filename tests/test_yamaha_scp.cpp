#include "protocol/yamaha/YamahaDM7Protocol.h"
#include "protocol/yamaha/YamahaProtocol.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

// Exposes the protected SCP line parser for testing.
class ScpProbe : public YamahaProtocol {
  public:
    explicit ScpProbe(QObject* parent = nullptr)
        : YamahaProtocol(MixerCapabilities::forConsole(ConsoleType::CL5), parent) {}

    void feed(const QByteArray& line) { parseScpLine(line); }
};

class TestYamahaScp : public QObject {
    Q_OBJECT

  private slots:
    // ---- pure command framing (exact ASCII strings) ----

    void scpSet_exactString() {
        QCOMPARE(YamahaProtocol::scpSet("MIXER:Current/InCh/Fader/Level", 0, 0, 1000),
                 QByteArray("set MIXER:Current/InCh/Fader/Level 0 0 1000\n"));
        // negative (centi-dB) value
        QCOMPARE(YamahaProtocol::scpSet("MIXER:Current/InCh/Fader/Level", 5, 0, -32768),
                 QByteArray("set MIXER:Current/InCh/Fader/Level 5 0 -32768\n"));
    }

    void scpSet_stringValueIsQuoted() {
        QCOMPARE(YamahaProtocol::scpSet("MIXER:Current/InCh/Label/Name", 2, 0, QString("Vox")),
                 QByteArray("set MIXER:Current/InCh/Label/Name 2 0 \"Vox\"\n"));
    }

    void scpGet_exactString() {
        QCOMPARE(YamahaProtocol::scpGet("MIXER:Current/InCh/Fader/Level", 0, 0),
                 QByteArray("get MIXER:Current/InCh/Fader/Level 0 0\n"));
    }

    void scpSceneRecall_exactString() {
        QCOMPARE(YamahaProtocol::scpSceneRecall(5),
                 QByteArray("ssrecall_ex MIXER:Lib/Scene 5\n"));
        QCOMPARE(YamahaProtocol::scpSceneRecall(100),
                 QByteArray("ssrecall_ex MIXER:Lib/Scene 100\n"));
    }

    // ---- value scaling ----

    void faderTaper_anchors() {
        QCOMPARE(YamahaProtocol::faderLevelToScp(0.0), -32768);  // -inf
        QCOMPARE(YamahaProtocol::faderLevelToScp(0.75), 0);      // 0 dB at unity
        QCOMPARE(YamahaProtocol::faderLevelToScp(1.0), 1000);    // +10 dB at top
        // out-of-range clamps
        QCOMPARE(YamahaProtocol::faderLevelToScp(-0.5), -32768);
        QCOMPARE(YamahaProtocol::faderLevelToScp(2.0), 1000);
    }

    void faderTaper_monotonic() {
        int prev = YamahaProtocol::faderLevelToScp(0.01);
        for (double l = 0.05; l <= 1.0; l += 0.05) {
            int cur = YamahaProtocol::faderLevelToScp(l);
            QVERIFY2(cur >= prev, qPrintable(QString("non-monotonic at %1").arg(l)));
            prev = cur;
        }
    }

    void scaling_helpers() {
        QCOMPARE(YamahaProtocol::dbToCentiDb(-10.0), -1000);
        QCOMPARE(YamahaProtocol::dbToCentiDb(10.0), 1000);
        QCOMPARE(YamahaProtocol::hzToScpFreq(100.0), 1000); // Hz * 10
        QCOMPARE(YamahaProtocol::hzToScpFreq(80.0), 800);
        QCOMPARE(YamahaProtocol::qToScp(1.4), 1400); // Q * 1000
    }

    // ---- semantic builders (channel index is 0-based) ----

    void buildFader_usesZeroBasedIndex() {
        ScpProbe p;
        QCOMPARE(p.buildChannelFader(0, 0.75),
                 QByteArray("set MIXER:Current/InCh/Fader/Level 0 0 0\n"));
        QCOMPARE(p.buildChannelFader(3, 1.0),
                 QByteArray("set MIXER:Current/InCh/Fader/Level 3 0 1000\n"));
    }

    void buildMute_onMeansChannelOff() {
        ScpProbe p;
        // muted -> Fader/On 0
        QCOMPARE(p.buildChannelMute(2, true),
                 QByteArray("set MIXER:Current/InCh/Fader/On 2 0 0\n"));
        // unmuted -> Fader/On 1
        QCOMPARE(p.buildChannelMute(2, false),
                 QByteArray("set MIXER:Current/InCh/Fader/On 2 0 1\n"));
    }

    void buildPreamp_clUsesCentiDb() {
        ScpProbe p; // CL caps
        QCOMPARE(p.buildChannelPreamp(0, 30.0),
                 QByteArray("set MIXER:Current/InCh/Port/HA/Gain 0 0 3000\n"));
    }

    void buildPreamp_dm7UsesWholeDb() {
        YamahaDM7Protocol dm7(MixerCapabilities::forConsole(ConsoleType::DM7));
        QCOMPARE(dm7.buildChannelPreamp(0, 30.0),
                 QByteArray("set MIXER:Current/InCh/Port/HA/Gain 0 0 30\n"));
    }

    void buildHpf() {
        ScpProbe p;
        QCOMPARE(p.buildChannelHpfOn(1, true),
                 QByteArray("set MIXER:Current/InCh/HPF/On 1 0 1\n"));
        QCOMPARE(p.buildChannelHpfFreq(2, 100.0),
                 QByteArray("set MIXER:Current/InCh/HPF/Freq 2 0 1000\n"));
    }

    void buildEqOnAndBands() {
        ScpProbe p;
        QCOMPARE(p.buildChannelEqOn(0, true),
                 QByteArray("set MIXER:Current/InCh/PEQ/On 0 0 1\n"));
        // band on -> Bypass 0, band off -> Bypass 1 (idx2 = band)
        QCOMPARE(p.buildChannelEqBandBypass(0, 1, true),
                 QByteArray("set MIXER:Current/InCh/PEQ/Band/Bypass 0 1 0\n"));
        QCOMPARE(p.buildChannelEqBandBypass(0, 1, false),
                 QByteArray("set MIXER:Current/InCh/PEQ/Band/Bypass 0 1 1\n"));
        QCOMPARE(p.buildChannelEqBandFreq(0, 1, 1000.0),
                 QByteArray("set MIXER:Current/InCh/PEQ/Band/Freq 0 1 10000\n"));
        QCOMPARE(p.buildChannelEqBandGain(0, 1, 6.0),
                 QByteArray("set MIXER:Current/InCh/PEQ/Band/Gain 0 1 600\n"));
        QCOMPARE(p.buildChannelEqBandQ(0, 1, 1.4),
                 QByteArray("set MIXER:Current/InCh/PEQ/Band/Q 0 1 1400\n"));
    }

    void buildEqBandGain_clamps() {
        ScpProbe p;
        QCOMPARE(p.buildChannelEqBandGain(0, 0, 100.0), // way over +18 dB
                 QByteArray("set MIXER:Current/InCh/PEQ/Band/Gain 0 0 1800\n"));
    }

    void buildDynamicsThreshold() {
        ScpProbe p;
        QCOMPARE(p.buildChannelDynamicsThreshold(0, -26.0),
                 QByteArray("set MIXER:Current/InCh/Dyna2/Threshold 0 0 -260\n"));
    }

    // ---- incoming line parsing ----

    void parseNotify_emitsParameterChanged() {
        ScpProbe p;
        QSignalSpy spy(&p, &MixerProtocol::parameterChanged);

        p.feed("NOTIFY set \"MIXER:Current/InCh/Fader/Level\" 0 0 1000");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("MIXER:Current/InCh/Fader/Level"));
        QCOMPARE(spy.at(0).at(1).value<QVariant>().toInt(), 1000);
    }

    void parseOkSet_emitsParameterChanged() {
        ScpProbe p;
        QSignalSpy spy(&p, &MixerProtocol::parameterChanged);

        p.feed("OK set \"MIXER:Current/InCh/Fader/On\" 4 0 0");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("MIXER:Current/InCh/Fader/On"));
        QCOMPARE(spy.at(0).at(1).value<QVariant>().toInt(), 0);
    }

    void parseDevinfo_doesNotEmitParameterChanged() {
        ScpProbe p;
        QSignalSpy spy(&p, &MixerProtocol::parameterChanged);

        p.feed("OK devinfo productname \"CL5\"");

        QCOMPARE(spy.count(), 0);
        QVERIFY(p.connectionStatus().contains("CL5"));
    }

    void parseError_isIgnored() {
        ScpProbe p;
        QSignalSpy spy(&p, &MixerProtocol::parameterChanged);

        p.feed("ERROR set \"MIXER:Current/InCh/Fader/Level\" 0 0 1000");

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestYamahaScp)
#include "test_yamaha_scp.moc"
