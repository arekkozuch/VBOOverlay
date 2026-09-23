// Placeholder target for the shared track-progress alignment work (KAN-31/32/33):
// projecting two independently recorded laps onto a common, gate-anchored,
// crossing-safe arc-length axis. Kept as its own executable rather than folded
// into TelemetryTests.cpp, which is already an oversized monolith. Real fixtures
// land alongside native/src/telemetry/TrackProgress.{h,cpp}.

#include "EventProjectFixture.h"

#include <QtTest>

class TrackProgressTests final : public QObject {
    Q_OBJECT
private slots:
    void placeholder();
};

void TrackProgressTests::placeholder()
{
    QVERIFY(true);
}

QTEST_GUILESS_MAIN(TrackProgressTests)
#include "TrackProgressTests.moc"
