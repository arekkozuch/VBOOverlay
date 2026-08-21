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
        || channelIterator->values.isEmpty() || !std::isfinite(time)) {
        return std::nullopt;
    }

    const TelemetryChannel &channel = channelIterator.value();
    const auto &timestamps = channel.timestamps;
    const auto &values = channel.values;
    if (timestamps.size() != values.size() || time < timestamps.front() || time > timestamps.back()) {
        return std::nullopt;
    }

    const auto nextIterator = std::lower_bound(timestamps.cbegin(), timestamps.cend(), time);
    const qsizetype next = std::distance(timestamps.cbegin(), nextIterator);
    if (next < 0 || next >= values.size()) {
        return std::nullopt;
    }
    const auto finiteValueAt = [&values](const qsizetype index) -> std::optional<double> {
        if (index < 0 || index >= values.size() || !std::isfinite(values[index])) {
            return std::nullopt;
        }
        return values[index];
    };
    if (*nextIterator == time) {
        return finiteValueAt(next);
    }
    if (next == 0) {
        return std::nullopt;
    }
    const qsizetype previous = next - 1;
    if (mode == InterpolationMode::Previous) {
        return finiteValueAt(previous);
    }
    if (mode == InterpolationMode::Nearest) {
        return time - timestamps[previous] <= timestamps[next] - time ? finiteValueAt(previous)
                                                                       : finiteValueAt(next);
    }
    const double span = timestamps[next] - timestamps[previous];
    const auto previousValue = finiteValueAt(previous);
    const auto nextValue = finiteValueAt(next);
    if (!previousValue || !nextValue || !std::isfinite(span) || span <= 0.0) {
        return std::nullopt;
    }
    const double ratio = (time - timestamps[previous]) / span;
    const double interpolated = *previousValue + (*nextValue - *previousValue) * ratio;
    return std::isfinite(interpolated) ? std::optional<double>(interpolated) : std::nullopt;
}

QStringList TelemetrySession::channelNames() const
{
    QStringList names = channels.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

QVector<QPointF> TelemetrySession::sampledRange(
    const QString &channelName,
    double rangeStart,
    double rangeEnd,
    const int maximumPoints) const
{
    if (!std::isfinite(rangeStart) || !std::isfinite(rangeEnd) || maximumPoints < 2) {
        return {};
    }
    if (rangeStart > rangeEnd) {
        std::swap(rangeStart, rangeEnd);
    }
    const int count = std::max(2, maximumPoints);
    const double span = rangeEnd - rangeStart;
    QVector<QPointF> result;
    result.reserve(count);
    for (int index = 0; index < count; ++index) {
        const double ratio = static_cast<double>(index) / static_cast<double>(count - 1);
        const double timestamp = rangeStart + span * ratio;
        const auto value = valueAt(channelName, timestamp);
        if (value && std::isfinite(*value)) {
            result.append(QPointF(timestamp, *value));
        }
    }
    return result;
}

double videoToTelemetryTime(const double videoTime, const SyncTransform &transform)
{
    return videoTime * transform.timeScale + transform.offset;
}

} // namespace FlappedEar
