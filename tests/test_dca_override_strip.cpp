#include "ui/DCAOverrideStrip.h"
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestDCAOverrideStrip : public QObject {
    Q_OBJECT

  private slots:
    void roundTrips_allStates() {
        DCAOverrideStrip strip(3, nullptr);

        // none
        QVERIFY(!strip.overrideValue().mute.has_value());
        QVERIFY(!strip.overrideValue().label.has_value());

        DCAOverride muted;
        muted.mute = true;
        strip.setOverride(muted);
        QCOMPARE(strip.overrideValue().mute, std::optional<bool>(true));

        DCAOverride unmuted;
        unmuted.mute = false;
        strip.setOverride(unmuted);
        QCOMPARE(strip.overrideValue().mute, std::optional<bool>(false));

        DCAOverride full;
        full.mute = true;
        full.label = "Anna";
        strip.setOverride(full);
        QCOMPARE(strip.overrideValue().mute, std::optional<bool>(true));
        QCOMPARE(strip.overrideValue().label, std::optional<QString>("Anna"));
        QCOMPARE(strip.labelText(), QString("Anna"));
    }

    void muteButton_cyclesTriState() {
        DCAOverrideStrip strip(1, nullptr);
        QSignalSpy edited(&strip, &DCAOverrideStrip::overrideEdited);
        auto* button = strip.findChild<QPushButton*>();
        QVERIFY(button);

        button->click(); // none -> muted
        QCOMPARE(strip.overrideValue().mute, std::optional<bool>(true));
        button->click(); // muted -> unmuted
        QCOMPARE(strip.overrideValue().mute, std::optional<bool>(false));
        button->click(); // unmuted -> none
        QVERIFY(!strip.overrideValue().mute.has_value());
        QCOMPARE(edited.count(), 3);
    }

    void setOverride_doesNotEmit() {
        DCAOverrideStrip strip(2, nullptr);
        QSignalSpy edited(&strip, &DCAOverrideStrip::overrideEdited);
        DCAOverride override;
        override.mute = true;
        override.label = "Elle";
        strip.setOverride(override);
        QCOMPARE(edited.count(), 0);
    }

    void labelEdit_emptyMeansNoOverride() {
        DCAOverrideStrip strip(4, nullptr);
        auto* edit = strip.findChild<QLineEdit*>();
        QVERIFY(edit);

        QSignalSpy edited(&strip, &DCAOverrideStrip::overrideEdited);
        edit->setText("Cosette");
        QCOMPARE(strip.overrideValue().label, std::optional<QString>("Cosette"));
        QVERIFY(edited.count() >= 1);

        edit->clear();
        QVERIFY(!strip.overrideValue().label.has_value());
    }

    void editingFinished_emitsLabelCommitted() {
        DCAOverrideStrip strip(5, nullptr);
        auto* edit = strip.findChild<QLineEdit*>();
        QVERIFY(edit);

        QSignalSpy committed(&strip, &DCAOverrideStrip::labelCommitted);
        edit->setText("Marius");
        emit edit->editingFinished();
        QCOMPARE(committed.count(), 1);
        QCOMPARE(committed.first().first().toInt(), 5);
    }
};

QTEST_MAIN(TestDCAOverrideStrip)
#include "test_dca_override_strip.moc"
