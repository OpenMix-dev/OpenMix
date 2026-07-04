#include "core/ActorProfileLibrary.h"
#include "core/CueList.h"
#include "core/Position.h"
#include "core/Show.h"
#include "io/TmixImporter.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestTmixImport : public QObject {
    Q_OBJECT

  private slots:
    void testImportPopulatesShow() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("show.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixfixture");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE config (param TEXT PRIMARY KEY, value TEXT)"));
            QVERIFY(q.exec("INSERT INTO config VALUES('designer','Jane Doe')"));
            QVERIFY(q.exec("INSERT INTO config VALUES('selectOnSpill','1')"));
            QVERIFY(q.exec("CREATE TABLE actors (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, `order` INTEGER, active INTEGER)"));
            QVERIFY(q.exec("INSERT INTO actors (channel,name,`order`,active) VALUES(3,'Alice',0,1)"));
            QVERIFY(q.exec("CREATE TABLE positions (id INTEGER PRIMARY KEY, name TEXT, "
                           "shortName TEXT, delay NUMERIC, pan NUMERIC)"));
            QVERIFY(q.exec("INSERT INTO positions (id,name,shortName,delay,pan) "
                           "VALUES(0,'Center Stage','CS',12,-0.5)"));
            QVERIFY(q.exec("CREATE TABLE cues (number INTEGER, point INTEGER, name TEXT, "
                           "dca01Channels TEXT, dca01Label TEXT)"));
            QVERIFY(q.exec("INSERT INTO cues (number,point,name,dca01Channels,dca01Label) "
                           "VALUES(1,0,'Opening','3,4','Vox')"));
            QVERIFY(q.exec("INSERT INTO cues (number,point,name) VALUES(2,0,'Second')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixfixture");

        Show show;
        TmixImporter importer;
        QString err;
        QVERIFY2(importer.import(path, &show, &err), qPrintable(err));

        QCOMPARE(show.designer(), QString("Jane Doe"));
        QVERIFY(show.selectOnSpill());

        QCOMPARE(show.cueList()->count(), 2);
        QCOMPARE(show.cueList()->at(0).name(), QString("Opening"));
        QCOMPARE(show.cueList()->at(0).number(), 1.0);
        QVERIFY(show.cueList()->at(0).hasCustomDCAMapping());
        QCOMPARE(show.cueList()->at(0).dcaOverride(1).label.value_or(QString()), QString("Vox"));

        QCOMPARE(show.positionLibrary()->count(), 1);
        QCOMPARE(show.positionLibrary()->positions().first().shortName(), QString("CS"));

        QCOMPARE(show.actorProfileLibrary()->actors().size(), 1);
        QCOMPARE(show.actorProfileLibrary()->actors().first().name(), QString("Alice"));
        QCOMPARE(show.actorProfileLibrary()->actors().first().channel(), 3);
        // 'Vox' labels a two-channel DCA: a group name, never a role
        QVERIFY(show.actorProfileLibrary()->actors().first().roles().isEmpty());
    }

    void testRoleBackfillFromCueDcaLabels() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("roles.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixroles");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE actors (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, `order` INTEGER, active INTEGER)"));
            QVERIFY(q.exec("INSERT INTO actors (channel,name,`order`,active) VALUES(3,'Alice',0,1)"));
            QVERIFY(q.exec("INSERT INTO actors (channel,name,`order`,active) VALUES(4,'Bob',1,1)"));
            QVERIFY(q.exec("INSERT INTO actors (channel,name,`order`,active) VALUES(5,'Cleo',2,1)"));
            QVERIFY(q.exec("CREATE TABLE cues (number INTEGER, point INTEGER, name TEXT, "
                           "dca01Channels TEXT, dca01Label TEXT, "
                           "dca02Channels TEXT, dca02Label TEXT, "
                           "dca03Channels TEXT, dca03Label TEXT)"));
            // ch3 always labeled 'Cosette' (single channel) -> role
            // ch4 labeled 'Marius' then 'Valjean' -> conflicting, no role
            // ch5 only ever in a multi-channel DCA 'ENS' -> no role
            QVERIFY(q.exec("INSERT INTO cues VALUES(1,0,'One','3','Cosette','4','Marius','5,6','ENS')"));
            QVERIFY(q.exec("INSERT INTO cues VALUES(2,0,'Two','3','Cosette','4','Valjean','5,6','ENS')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixroles");

        Show show;
        TmixImporter importer;
        QString err;
        TmixImportSummary summary;
        QVERIFY2(importer.import(path, &show, &err, &summary), qPrintable(err));

        const ActorProfileLibrary* lib = show.actorProfileLibrary();
        QCOMPARE(lib->actorForChannel(3)->roles(), QStringList({"Cosette"}));
        QVERIFY(lib->actorForChannel(4)->roles().isEmpty());
        QVERIFY(lib->actorForChannel(5)->roles().isEmpty());
        QCOMPARE(summary.rolesInferred, 1);
        QCOMPARE(summary.actors, 3);
        QCOMPARE(summary.cues, 2);
    }

    void testChannelNamesFromDefaultProfiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("names.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixnames");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            // real files ship an empty actors table; the cast lives in profiles
            QVERIFY(q.exec("CREATE TABLE actors (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, `order` INTEGER, active INTEGER)"));
            QVERIFY(q.exec("CREATE TABLE profiles (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, label TEXT, `default` INTEGER, data TEXT)"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(1,3,'Alice','',1,'')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(2,3,'Umbrella','',0,'')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(3,4,'Bob','',1,'')"));
            QVERIFY(q.exec("CREATE TABLE cues (number INTEGER, point INTEGER, name TEXT, "
                           "channelProfiles TEXT)"));
            QVERIFY(q.exec("INSERT INTO cues VALUES(1,0,'One','3=2,4=3')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixnames");

        Show show;
        TmixImporter importer;
        QString err;
        TmixImportSummary summary;
        QVERIFY2(importer.import(path, &show, &err, &summary), qPrintable(err));

        const ActorProfileLibrary* lib = show.actorProfileLibrary();
        QCOMPARE(lib->actors().size(), 2);
        QVERIFY(lib->actorForChannel(3));
        QCOMPARE(lib->actorForChannel(3)->name(), QString("Alice"));
        QVERIFY(lib->actorForChannel(4));
        QCOMPARE(lib->actorForChannel(4)->name(), QString("Bob"));
        // only the non-default preset becomes a slot
        QCOMPARE(lib->profileSlots(), QStringList({"Main", "Umbrella"}));
        QCOMPARE(summary.actors, 2);
        QCOMPARE(summary.channelNames, 2);
        QCOMPARE(summary.profileSlots, QStringList({"Umbrella"}));
        // cue references resolve: non-default id to its slot, default id to Main
        QCOMPARE(show.cueList()->at(0).channelProfiles().value(3), QString("Umbrella"));
        QCOMPARE(show.cueList()->at(0).channelProfiles().value(4), QString("Main"));
    }

    void testChannelNamesWithoutLabelColumn() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("oldnames.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixoldnames");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            // older files: profiles has no label column
            QVERIFY(q.exec("CREATE TABLE profiles (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, `default` INTEGER, data TEXT)"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(1,1,'Barry',1,'')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(2,2,'Trent',1,'')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixoldnames");

        Show show;
        TmixImporter importer;
        QString err;
        TmixImportSummary summary;
        QVERIFY2(importer.import(path, &show, &err, &summary), qPrintable(err));

        const ActorProfileLibrary* lib = show.actorProfileLibrary();
        QCOMPARE(lib->actors().size(), 2);
        QCOMPARE(lib->actorForChannel(1)->name(), QString("Barry"));
        QCOMPARE(lib->actorForChannel(2)->name(), QString("Trent"));
        QCOMPARE(lib->profileSlots(), QStringList({"Main"}));
        QVERIFY(summary.profileSlots.isEmpty());
        QCOMPARE(summary.channelNames, 2);
    }

    void testActorsTableNameWins() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("actorswin.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixactorswin");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE actors (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, `order` INTEGER, active INTEGER)"));
            QVERIFY(q.exec("INSERT INTO actors (channel,name,`order`,active) VALUES(3,'Alice',0,1)"));
            QVERIFY(q.exec("INSERT INTO actors (channel,name,`order`,active) VALUES(5,'',1,1)"));
            QVERIFY(q.exec("CREATE TABLE profiles (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT, label TEXT, `default` INTEGER, data TEXT)"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(1,3,'Wrong','',1,'')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(2,5,'Eve','',1,'')"));
            QVERIFY(q.exec("CREATE TABLE cues (number INTEGER, point INTEGER, name TEXT, "
                           "channelProfiles TEXT)"));
            QVERIFY(q.exec("INSERT INTO cues VALUES(1,0,'One','3=1')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixactorswin");

        Show show;
        TmixImporter importer;
        QString err;
        TmixImportSummary summary;
        QVERIFY2(importer.import(path, &show, &err, &summary), qPrintable(err));

        const ActorProfileLibrary* lib = show.actorProfileLibrary();
        QCOMPARE(lib->actors().size(), 2); // filled in, not duplicated
        QCOMPARE(lib->actorForChannel(3)->name(), QString("Alice"));
        QCOMPARE(lib->actorForChannel(5)->name(), QString("Eve"));
        QCOMPARE(summary.actors, 2);
        QCOMPARE(summary.channelNames, 1);
        QCOMPARE(show.cueList()->at(0).channelProfiles().value(3), QString("Main"));
    }

    void testFlatProfilesStillBecomeSlots() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("flat.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixflat");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            // no channel column: every label is a show-wide slot
            QVERIFY(q.exec("CREATE TABLE profiles (id INTEGER PRIMARY KEY, label TEXT)"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(1,'Loud')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(2,'Soft')"));
            QVERIFY(q.exec("CREATE TABLE cues (number INTEGER, point INTEGER, name TEXT, "
                           "channelProfiles TEXT)"));
            QVERIFY(q.exec("INSERT INTO cues VALUES(1,0,'One','3=1')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixflat");

        Show show;
        TmixImporter importer;
        QString err;
        TmixImportSummary summary;
        QVERIFY2(importer.import(path, &show, &err, &summary), qPrintable(err));

        QCOMPARE(show.actorProfileLibrary()->profileSlots(), QStringList({"Main", "Loud", "Soft"}));
        QVERIFY(show.actorProfileLibrary()->actors().isEmpty());
        QCOMPARE(summary.channelNames, 0);
        QCOMPARE(show.cueList()->at(0).channelProfiles().value(3), QString("Loud"));
    }

    void testPerChannelProfilesWithoutDefaultFlag() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("nodefault.tmix");

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixnodefault");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            // no default column: the first row per channel is the default
            QVERIFY(q.exec("CREATE TABLE profiles (id INTEGER PRIMARY KEY, channel INTEGER, "
                           "name TEXT)"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(1,3,'Alice')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(2,3,'Umbrella')"));
            QVERIFY(q.exec("INSERT INTO profiles VALUES(3,4,'Bob')"));
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixnodefault");

        Show show;
        TmixImporter importer;
        QString err;
        TmixImportSummary summary;
        QVERIFY2(importer.import(path, &show, &err, &summary), qPrintable(err));

        const ActorProfileLibrary* lib = show.actorProfileLibrary();
        QCOMPARE(lib->actors().size(), 2);
        QCOMPARE(lib->actorForChannel(3)->name(), QString("Alice"));
        QCOMPARE(lib->actorForChannel(4)->name(), QString("Bob"));
        QCOMPARE(lib->profileSlots(), QStringList({"Main", "Umbrella"}));
    }

    void testImportMissingFileFails() {
        Show show;
        TmixImporter importer;
        QString err;
        // opening a nonexistent database path still "opens" in sqlite (creates it);
        // an unreadable path is the real failure. Assert import never crashes and
        // leaves an empty show when there are no tables.
        QTemporaryDir dir;
        const QString path = dir.filePath("empty.tmix");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tmixempty");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            db.close();
        }
        QSqlDatabase::removeDatabase("tmixempty");
        QVERIFY(importer.import(path, &show, &err));
        QCOMPARE(show.cueList()->count(), 0);
    }
};

QTEST_MAIN(TestTmixImport)
#include "test_tmix_import.moc"
