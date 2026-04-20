#include "protocol/allenheath/AllenHeathTcpProtocol.h"
#include <QSignalSpy>
#include <QtEndian>
#include <QtTest/QtTest>
#include <cstring>

using namespace OpenMix;

class ParseableAllenHeathProtocol : public AllenHeathTcpProtocol {
    Q_OBJECT
  public:
    explicit ParseableAllenHeathProtocol(QObject* parent = nullptr)
        : AllenHeathTcpProtocol(MixerCapabilities{}, parent) {}

    void parseData(const QByteArray& data) { parseProtocolData(data); }

  protected:
    void initializeSnapshotParams() override {}
};

class TestAllenHeathParsing : public QObject {
    Q_OBJECT

  private:
    static QByteArray makeParameterFloatFrame(const QString& path, float value) {
        QByteArray pathBytes = path.toUtf8();
        QByteArray frame;
        frame.append(static_cast<char>(0x02)); // TYPE_PARAMETER
        frame.append(static_cast<char>(pathBytes.size()));
        frame.append(pathBytes);
        frame.append(static_cast<char>(0x01)); // float indicator

        quint32 bits;
        std::memcpy(&bits, &value, 4);
        quint32 be = qToBigEndian(bits);
        frame.append(reinterpret_cast<const char*>(&be), 4);

        QByteArray msg;
        quint16 len = static_cast<quint16>(frame.size());
        msg.append(static_cast<char>((len >> 8) & 0xFF));
        msg.append(static_cast<char>(len & 0xFF));
        msg.append(frame);
        return msg;
    }

    static QByteArray makeDcaFaderFrame(int dcaIndex0, float level) {
        QByteArray frame;
        frame.append(static_cast<char>(0x10)); // TYPE_DCA
        frame.append(static_cast<char>(dcaIndex0));

        quint32 bits;
        std::memcpy(&bits, &level, 4);
        quint32 be = qToBigEndian(bits);
        frame.append(reinterpret_cast<const char*>(&be), 4);

        QByteArray msg;
        quint16 len = static_cast<quint16>(frame.size());
        msg.append(static_cast<char>((len >> 8) & 0xFF));
        msg.append(static_cast<char>(len & 0xFF));
        msg.append(frame);
        return msg;
    }

  private slots:
    void typeParameter_floatRoundTrip() {
        ParseableAllenHeathProtocol proto;
        QSignalSpy spy(&proto, &ParseableAllenHeathProtocol::parameterChanged);

        proto.parseData(makeParameterFloatFrame("/ch/01/mix/fader", 0.75f));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/ch/01/mix/fader"));
        QVERIFY(qAbs(spy.at(0).at(1).toFloat() - 0.75f) < 1e-6f);
    }

    void dcaFader_floatRoundTrip() {
        ParseableAllenHeathProtocol proto;
        QSignalSpy spy(&proto, &ParseableAllenHeathProtocol::parameterChanged);

        proto.parseData(makeDcaFaderFrame(0, 0.5f)); // DCA 1 (0-indexed: 0)

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/dca/1/fader"));
        QVERIFY(qAbs(spy.at(0).at(1).toFloat() - 0.5f) < 1e-6f);
    }

    void typeParameter_multipleFrames_parsedInOrder() {
        ParseableAllenHeathProtocol proto;
        QSignalSpy spy(&proto, &ParseableAllenHeathProtocol::parameterChanged);

        QByteArray combined;
        combined.append(makeParameterFloatFrame("/dca/1/fader", 0.25f));
        combined.append(makeParameterFloatFrame("/dca/2/fader", 0.50f));
        proto.parseData(combined);

        QCOMPARE(spy.count(), 2);
        QVERIFY(qAbs(spy.at(0).at(1).toFloat() - 0.25f) < 1e-6f);
        QVERIFY(qAbs(spy.at(1).at(1).toFloat() - 0.50f) < 1e-6f);
    }
};

QTEST_MAIN(TestAllenHeathParsing)
#include "test_allenheath_parsing.moc"
