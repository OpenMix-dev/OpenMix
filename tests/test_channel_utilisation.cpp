#include "core/ChannelUtilisation.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/DCAMapping.h"
#include "core/Show.h"
#include <QMap>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestChannelUtilisation : public QObject {
    Q_OBJECT

    static QMap<int, int> counts(const QList<ChannelUsage>& usage) {
        QMap<int, int> c;
        for (const ChannelUsage& u : usage)
            c.insert(u.channel, u.cueNumbers.size());
        return c;
    }

  private slots:
    void testCustomMappingAndPerChannel() {
        Show show;
        Cue c1(1.0, "One");
        QMap<int, QList<int>> m;
        m.insert(1, {5, 6});
        c1.setDCAChannelMapping(m);
        c1.setChannelProfile(7, "lead");
        show.cueList()->addCue(c1);

        Cue c2(2.0, "Two");
        c2.setChannelLevel(6, 0.5);
        show.cueList()->addCue(c2);

        const QMap<int, int> c = counts(computeChannelUtilisation(&show));
        QCOMPARE(c.value(5), 1);
        QCOMPARE(c.value(6), 2); // c1 via DCA mapping, c2 via level override
        QCOMPARE(c.value(7), 1); // c1 via profile
    }

    void testShowMappingUsedWhenNoCustom() {
        Show show;
        show.dcaMapping()->assignChannelToDCA(3, 1);
        Cue c(1.0, "One"); // no custom mapping -> falls back to show mapping
        show.cueList()->addCue(c);

        const QMap<int, int> counts = TestChannelUtilisation::counts(computeChannelUtilisation(&show));
        QCOMPARE(counts.value(3), 1);
    }

    void testEmptyShow() {
        Show show;
        QVERIFY(computeChannelUtilisation(&show).isEmpty());
        QVERIFY(computeChannelUtilisation(nullptr).isEmpty());
    }
};

QTEST_MAIN(TestChannelUtilisation)
#include "test_channel_utilisation.moc"
