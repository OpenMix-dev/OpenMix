#include "protocol/yamaha/YamahaProtocol.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestYamahaProtocol : public YamahaProtocol {
    Q_OBJECT
  public:
    explicit TestYamahaProtocol(QObject* parent = nullptr)
        : YamahaProtocol(MixerCapabilities{}, parent) {}

    void parseOscData(const QByteArray& data) { parseOscMessage(data); }

  protected:
    void initializeSnapshotParams() override {}
};

// builds an OSC message with a single blob argument whose declared size is negative.
// path is 4-byte aligned; type tag ",b" is 4-byte aligned; then 4 bytes of big-endian int32.
static QByteArray makeMalformedBlobMessage(const QString& path, qint32 blobSize) {
    QByteArray data;

    QByteArray pathBytes = path.toUtf8();
    pathBytes.append('\0');
    while (pathBytes.size() % 4 != 0)
        pathBytes.append('\0');
    data.append(pathBytes);

    QByteArray typeTag(",b\0", 3);
    while (typeTag.size() % 4 != 0)
        typeTag.append('\0');
    data.append(typeTag);

    data.append(static_cast<char>((blobSize >> 24) & 0xFF));
    data.append(static_cast<char>((blobSize >> 16) & 0xFF));
    data.append(static_cast<char>((blobSize >> 8) & 0xFF));
    data.append(static_cast<char>(blobSize & 0xFF));

    return data;
}

class TestYamahaOsc : public QObject {
    Q_OBJECT

  private slots:
    void negativeBlobSize_doesNotEmitParameterChanged() {
        TestYamahaProtocol proto;
        QSignalSpy spy(&proto, &TestYamahaProtocol::parameterChanged);

        proto.parseOscData(makeMalformedBlobMessage("/test/blob", -1));

        QCOMPARE(spy.count(), 0);
    }

    void validFloat_emitsParameterChanged() {
        TestYamahaProtocol proto;
        QSignalSpy spy(&proto, &TestYamahaProtocol::parameterChanged);

        // build valid OSC float message: path "/dca/1/fader", type 'f', value 0.75
        QByteArray data;
        QByteArray pathBytes("/dca/1/fader\0\0\0\0", 16); // 12 chars + null + 3 pads = 16
        data.append(pathBytes);
        data.append(",f\0\0", 4); // type tag
        // 0.75f as big-endian
        quint32 bits;
        float v = 0.75f;
        std::memcpy(&bits, &v, 4);
        quint32 be = qToBigEndian(bits);
        data.append(reinterpret_cast<const char*>(&be), 4);

        proto.parseOscData(data);

        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(1).toFloat() - 0.75f) < 1e-6f);
    }
};

QTEST_MAIN(TestYamahaOsc)
#include "test_yamaha_osc.moc"
