#include "midi/MidiInputManager.h"
#include "ui/MidiConfigDialog.h"
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestMidiConfigDialog : public QObject {
    Q_OBJECT

    static MidiTrigger makeTrigger(int note) {
        MidiTrigger t;
        t.type = MidiMessageType::NoteOn;
        t.channel = 0;
        t.noteOrCC = note;
        t.minValue = 1;
        return t;
    }

    static QTableWidget* tableIn(MidiConfigDialog& dialog, int which) {
        // 0 = action mappings, 1 = mute assignments (creation order)
        const auto tables = dialog.findChildren<QTableWidget*>();
        Q_ASSERT(tables.size() >= 2);
        return tables[which];
    }

  private slots:
    void initTestCase() {
        // keep QSettings writes inside the test sandbox
        QVERIFY(m_settingsDir.isValid());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName("OpenMixTest");
        QCoreApplication::setApplicationName("MidiConfigDialogTest");
    }

    // regression: removing a row above another must not make later combo edits
    // hit the wrong mapping (the old lambdas captured the row index by value)
    void removeRow_thenEditCombo_editsCorrectMapping() {
        MidiInputManager manager;
        MidiMapping a{makeTrigger(1), MidiAction::Go};
        MidiMapping b{makeTrigger(2), MidiAction::Stop};
        MidiMapping c{makeTrigger(3), MidiAction::Next};
        manager.setMappings({a, b, c});

        MidiConfigDialog dialog(&manager, 32);
        QTableWidget* table = tableIn(dialog, 0);
        QCOMPARE(table->rowCount(), 3);

        // remove the middle row via its remove button
        auto* removeBtn = qobject_cast<QToolButton*>(table->cellWidget(1, 2));
        QVERIFY(removeBtn);
        removeBtn->click();
        QCOMPARE(table->rowCount(), 2);

        // row 1 is now mapping C; change its action to Panic
        auto* combo = qobject_cast<QComboBox*>(table->cellWidget(1, 1));
        QVERIFY(combo);
        combo->setCurrentIndex(combo->findData(static_cast<int>(MidiAction::Panic)));

        dialog.accept();

        const QVector<MidiMapping> result = manager.mappings();
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0].action, MidiAction::Go);
        QCOMPARE(result[0].trigger.noteOrCC, 1);
        QCOMPARE(result[1].action, MidiAction::Panic); // C, edited
        QCOMPARE(result[1].trigger.noteOrCC, 3);       // C's trigger untouched
    }

    void muteAssignments_addEditRemovePersist() {
        MidiInputManager manager;
        MidiConfigDialog dialog(&manager, 32);
        QTableWidget* mutes = tableIn(dialog, 1);
        QCOMPARE(mutes->rowCount(), 0);

        // add + set channel 12
        auto buttons = dialog.findChildren<QPushButton*>();
        QPushButton* addMute = nullptr;
        for (QPushButton* b : buttons) {
            if (b->text() == "Add Assignment")
                addMute = b;
        }
        QVERIFY(addMute);
        addMute->click();
        QCOMPARE(mutes->rowCount(), 1);

        auto* spin = qobject_cast<QSpinBox*>(mutes->cellWidget(0, 1));
        QVERIFY(spin);
        spin->setValue(12);
        dialog.accept();

        QCOMPARE(manager.muteAssignments().size(), 1);
        QCOMPARE(manager.muteAssignments()[0].channel, 12);

        // reopen: the assignment shows; remove it and accept
        MidiConfigDialog dialog2(&manager, 32);
        QTableWidget* mutes2 = tableIn(dialog2, 1);
        QCOMPARE(mutes2->rowCount(), 1);
        auto* removeBtn = qobject_cast<QToolButton*>(mutes2->cellWidget(0, 2));
        QVERIFY(removeBtn);
        removeBtn->click();
        dialog2.accept();
        QVERIFY(manager.muteAssignments().isEmpty());
    }

    void channelSpin_respectsChannelCount() {
        MidiInputManager manager;
        MidiMuteAssignment assignment;
        assignment.trigger = makeTrigger(9);
        assignment.channel = 4;
        manager.setMuteAssignments({assignment});

        MidiConfigDialog dialog(&manager, 48);
        auto* spin = qobject_cast<QSpinBox*>(tableIn(dialog, 1)->cellWidget(0, 1));
        QVERIFY(spin);
        QCOMPARE(spin->maximum(), 48);
        QCOMPARE(spin->minimum(), 1);
    }

    void learnWithoutDevice_staysDisarmed() {
        MidiInputManager manager; // no device open in a headless test
        MidiConfigDialog dialog(&manager, 32);

        QPushButton* learn = nullptr;
        for (QPushButton* b : dialog.findChildren<QPushButton*>()) {
            if (b->text() == "MIDI Learn") {
                learn = b;
                break;
            }
        }
        QVERIFY(learn);

        // suppress the modal warning: QMessageBox::warning spins its own event
        // loop; use a queued close
        QTimer::singleShot(0, &dialog, []() {
            for (QWidget* w : QApplication::topLevelWidgets()) {
                if (auto* box = qobject_cast<QMessageBox*>(w))
                    box->close();
            }
        });
        learn->click();

        QCOMPARE(learn->text(), QString("MIDI Learn")); // never flipped to Listening...
    }

  private:
    QTemporaryDir m_settingsDir;
};

QTEST_MAIN(TestMidiConfigDialog)
#include "test_midi_config_dialog.moc"
