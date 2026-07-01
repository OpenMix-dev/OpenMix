#include "core/Ensemble.h"
#include "core/Show.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestEnsembles : public QObject {
    Q_OBJECT

  private slots:
    // --- Ensemble --------------------------------------------------------
    void ensemble_channels_normalized() {
        Ensemble e("Chorus");
        e.setChannels({5, 1, 3, 1, 0, -2}); // unsorted, dupes, non-positive
        QCOMPARE(e.channels(), QList<int>({1, 3, 5}));

        e.addChannel(2);
        e.addChannel(2); // no dupe
        e.addChannel(0); // rejected
        QCOMPARE(e.channels(), QList<int>({1, 2, 3, 5}));

        e.removeChannel(3);
        QVERIFY(!e.hasChannel(3));
        QVERIFY(e.hasChannel(5));
    }

    void ensemble_defaultSlot_andSetter() {
        Ensemble e("A");
        QCOMPARE(e.profileSlot(), QString("Main"));
        e.setProfileSlot("Solo");
        QCOMPARE(e.profileSlot(), QString("Solo"));
        e.setProfileSlot(""); // empty falls back to default
        QCOMPARE(e.profileSlot(), QString("Main"));
    }

    void ensemble_roundTrip() {
        Ensemble e("Band");
        e.setChannels({9, 4, 4, 7});
        e.setProfileSlot("Solo");

        Ensemble r = Ensemble::fromJson(e.toJson());
        QCOMPARE(r.id(), e.id());
        QCOMPARE(r.name(), QString("Band"));
        QCOMPARE(r.channels(), QList<int>({4, 7, 9}));
        QCOMPARE(r.profileSlot(), QString("Solo"));
    }

    // --- EnsembleLibrary -------------------------------------------------
    void library_addUpdateRemove_emitsSignals() {
        EnsembleLibrary lib;
        QSignalSpy added(&lib, &EnsembleLibrary::ensembleAdded);
        QSignalSpy modified(&lib, &EnsembleLibrary::ensembleModified);
        QSignalSpy removed(&lib, &EnsembleLibrary::ensembleRemoved);
        QSignalSpy changed(&lib, &EnsembleLibrary::changed);

        Ensemble e("Cast");
        e.setChannels({1, 2});
        lib.addEnsemble(e);
        QCOMPARE(lib.count(), 1);
        QCOMPARE(added.count(), 1);

        Ensemble upd = e;
        upd.setName("Full Cast");
        lib.updateEnsemble(e.id(), upd);
        QCOMPARE(modified.count(), 1);
        QCOMPARE(lib.ensembleById(e.id())->name(), QString("Full Cast"));

        lib.removeEnsemble(e.id());
        QCOMPARE(removed.count(), 1);
        QCOMPARE(lib.count(), 0);
        QVERIFY(changed.count() >= 3);
    }

    void library_ensemblesForChannel() {
        EnsembleLibrary lib;
        Ensemble a("A");
        a.setChannels({1, 2, 3});
        Ensemble b("B");
        b.setChannels({3, 4});
        lib.addEnsemble(a);
        lib.addEnsemble(b);

        QCOMPARE(lib.ensemblesForChannel(3).size(), 2);
        QCOMPARE(lib.ensemblesForChannel(1).size(), 1);
        QCOMPARE(lib.ensemblesForChannel(9).size(), 0);
    }

    void library_roundTrips() {
        EnsembleLibrary lib;
        Ensemble a("Chorus");
        a.setChannels({10, 11, 12});
        a.setProfileSlot("Solo");
        lib.addEnsemble(a);

        EnsembleLibrary other;
        other.loadFromJson(lib.toJson());
        QCOMPARE(other.count(), 1);
        const Ensemble& r = other.ensembles().first();
        QCOMPARE(r.name(), QString("Chorus"));
        QCOMPARE(r.channels(), QList<int>({10, 11, 12}));
        QCOMPARE(r.profileSlot(), QString("Solo"));
    }

    // --- Show persistence ------------------------------------------------
    void show_persistsEnsembles_andMarksDirty() {
        Show show;
        QSignalSpy spy(&show, &Show::modifiedChanged);
        Ensemble e("Kids");
        e.setChannels({20, 21});
        show.ensembleLibrary()->addEnsemble(e);
        QVERIFY(show.isModified());
        QVERIFY(spy.count() >= 1);

        const QJsonObject json = show.toJson();
        QCOMPARE(json["version"].toString(), QString("1.7"));

        Show loaded;
        loaded.fromJson(json);
        QCOMPARE(loaded.ensembleLibrary()->count(), 1);
        QCOMPARE(loaded.ensembleLibrary()->ensembles().first().name(), QString("Kids"));
    }

    void show_loadsLegacy_withoutEnsembles() {
        QJsonObject legacy;
        legacy["version"] = "1.2";
        legacy["name"] = "Legacy";
        legacy["cues"] = QJsonArray();

        Show show;
        show.ensembleLibrary()->addEnsemble(Ensemble("Stale"));
        show.fromJson(legacy);

        QCOMPARE(show.ensembleLibrary()->count(), 0); // cleared on load
        QVERIFY(!show.isModified());
    }
};

QTEST_MAIN(TestEnsembles)
#include "test_ensembles.moc"
