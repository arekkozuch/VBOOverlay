#include "telemetry/TelemetrySource.h"
#include "telemetry/RczParser.h"
#include "telemetry/VboParser.h"
#include <QFileInfo>
namespace FlappedEar {
bool TelemetrySource::supportsPath(const QString &path)
{
    const auto suffix = QFileInfo(path).suffix().toLower();
    return suffix == "vbo" || suffix == "rcz";
}
TelemetrySession TelemetrySource::load(const QString &path, const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "vbo") return VboParser::parseFile(path, cancelled);
    if (suffix == "rcz") return RczParser::parseFile(path, cancelled);
    throw std::runtime_error("Choose a VBO or RaceChrono RCZ telemetry file.");
}
}
