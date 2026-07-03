#include "ui/CastTextParse.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestCastTextParse : public QObject {
    Q_OBJECT

  private slots:
    // --- parseRoles --------------------------------------------------------
    void roles_splitTrimSkipEmptyDedupe() {
        QCOMPARE(CastTextParse::parseRoles("Cosette, Singer #1"),
                 QStringList({"Cosette", "Singer #1"}));
        // stray commas, whitespace, and ci-duplicates collapse
        QCOMPARE(CastTextParse::parseRoles("Cosette, cosette ,, Singer #1"),
                 QStringList({"Cosette", "Singer #1"}));
        QCOMPARE(CastTextParse::parseRoles("  Eponine  "), QStringList({"Eponine"}));
        QVERIFY(CastTextParse::parseRoles("").isEmpty());
        QVERIFY(CastTextParse::parseRoles(" , ,, ").isEmpty());
    }

    // --- parseCastLines ----------------------------------------------------
    void castLines_plainNames() {
        const auto lines = CastTextParse::parseCastLines("Alice\nBob\nCarol");
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0].name, QString("Alice"));
        QVERIFY(lines[0].roles.isEmpty());
        QCOMPARE(lines[2].name, QString("Carol"));
    }

    void castLines_tabSeparatedRoles() {
        // pasting Name | Role columns from a spreadsheet produces tabs
        const auto lines =
            CastTextParse::parseCastLines("Alice\tCosette, Singer #1\nBob\tMarius");
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines[0].name, QString("Alice"));
        QCOMPARE(lines[0].roles, QStringList({"Cosette", "Singer #1"}));
        QCOMPARE(lines[1].roles, QStringList({"Marius"}));
    }

    void castLines_extraTabCellsAppend() {
        const auto lines = CastTextParse::parseCastLines("Alice\tCosette\tSinger #1, cosette");
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines[0].roles, QStringList({"Cosette", "Singer #1"}));
    }

    void castLines_blankAndWhitespaceLinesSkipped() {
        const auto lines = CastTextParse::parseCastLines("Alice\n\n   \n\tOrphanRole\nBob");
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines[0].name, QString("Alice"));
        QCOMPARE(lines[1].name, QString("Bob"));
    }

    void castLines_crlfInput() {
        const auto lines = CastTextParse::parseCastLines("Alice\tCosette\r\nBob\r\n");
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines[0].name, QString("Alice"));
        QCOMPARE(lines[0].roles, QStringList({"Cosette"}));
        QCOMPARE(lines[1].name, QString("Bob"));
    }
};

QTEST_MAIN(TestCastTextParse)
#include "test_cast_text_parse.moc"
