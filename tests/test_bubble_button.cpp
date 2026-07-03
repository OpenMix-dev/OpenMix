#include "ui/BubbleBar.h"
#include "ui/BubbleButton.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestBubbleButton : public QObject {
    Q_OBJECT

  private slots:
    void clickDoesNotSelfActivate();
    void setButtonActiveDrivesVisualState();
};

// visual active state must be driven only by setButtonActive (visibility
// sync), never by the click itself — a checkable button drifted out of step
// with the window it toggles
void TestBubbleButton::clickDoesNotSelfActivate() {
    BubbleBar bar;
    BubbleButton* button = bar.addButton("test", QString::fromUtf8("★"), "Test");
    bar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bar));

    QSignalSpy clickedSpy(&bar, &BubbleBar::buttonClicked);
    QTest::mouseClick(button, Qt::LeftButton);

    QCOMPARE(clickedSpy.count(), 1);
    QCOMPARE(clickedSpy.takeFirst().at(0).toString(), QString("test"));
    QVERIFY(!button->isActive());
    QVERIFY(!button->isChecked());
    QVERIFY(!button->property("active").toBool());
}

void TestBubbleButton::setButtonActiveDrivesVisualState() {
    BubbleBar bar;
    BubbleButton* button = bar.addButton("test", QString::fromUtf8("★"), "Test");

    bar.setButtonActive("test", true);
    QVERIFY(button->isActive());
    QVERIFY(button->property("active").toBool());

    bar.setButtonActive("test", false);
    QVERIFY(!button->isActive());
    QVERIFY(!button->property("active").toBool());
}

QTEST_MAIN(TestBubbleButton)
#include "test_bubble_button.moc"
