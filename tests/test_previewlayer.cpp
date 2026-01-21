#include "core/PreviewLayer.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestPreviewLayer : public QObject {
    Q_OBJECT

  private slots:
    void testInitialState() {
        PreviewLayer layer;
        QVERIFY(!layer.isEnabled());
        QCOMPARE(layer.cachedCount(), 0);
        QVERIFY(layer.cachedPaths().isEmpty());
    }

    void testSetEnabled() {
        PreviewLayer layer;

        QSignalSpy enabledSpy(&layer, &PreviewLayer::enabledChanged);

        layer.setEnabled(true);
        QVERIFY(layer.isEnabled());
        QCOMPARE(enabledSpy.count(), 1);

        // setting to same value should not emit signal
        layer.setEnabled(true);
        QCOMPARE(enabledSpy.count(), 1);

        layer.setEnabled(false);
        QVERIFY(!layer.isEnabled());
        QCOMPARE(enabledSpy.count(), 2);
    }

    void testSendParameterWhenEnabled() {
        PreviewLayer layer;

        QSignalSpy cachedSpy(&layer, &PreviewLayer::parameterCached);

        layer.setEnabled(true);
        layer.sendParameter("/dca/1/fader", 0.75);

        QVERIFY(layer.hasCachedValue("/dca/1/fader"));
        QCOMPARE(layer.cachedValue("/dca/1/fader").toDouble(), 0.75);
        QCOMPARE(layer.cachedCount(), 1);
        QCOMPARE(cachedSpy.count(), 1);
    }

    void testSendParameterWhenDisabled() {
        PreviewLayer layer;

        QSignalSpy cachedSpy(&layer, &PreviewLayer::parameterCached);

        layer.setEnabled(false);
        layer.sendParameter("/dca/1/fader", 0.75);

        // without a mixer, nothing happens
        QVERIFY(!layer.hasCachedValue("/dca/1/fader"));
        QCOMPARE(layer.cachedCount(), 0);
        QCOMPARE(cachedSpy.count(), 0);
    }

    void testMultipleParameters() {
        PreviewLayer layer;
        layer.setEnabled(true);

        layer.sendParameter("/dca/1/fader", 0.5);
        layer.sendParameter("/dca/2/fader", 0.6);
        layer.sendParameter("/dca/3/fader", 0.7);

        QCOMPARE(layer.cachedCount(), 3);

        QStringList paths = layer.cachedPaths();
        QVERIFY(paths.contains("/dca/1/fader"));
        QVERIFY(paths.contains("/dca/2/fader"));
        QVERIFY(paths.contains("/dca/3/fader"));
    }

    void testUpdateExistingParameter() {
        PreviewLayer layer;
        layer.setEnabled(true);

        layer.sendParameter("/dca/1/fader", 0.5);
        QCOMPARE(layer.cachedValue("/dca/1/fader").toDouble(), 0.5);

        layer.sendParameter("/dca/1/fader", 0.8);
        QCOMPARE(layer.cachedValue("/dca/1/fader").toDouble(), 0.8);

        // should still only have one cached value
        QCOMPARE(layer.cachedCount(), 1);
    }

    void testDiscard() {
        PreviewLayer layer;
        layer.setEnabled(true);

        layer.sendParameter("/dca/1/fader", 0.5);
        layer.sendParameter("/dca/2/fader", 0.6);
        QCOMPARE(layer.cachedCount(), 2);

        QSignalSpy clearedSpy(&layer, &PreviewLayer::cacheCleared);

        layer.discard();

        QCOMPARE(layer.cachedCount(), 0);
        QVERIFY(!layer.hasCachedValue("/dca/1/fader"));
        QVERIFY(!layer.hasCachedValue("/dca/2/fader"));
        QCOMPARE(clearedSpy.count(), 1);
    }

    void testFlushWithoutMixer() {
        PreviewLayer layer;
        layer.setEnabled(true);

        layer.sendParameter("/dca/1/fader", 0.5);
        QCOMPARE(layer.cachedCount(), 1);

        QSignalSpy flushedSpy(&layer, &PreviewLayer::cacheFlushed);

        // flush without a mixer should still clear cache & emit signal
        layer.flush();

        QCOMPARE(layer.cachedCount(), 0);
        QCOMPARE(flushedSpy.count(), 1);
    }

    void testFlushEmptyCache() {
        PreviewLayer layer;
        layer.setEnabled(true);

        QSignalSpy flushedSpy(&layer, &PreviewLayer::cacheFlushed);

        // flushing empty cache should emit signal
        layer.flush();

        QCOMPARE(flushedSpy.count(), 1);
    }

    void testCachedValueForNonExistent() {
        PreviewLayer layer;
        layer.setEnabled(true);

        layer.sendParameter("/dca/1/fader", 0.5);

        // getting non-existent cached value should return invalid variant
        QVariant nonExistent = layer.cachedValue("/dca/999/fader");
        QVERIFY(!nonExistent.isValid());
    }

    void testHasCachedValue() {
        PreviewLayer layer;
        layer.setEnabled(true);

        QVERIFY(!layer.hasCachedValue("/dca/1/fader"));

        layer.sendParameter("/dca/1/fader", 0.5);
        QVERIFY(layer.hasCachedValue("/dca/1/fader"));
        QVERIFY(!layer.hasCachedValue("/dca/2/fader"));

        layer.discard();
        QVERIFY(!layer.hasCachedValue("/dca/1/fader"));
    }

    void testDifferentValueTypes() {
        PreviewLayer layer;
        layer.setEnabled(true);

        // test different value types
        layer.sendParameter("/dca/1/fader", 0.75);  // double
        layer.sendParameter("/dca/1/on", 1);        // int
        layer.sendParameter("/dca/1/mute", true);   // bool
        layer.sendParameter("/dca/1/name", "Test"); // string

        QCOMPARE(layer.cachedCount(), 4);
        QCOMPARE(layer.cachedValue("/dca/1/fader").toDouble(), 0.75);
        QCOMPARE(layer.cachedValue("/dca/1/on").toInt(), 1);
        QCOMPARE(layer.cachedValue("/dca/1/mute").toBool(), true);
        QCOMPARE(layer.cachedValue("/dca/1/name").toString(), QString("Test"));
    }
};

QTEST_MAIN(TestPreviewLayer)
#include "test_previewlayer.moc"
