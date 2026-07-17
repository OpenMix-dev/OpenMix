#include "core/Show.h"
#include <QJsonArray>
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestShow : public QObject {
    Q_OBJECT

  private slots:
    void setName_marksShowDirty() {
        Show show;
        QVERIFY(!show.isModified());
        QSignalSpy spy(&show, &Show::modifiedChanged);

        show.setName("Concert Night");

        QVERIFY(show.isModified());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setAuthor_marksShowDirty() {
        Show show;
        QVERIFY(!show.isModified());
        QSignalSpy spy(&show, &Show::modifiedChanged);

        show.setAuthor("Jane");

        QVERIFY(show.isModified());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setNotes_marksShowDirty() {
        Show show;
        QVERIFY(!show.isModified());
        QSignalSpy spy(&show, &Show::modifiedChanged);

        show.setNotes("Pre-show notes");

        QVERIFY(show.isModified());
        QCOMPARE(spy.count(), 1);
    }

    void setMixerConfig_marksShowDirty() {
        Show show;
        QVERIFY(!show.isModified());
        QSignalSpy spy(&show, &Show::modifiedChanged);

        MixerConfig config;
        config.type = "wing";
        config.host = "192.168.1.100";
        config.port = 10023;
        show.setMixerConfig(config);

        QVERIFY(show.isModified());
        QCOMPARE(spy.count(), 1);
    }

    void mixerConfig_faderLawRoundTrips() {
        // the console cannot report its NRPN Fader Law, so the show has to carry it
        MixerConfig config;
        config.type = "sq7";
        config.host = "192.168.1.70";
        config.faderLaw = "audio";

        const MixerConfig back = MixerConfig::fromJson(config.toJson());
        QCOMPARE(back.faderLaw, QString("audio"));
        QVERIFY(back == config);

        // shows written before the setting existed default to the console's
        // standard mode rather than to nothing
        QJsonObject legacy;
        legacy["type"] = "sq7";
        QCOMPARE(MixerConfig::fromJson(legacy).faderLaw, QString("linear"));
    }

    void setMixerConfig_identicalConfig_isNoOp() {
        Show show;
        MixerConfig config = show.mixerConfig();
        QSignalSpy changed(&show, &Show::mixerConfigChanged);
        QSignalSpy modified(&show, &Show::modifiedChanged);

        show.setMixerConfig(config); // same values -> no dirty, no signal
        QVERIFY(!show.isModified());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(modified.count(), 0);

        config.type = "cl5";
        show.setMixerConfig(config);
        QCOMPARE(changed.count(), 1);
        QVERIFY(show.isModified());
    }

    void mixerConfig_roundTripsDcaCount() {
        MixerConfig config;
        config.type = "wing";
        config.dcaCount = 24;
        const MixerConfig loaded = MixerConfig::fromJson(config.toJson());
        QVERIFY(loaded == config);
        QCOMPARE(loaded.dcaCount, 24);
    }

    void setFilePath_doesNotMarkDirty() {
        Show show;
        QVERIFY(!show.isModified());
        QSignalSpy spy(&show, &Show::modifiedChanged);

        show.setFilePath("/path/to/show.omx");

        QVERIFY(!show.isModified());
        QCOMPARE(spy.count(), 0);
    }

    void fromJson_clearsDirtyFlag() {
        Show show;
        show.setName("Dirty Show");
        QVERIFY(show.isModified());

        QJsonObject json = show.toJson();
        show.fromJson(json);

        QVERIFY(!show.isModified());
    }

    void newShow_clearsDirtyFlag() {
        Show show;
        show.setName("Something");
        QVERIFY(show.isModified());

        show.newShow();

        QVERIFY(!show.isModified());
    }

    void modifiedChanged_notEmittedAgain_whenAlreadyDirty() {
        Show show;
        show.setName("First");
        QVERIFY(show.isModified());

        QSignalSpy spy(&show, &Show::modifiedChanged);
        show.setName("Second");

        QCOMPARE(spy.count(), 0);
    }

    void setModified_false_clearsDirtyAndEmitsSignal() {
        Show show;
        show.setName("Something");
        QVERIFY(show.isModified());

        QSignalSpy spy(&show, &Show::modifiedChanged);
        show.setModified(false);

        QVERIFY(!show.isModified());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    void inactiveDcas_roundTripAndSignals() {
        Show show;
        QSignalSpy changed(&show, &Show::activeDcasChanged);

        show.setDcaActive(3, false);
        show.setDcaActive(7, false);
        show.setDcaActive(3, false); // no-op: already inactive
        QCOMPARE(changed.count(), 2);
        QVERIFY(show.isModified());
        QVERIFY(!show.isDcaActive(3));
        QVERIFY(show.isDcaActive(1));

        Show other;
        other.fromJson(show.toJson());
        QCOMPARE(other.inactiveDcas(), QSet<int>({3, 7}));
        QVERIFY(!other.isModified());

        show.setDcaActive(3, true);
        QCOMPARE(show.inactiveDcas(), QSet<int>({7}));
    }

    void inactiveDcas_missingKeyMeansAllActive() {
        QJsonObject legacy;
        legacy["version"] = "1.9";
        legacy["name"] = "Legacy";
        legacy["cues"] = QJsonArray();

        Show show;
        show.setDcaActive(2, false); // pre-existing state must clear on load
        show.fromJson(legacy);

        QVERIFY(show.inactiveDcas().isEmpty());
        QVERIFY(show.isDcaActive(2));
    }
};

QTEST_MAIN(TestShow)
#include "test_show.moc"
