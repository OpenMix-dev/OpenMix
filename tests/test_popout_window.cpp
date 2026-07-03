#include "ui/PopOutWindow.h"

#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestPopOutWindow : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void enterDoesNotClickButtonsOrHide();
    void escapeClosesViaProperPath();
    void visibilityChangedFiresOncePerTransition();
    void showAndRestoreUnminimizes();

  private:
    QTemporaryDir m_settingsDir;
};

void TestPopOutWindow::initTestCase() {
    // keep geometry writes out of the real user config
    QVERIFY(m_settingsDir.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

// Enter in a field must commit the edit, never click a button or hide the
// window (both happened while PopOutWindow was a QDialog: autoDefault buttons
// captured Return)
void TestPopOutWindow::enterDoesNotClickButtonsOrHide() {
    PopOutWindow window("test", "Test");

    QWidget* content = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(content);
    QLineEdit* edit = new QLineEdit(content);
    QPushButton* button = new QPushButton("Save", content);
    layout->addWidget(edit);
    layout->addWidget(button);
    window.setContentWidget(content);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QSignalSpy clickedSpy(button, &QPushButton::clicked);
    edit->setFocus();
    QTest::keyClick(edit, Qt::Key_Return);
    QTest::keyClick(edit, Qt::Key_Enter);

    QCOMPARE(clickedSpy.count(), 0);
    QVERIFY(window.isVisible());
}

void TestPopOutWindow::escapeClosesViaProperPath() {
    PopOutWindow window("test", "Test");
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QSignalSpy closedSpy(&window, &PopOutWindow::closed);
    QSignalSpy visibilitySpy(&window, &PopOutWindow::visibilityChanged);

    QTest::keyClick(&window, Qt::Key_Escape);

    QVERIFY(!window.isVisible());
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);
    QCOMPARE(visibilitySpy.takeFirst().at(0).toBool(), false);
}

void TestPopOutWindow::visibilityChangedFiresOncePerTransition() {
    PopOutWindow window("test", "Test");
    QSignalSpy spy(&window, &PopOutWindow::visibilityChanged);

    window.show();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);

    window.hide();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), false);

    window.hide(); // already hidden: no event
    QCOMPARE(spy.count(), 0);
}

void TestPopOutWindow::showAndRestoreUnminimizes() {
    PopOutWindow window("test", "Test");
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.setWindowState(window.windowState() | Qt::WindowMinimized);
    QVERIFY(!window.isEffectivelyVisible());

    window.showAndRestore();
    QVERIFY(!(window.windowState() & Qt::WindowMinimized));
    QVERIFY(window.isEffectivelyVisible());
}

QTEST_MAIN(TestPopOutWindow)
#include "test_popout_window.moc"
