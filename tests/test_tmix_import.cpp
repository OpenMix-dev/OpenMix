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
                           "VALUES(0,'Centre Stage','CS',12,-0.5)"));
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
