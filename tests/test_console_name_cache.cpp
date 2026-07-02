#include "core/ConsoleNameCache.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestConsoleNameCache : public QObject {
    Q_OBJECT

  private slots:
    void setAndGet() {
        ConsoleNameCache c;
        c.setSnippetName(3, "Reverb");
        c.setSceneName(1, "Act 1");
        QCOMPARE(c.snippetName(3), QString("Reverb"));
        QCOMPARE(c.sceneName(1), QString("Act 1"));
        QVERIFY(c.snippetName(99).isEmpty());
        QVERIFY(!c.isEmpty());
    }

    void roundTrip() {
        ConsoleNameCache c;
        c.setSnippetName(3, "Reverb");
        c.setSnippetName(4, "Delay");
        c.setSceneName(1, "Act 1");

        ConsoleNameCache restored;
        restored.loadFromJson(c.toJson());
        QCOMPARE(restored.snippetName(3), QString("Reverb"));
        QCOMPARE(restored.snippetName(4), QString("Delay"));
        QCOMPARE(restored.sceneName(1), QString("Act 1"));
    }

    void clearEmpties() {
        ConsoleNameCache c;
        c.setSnippetName(1, "x");
        QVERIFY(!c.isEmpty());
        c.clear();
        QVERIFY(c.isEmpty());
    }
};

QTEST_MAIN(TestConsoleNameCache)
#include "test_console_name_cache.moc"
