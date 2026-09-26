// Braking, apex, exit and racing-line variability (native/src/telemetry/
// DrivingVariability.*, KAN-63).

#include "telemetry/DrivingVariability.h"

#include <QtTest>
#include <cmath>

using namespace FlappedEar;

namespace {

CornerLapObservation lap(const double braking, const QString &provenance, const double minimum,
    const std::optional<double> line = std::nullopt, const std::optional<double> accuracy = std::nullopt)
{
    CornerLapObservation observation;
    observation.brakingPointMeters = braking;
    observation.brakingProvenance = provenance;
    observation.minimumSpeed = minimum;
    observation.apexSpeed = minimum + 2.0;
    observation.exitSpeed = minimum + 20.0;
    observation.pickupMeters = braking + 60.0;
    observation.pickupProvenance = provenance;
    observation.lineOffsetMeters = line;
    observation.gpsAccuracyMeters = accuracy;
    return observation;
}

// A straight east-going axis: 0..100 m along x.
ProgressAxis straightAxis()
{
    ProgressAxis axis;
    for (int meters = 0; meters <= 100; meters += 10) {
        axis.points.append(QPointF(meters, 0));
        axis.cumulative.append(meters);
    }
    axis.lengthMeters = 100;
    axis.spacingMeters = 10;
    axis.valid = true;
    return axis;
}

} // namespace

class DrivingVariabilityTests final : public QObject {
    Q_OBJECT
private slots:
    void summarizesEachMetricWithCounts();
    void keepsMeasuredAndInferredApart();
    void comparesLineSpreadWithGpsAccuracy();
    void measuresSignedLateralOffset();
};

void DrivingVariabilityTests::summarizesEachMetricWithCounts()
{
    const auto result = summarizeCornerVariability("c1", "Corner 1", {
        lap(140, "measured", 50), lap(142, "measured", 52), lap(146, "measured", 51), lap(150, "measured", 49)});
    QCOMPARE(result.segmentId, QString("c1"));
    QVERIFY(result.brakingPointMeasured.available);
    QCOMPARE(result.brakingPointMeasured.count, 4);
    QVERIFY(std::abs(*result.brakingPointMeasured.median - 144.0) < 1e-9);
    QVERIFY(result.minimumSpeed.available && result.apexSpeed.available && result.exitSpeed.available);
    QVERIFY(std::abs(*result.minimumSpeed.median - 50.5) < 1e-9);
    QVERIFY(result.pickupMeasured.available);
    QVERIFY(!result.brakingPointInferred.available);
    QCOMPARE(result.brakingPointInferred.count, 0);
    QVERIFY(!result.lineOffset.available); // no line observations
    QVERIFY(!result.lineSpreadResolvable);
}

void DrivingVariabilityTests::keepsMeasuredAndInferredApart()
{
    const auto result = summarizeCornerVariability("c1", "Corner 1", {
        lap(140, "measured", 50), lap(141, "measured", 50), lap(142, "measured", 50),
        lap(100, "inferred", 50), lap(101, "inferred", 50), lap(102, "inferred", 50)});
    QCOMPARE(result.brakingPointMeasured.count, 3);
    QCOMPARE(result.brakingPointInferred.count, 3);
    QVERIFY(std::abs(*result.brakingPointMeasured.median - 141.0) < 1e-9);
    QVERIFY(std::abs(*result.brakingPointInferred.median - 101.0) < 1e-9);
    QCOMPARE(result.pickupMeasured.count, 3);
    QCOMPARE(result.pickupInferred.count, 3);
}

void DrivingVariabilityTests::comparesLineSpreadWithGpsAccuracy()
{
    // Offsets -2..+2 m (IQR 2 m) with ~0.8 m GPS accuracy: resolvable.
    auto wide = summarizeCornerVariability("c1", "Corner 1", {
        lap(1, "measured", 1, -2.0, 0.8), lap(1, "measured", 1, -1.0, 0.8), lap(1, "measured", 1, 0.0, 0.7),
        lap(1, "measured", 1, 1.0, 0.9), lap(1, "measured", 1, 2.0, 0.8)});
    QVERIFY(wide.lineOffset.available);
    QVERIFY(std::abs(*wide.lineOffset.interquartileRange - 2.0) < 1e-9);
    QVERIFY(std::abs(*wide.typicalGpsAccuracyMeters - 0.8) < 1e-9);
    QVERIFY(wide.lineSpreadResolvable);
    // The same spread with 3 m GPS accuracy cannot be told apart from noise.
    auto noisy = summarizeCornerVariability("c1", "Corner 1", {
        lap(1, "measured", 1, -2.0, 3.0), lap(1, "measured", 1, -1.0, 3.0), lap(1, "measured", 1, 0.0, 3.0),
        lap(1, "measured", 1, 1.0, 3.0), lap(1, "measured", 1, 2.0, 3.0)});
    QVERIFY(noisy.lineOffset.available);
    QVERIFY(!noisy.lineSpreadResolvable);
    // Without a stated accuracy it is never claimed resolvable.
    auto unknown = summarizeCornerVariability("c1", "Corner 1", {
        lap(1, "measured", 1, -2.0), lap(1, "measured", 1, 0.0), lap(1, "measured", 1, 2.0)});
    QVERIFY(unknown.lineOffset.available);
    QVERIFY(!unknown.typicalGpsAccuracyMeters);
    QVERIFY(!unknown.lineSpreadResolvable);
}

void DrivingVariabilityTests::measuresSignedLateralOffset()
{
    const auto axis = straightAxis();
    QVERIFY(std::abs(*lateralOffsetMeters(axis, 35.0, QPointF(35, 1.5)) - 1.5) < 1e-9);  // left of travel
    QVERIFY(std::abs(*lateralOffsetMeters(axis, 35.0, QPointF(35, -2.0)) + 2.0) < 1e-9); // right
    QVERIFY(std::abs(*lateralOffsetMeters(axis, 100.0, QPointF(100, 0.5)) - 0.5) < 1e-9);
    QVERIFY(!lateralOffsetMeters(axis, 101.0, QPointF(0, 0)));
    QVERIFY(!lateralOffsetMeters(axis, -1.0, QPointF(0, 0)));
    QVERIFY(!lateralOffsetMeters(ProgressAxis{}, 10.0, QPointF(0, 0)));
}

QTEST_GUILESS_MAIN(DrivingVariabilityTests)
#include "DrivingVariabilityTests.moc"
