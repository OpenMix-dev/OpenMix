#include "core/ChannelMonitor.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestChannelMonitor : public QObject {
    Q_OBJECT

    static constexpr int kSilenceMs = 40;
    static constexpr int kClipMs = 40;

    static void configure(ChannelMonitor& monitor) {
        monitor.setSilenceThreshold(0.05);
        monitor.setClipThreshold(0.95);
        monitor.setSilenceTimeoutMs(kSilenceMs);
        monitor.setClipHoldMs(kClipMs);
    }

  private slots:
    void normalLevel_staysNormal() {
        ChannelMonitor monitor;
        configure(monitor);
        QSignalSpy spy(&monitor, &ChannelMonitor::channelStateChanged);

        monitor.onLevel(1, 0.5);
        QCOMPARE(monitor.channelState(1), ChannelState::Normal);
        QCOMPARE(spy.count(), 0); // already Normal, no transition
    }

    void silence_afterTimeout() {
        ChannelMonitor monitor;
        configure(monitor);
        QSignalSpy spy(&monitor, &ChannelMonitor::channelStateChanged);

        monitor.onLevel(1, 0.0);
        // not silent yet: timeout has not elapsed
        QCOMPARE(monitor.channelState(1), ChannelState::Normal);
        QCOMPARE(spy.count(), 0);

        QVERIFY(QTest::qWaitFor(
            [&]() { return monitor.channelState(1) == ChannelState::Silent; }, 4 * kSilenceMs));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);                    // channel
        QCOMPARE(spy.at(0).at(1).toInt(), int(ChannelState::Silent));
    }

    void silence_recoversWhenAudioReturns() {
        ChannelMonitor monitor;
        configure(monitor);
        monitor.onLevel(1, 0.0);
        QVERIFY(QTest::qWaitFor(
            [&]() { return monitor.channelState(1) == ChannelState::Silent; }, 4 * kSilenceMs));

        QSignalSpy spy(&monitor, &ChannelMonitor::channelStateChanged);
        monitor.onLevel(1, 0.5); // audio back
        QCOMPARE(monitor.channelState(1), ChannelState::Normal);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), int(ChannelState::Normal));
    }

    void clip_thenRestores() {
        ChannelMonitor monitor;
        configure(monitor);
        QSignalSpy spy(&monitor, &ChannelMonitor::channelStateChanged);

        monitor.onLevel(2, 1.0);
        // clipping is immediate
        QCOMPARE(monitor.channelState(2), ChannelState::Clipping);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), int(ChannelState::Clipping));

        // after the hold window with no further clips, restore to Normal
        QVERIFY(QTest::qWaitFor(
            [&]() { return monitor.channelState(2) == ChannelState::Normal; }, 4 * kClipMs));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(1).toInt(), int(ChannelState::Normal));
    }

    void clip_holdsWhileHot() {
        ChannelMonitor monitor;
        configure(monitor);
        monitor.onLevel(3, 1.0);
        QCOMPARE(monitor.channelState(3), ChannelState::Clipping);
        // a quiet sample during the hold window must not flip it to Silent/Normal
        monitor.onLevel(3, 0.0);
        QCOMPARE(monitor.channelState(3), ChannelState::Clipping);
    }
};

QTEST_MAIN(TestChannelMonitor)
#include "test_channel_monitor.moc"
