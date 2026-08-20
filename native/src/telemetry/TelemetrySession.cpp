#include "telemetry/TelemetrySession.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {

std::optional<double> TelemetrySession::valueAt(
    const QString &channelName,
    const double time,
    const InterpolationMode mode) const
{
    const QString resolved = aliases.value(channelName, channelName);
    const auto channelIterator = channels.constFind(resolved);
    if (channelIterator == channels.cend() || channelIterator->timestamps.isEmpty()
        || !std::isfinite(time)) {
        return std::nullopt;
    }

    const TelemetryChannel &channel = channelIterator.value();
    const auto &timestamps = channel.timestamps;
    const auto &values = channel.values;
    if (time <= timestamps.front()) {
        return values.front();
    }
    if (time >= timestamps.back()) {
        return values.back();
    }

    const auto nextIterator = std::lower_bound(timestamps.cbegin(), timestamps.cend(), time);
    const qsizetype next = std::distance(timestamps.cbegin(), nextIterator);
    if (*nextIterator == time) {
        return values[next];
    }
    const qsizetype previous = next - 1;
    if (mode == InterpolationMode::Previous) {
        return values[previous];
    }
    if (mode == InterpolationMode::Nearest) {
        return time - timestamps[previous] <= timestamps[next] - time ? values[previous]
                                                                       : values[next];
    }
    const double span = timestamps[next] - timestamps[previous];
    const double ratio = span == 0.0 ? 0.0 : (time - timestamps[previous]) / span;
    return values[previous] + (values[next] - values[previous]) * ratio;
}

QStringList TelemetrySession::channelNames() const
{
    QStringList names = channels.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

double videoToTelemetryTime(const double videoTime, const SyncTransform &transform)
{
    return videoTime * transform.timeScale + transform.offset;
}

} // namespace FlappedEar
