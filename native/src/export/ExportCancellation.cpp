#include "export/ExportCancellation.h"

#include "export/ExportProcessSupervisor.h"

#include <QFile>

namespace FlappedEar {

ExportCancellationResult ExportCancellation::request(
    const QString &markerPath,
    ExportProcessSupervisor *supervisor,
    MarkerWriter markerWriter)
{
    ExportCancellationResult result;
    const auto writeMarker = markerWriter ? std::move(markerWriter)
        : [](const QString &path, QString *error) {
              QFile marker(path);
              if (marker.open(QIODevice::WriteOnly)) {
                  marker.close();
                  return true;
              }
              if (error) *error = marker.errorString();
              return false;
          };
    result.markerCreated = writeMarker(markerPath, &result.error);
    if (result.markerCreated) return result;

    if (!supervisor) {
        if (result.error.isEmpty()) result.error = QStringLiteral("Export process supervision is unavailable.");
        return result;
    }
    result.workerStopped = supervisor->stopAndWait();
    if (!result.workerStopped && result.error.isEmpty()) {
        result.error = QStringLiteral("Export worker did not stop after cancellation marker creation failed.");
    }
    return result;
}

} // namespace FlappedEar
