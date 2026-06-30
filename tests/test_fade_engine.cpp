#include "core/FadeEngine.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestFadeEngine : public QObject {
    Q_OBJECT

  private slots:
    void linearFade_interpolatesAndCompletes() {
        FadeEngine fe;
        QVector<double> vals;
        QSignalSpy done(&fe, &FadeEngine::allCompleted);

        fe.start("x", 0.0, 1.0, 100.0, FadeCurve::Linear, [&](double v) { vals.append(v); });
        QVERIFY(!vals.isEmpty());
        QCOMPARE(vals.last(), 0.0); // 'from' applied immediately
        QVERIFY(fe.isActive());

        fe.advance(50.0);
        QVERIFY(qAbs(vals.last() - 0.5) < 1e-9);

        fe.advance(50.0);
        QVERIFY(qAbs(vals.last() - 1.0) < 1e-9);
        QVERIFY(!fe.isActive());
        QCOMPARE(done.count(), 1);
    }

    void instantFade_appliesTargetImmediately() {
        FadeEngine fe;
        double v = -1.0;
        fe.start("x", 0.0, 1.0, 0.0, FadeCurve::Linear, [&](double x) { v = x; });
        QCOMPARE(v, 1.0);
        QVERIFY(!fe.isActive());
    }

    void easeInOut_isSmoothAndSymmetric() {
        QCOMPARE(FadeEngine::ease(FadeCurve::EaseInOut, 0.0), 0.0);
        QCOMPARE(FadeEngine::ease(FadeCurve::EaseInOut, 1.0), 1.0);
        QVERIFY(qAbs(FadeEngine::ease(FadeCurve::EaseInOut, 0.5) - 0.5) < 1e-9);
    }

    void curve_clampsOutOfRangeT() {
        QCOMPARE(FadeEngine::ease(FadeCurve::Linear, -1.0), 0.0);
        QCOMPARE(FadeEngine::ease(FadeCurve::Linear, 2.0), 1.0);
    }

    void restart_replacesInflightFade() {
        FadeEngine fe;
        double v = -1.0;
        fe.start("x", 0.0, 1.0, 100.0, FadeCurve::Linear, [&](double x) { v = x; });
        fe.advance(50.0); // ~0.5
        fe.start("x", 0.0, 0.2, 100.0, FadeCurve::Linear, [&](double x) { v = x; });
        QCOMPARE(fe.activeCount(), 1); // still one fade for key "x"
        QCOMPARE(v, 0.0);              // restart applies new 'from'
        fe.advance(100.0);
        QVERIFY(qAbs(v - 0.2) < 1e-9);
    }

    void overshoot_clampsToTarget() {
        FadeEngine fe;
        double v = -1.0;
        fe.start("x", 0.0, 1.0, 100.0, FadeCurve::Linear, [&](double x) { v = x; });
        fe.advance(5000.0); // way past the end
        QVERIFY(qAbs(v - 1.0) < 1e-9);
        QVERIFY(!fe.isActive());
    }

    void curveRoundTrip_stringConversion() {
        for (FadeCurve c : {FadeCurve::Linear, FadeCurve::EaseInOut, FadeCurve::EaseIn,
                            FadeCurve::EaseOut}) {
            QCOMPARE(stringToFadeCurve(fadeCurveToString(c)), c);
        }
    }
};

QTEST_MAIN(TestFadeEngine)
#include "test_fade_engine.moc"
