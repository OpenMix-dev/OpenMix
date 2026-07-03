#include "ui/DcaGridNav.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestDcaGridNav : public QObject {
    Q_OBJECT

  private slots:
    // channel grid: 32 items, 16 rows per block -> 2 blocks
    void movesWithinBlock() {
        QCOMPARE(DcaGridNav::move(0, 32, 16, 1, 0), 1);   // ch1 down -> ch2
        QCOMPARE(DcaGridNav::move(5, 32, 16, -1, 0), 4);  // ch6 up -> ch5
    }

    void movesAcrossBlocks() {
        QCOMPARE(DcaGridNav::move(0, 32, 16, 0, 1), 16);  // ch1 right -> ch17
        QCOMPARE(DcaGridNav::move(16, 32, 16, 0, -1), 0); // ch17 left -> ch1
    }

    void edgesClamp() {
        QCOMPARE(DcaGridNav::move(0, 32, 16, -1, 0), -1);  // top row up
        QCOMPARE(DcaGridNav::move(15, 32, 16, 1, 0), -1);  // block bottom down
        QCOMPARE(DcaGridNav::move(0, 32, 16, 0, -1), -1);  // first block left
        QCOMPARE(DcaGridNav::move(16, 32, 16, 0, 1), -1);  // last block right
    }

    void partialLastBlock() {
        // 20 items, 16 rows: second block has rows 0..3 only
        QCOMPARE(DcaGridNav::move(3, 20, 16, 0, 1), 19);  // row 3 right -> exists
        QCOMPARE(DcaGridNav::move(4, 20, 16, 0, 1), -1);  // row 4 right -> hole
        QCOMPARE(DcaGridNav::move(19, 20, 16, 1, 0), -1); // last item down
    }

    void invalidInputs() {
        QCOMPARE(DcaGridNav::move(-1, 32, 16, 1, 0), -1);
        QCOMPARE(DcaGridNav::move(32, 32, 16, 1, 0), -1);
        QCOMPARE(DcaGridNav::move(0, 32, 0, 1, 0), -1);
    }

    void digitKeysMapToComboRows() {
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_1, 8), 1);
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_8, 8), 8);
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_9, 8), -1); // beyond dcaCount
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_9, 24), 9);
    }

    void clearKeysMapToNone() {
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_0, 8), 0);
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_Delete, 8), 0);
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_Backspace, 8), 0);
    }

    void otherKeysIgnored() {
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_A, 8), -1);
        QCOMPARE(DcaGridNav::comboRowForKey(Qt::Key_Enter, 8), -1);
    }
};

QTEST_MAIN(TestDcaGridNav)
#include "test_dca_grid_nav.moc"
