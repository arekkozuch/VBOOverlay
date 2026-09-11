#include "telemetry/VboParser.h"
#include "telemetry/TelemetryGeometry.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <cmath>
#include <limits>
#include <numbers>

namespace FlappedEar {
namespace {

struct AliasPattern {
    QString alias;
    QList<QRegularExpression> patterns;
};

QRegularExpression expression(const QString &pattern)
{
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
}

const QList<AliasPattern> aliasPatterns = {
    {"speed", {expression("^velocity$"), expression("^gps.?speed"), expression("^speed$"), expression("^obd.?speed")}},
    {"rpm", {expression("^rpm(?:[-_].*)?$"), expression("engine.?speed")}},
    {"throttle", {expression("throttle"), expression("accelerator.?pedal")}},
    {"brake", {expression("^brake(?:[-_].*)?$"), expression("brake.?pressure"), expression("brake.?pedal")}},
    {"heartRate", {expression("heart.?rate"), expression("^hr$"), expression("^bpm$")}},
    {"latitude", {expression("^latitude$"), expression("^lat$")}},
    {"longitude", {expression("^longitude$"), expression("^lon(?:g)?$")}},
    {"lateralAcceleration", {expression("^latacc$"), expression("lat(?:eral)?.?(?:accel|acceleration|g)"), expression("^g.?x$")}},
    {"longitudinalAcceleration", {expression("^longacc$"), expression("long(?:itudinal)?.?(?:accel|acceleration|g)"), expression("^g.?y$")}},
};

const QRegularExpression timeName("^(?:time|timestamp|utc.?time)$", QRegularExpression::CaseInsensitiveOption);

QString normalizeName(QString name)
{
    name = name.trimmed();
    if (name.size() >= 2 && ((name.front() == '\'' && name.back() == '\'')
                             || (name.front() == '"' && name.back() == '"'))) {
        name = name.sliced(1, name.size() - 2);
    }
    return name.replace(QRegularExpression("\\s+"), " ");
}

QStringList splitRow(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.contains(',')) {
        QStringList pieces = trimmed.split(',');
        for (QString &piece : pieces) {
            piece = piece.trimmed();
        }
        return pieces;
    }
    return trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
}

QStringList uniqueNames(const QStringList &input)
{
    QHash<QString, int> counts;
    QStringList output;
    output.reserve(input.size());
    for (const QString &name : input) {
        const int count = counts.value(name) + 1;
        counts.insert(name, count);
        output.append(count == 1 ? name : QStringLiteral("%1 (%2)").arg(name).arg(count));
    }
    return output;
}

enum class TimestampFormat { RelativeSeconds, Clock };

struct ParsedTimestamp {
    double seconds = 0.0;
    TimestampFormat format = TimestampFormat::RelativeSeconds;
};

// Clock syntax is deliberately recognized from the original field text. In
// particular, 003059.500 is 00:30:59.500, not 3,059.5 relative seconds.
std::optional<ParsedTimestamp> parseTimestamp(const QString &value)
{
    const QString text = value.trimmed();
    if (text.contains(':')) {
        static const QRegularExpression colonClock(
            "^(\\d{1,2}):(\\d{2}):(\\d{2}(?:\\.\\d+)?)$");
        const QRegularExpressionMatch match = colonClock.match(text);
        if (!match.hasMatch()) {
            return std::nullopt;
        }
        bool secondsValid = false;
        const int hours = match.captured(1).toInt();
        const int minutes = match.captured(2).toInt();
        const double seconds = match.captured(3).toDouble(&secondsValid);
        if (!secondsValid || !std::isfinite(seconds) || hours < 0 || hours >= 24
            || minutes < 0 || minutes >= 60 || seconds < 0.0 || seconds >= 60.0) {
            return std::nullopt;
        }
        return ParsedTimestamp{hours * 3600.0 + minutes * 60.0 + seconds,
                               TimestampFormat::Clock};
    }

    // A six-digit integer component is always compact HHMMSS syntax. Keep
    // this check textual so leading-zero timestamps before 01:00 are handled.
    static const QRegularExpression compactClockCandidate("^\\d{6}(?:\\..*)?$");
    if (compactClockCandidate.match(text).hasMatch()) {
        static const QRegularExpression compactClock(
            "^(\\d{2})(\\d{2})(\\d{2})(?:\\.(\\d+))?$");
        const QRegularExpressionMatch match = compactClock.match(text);
        if (!match.hasMatch()) {
            return std::nullopt;
        }
        const int hours = match.captured(1).toInt();
        const int minutes = match.captured(2).toInt();
        const int wholeSeconds = match.captured(3).toInt();
        const QString fraction = match.captured(4);
        const double seconds = wholeSeconds + (fraction.isEmpty()
            ? 0.0 : QStringLiteral("0.%1").arg(fraction).toDouble());
        if (!std::isfinite(seconds) || hours >= 24 || minutes >= 60 || seconds >= 60.0) {
            return std::nullopt;
        }
        return ParsedTimestamp{hours * 3600.0 + minutes * 60.0 + seconds,
                               TimestampFormat::Clock};
    }

    bool valid = false;
    const double numeric = text.toDouble(&valid);
    if (!valid || !std::isfinite(numeric)) {
        return std::nullopt;
    }
    return ParsedTimestamp{numeric, TimestampFormat::RelativeSeconds};
}

std::optional<CoordinateAxis> coordinateAxisForName(const QString &name)
{
    const bool latitude = name.compare("lat", Qt::CaseInsensitive) == 0
        || name.compare("latitude", Qt::CaseInsensitive) == 0;
    const bool longitude = name.compare("lon", Qt::CaseInsensitive) == 0
        || name.compare("long", Qt::CaseInsensitive) == 0
        || name.compare("longitude", Qt::CaseInsensitive) == 0;
    if (latitude) return CoordinateAxis::Latitude;
    if (longitude) return CoordinateAxis::Longitude;
    return std::nullopt;
}

double normalizeChannelValue(const QString &name, const double value)
{
    const auto axis = coordinateAxisForName(name);
    if (!axis) return value;
    return normalizeCoordinateDegrees(*axis, value).value_or(
        std::numeric_limits<double>::quiet_NaN());
}

struct TimingGateParseResult {
    std::optional<TimingGate> gate;
    QString error;
};

TimingGateParseResult parseTimingGate(const QString &line, const bool centreDirection)
{
    static const QRegularExpression gateLine(
        QStringLiteral("^(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)(?:\\s+(.*))?$"));
    const QRegularExpressionMatch match = gateLine.match(line);
    if (!match.hasMatch()) {
        return {{}, QStringLiteral("expected a gate name and four coordinates")};
    }
    if (match.captured(1).size() > VboParser::kMaximumFieldCharacters) {
        return {{}, QStringLiteral("gate name exceeds the supported field limit")};
    }
    const QString description = match.captured(6).trimmed();
    if (description.size() > 4'096) {
        return {{}, QStringLiteral("gate description exceeds 4096 characters")};
    }
    double sourceCoordinates[4]{};
    for (int index = 0; index < 4; ++index) {
        const QString token = match.captured(index + 2);
        if (token.size() > VboParser::kMaximumFieldCharacters) {
            return {{}, QStringLiteral("coordinate exceeds the supported field limit")};
        }
        bool valid = false;
        sourceCoordinates[index] = token.toDouble(&valid);
        if (!valid || !std::isfinite(sourceCoordinates[index])) {
            return {{}, QStringLiteral("coordinate %1 is not finite").arg(index + 1)};
        }
    }
    const auto longitudeA = normalizeCoordinateDegrees(
        CoordinateAxis::Longitude, sourceCoordinates[0]);
    const auto latitudeA = normalizeCoordinateDegrees(
        CoordinateAxis::Latitude, sourceCoordinates[1]);
    const auto longitudeB = normalizeCoordinateDegrees(
        CoordinateAxis::Longitude, sourceCoordinates[2]);
    const auto latitudeB = normalizeCoordinateDegrees(
        CoordinateAxis::Latitude, sourceCoordinates[3]);
    if (!longitudeA || !latitudeA || !longitudeB || !latitudeB) {
        return {{}, QStringLiteral("coordinate is outside the supported degree range")};
    }
    GeoCoordinate endpointA{*latitudeA, *longitudeA};
    GeoCoordinate endpointB{*latitudeB, *longitudeB};
    if (endpointA.latitudeDegrees == endpointB.latitudeDegrees
        && endpointA.longitudeDegrees == endpointB.longitudeDegrees) {
        return {{}, QStringLiteral("gate endpoints must be distinct")};
    }
    if (centreDirection) {
        // Verified RaceChrono Pro 10.2.4 export: A is the centre; A->B is
        // backward travel with magnitude equal to the complete gate width.
        // Rotate in metric space, then extend half that width on either side.
        const auto vector = projectCoordinate(endpointB, endpointA);
        const double width = std::hypot(vector.eastMeters, vector.northMeters);
        if (std::abs(endpointA.latitudeDegrees) >= 89.0 || width < 0.01 || width > 1000.0)
            return {{}, QStringLiteral("unsupported RaceChrono gate width or latitude")};
        constexpr double metersPerDegree = 6'371'000.0 * std::numbers::pi / 180.0;
        const double deltaLat = vector.eastMeters * .5 / metersPerDegree;
        const double deltaLon = -vector.northMeters * .5
            / (metersPerDegree * std::cos(endpointA.latitudeDegrees * std::numbers::pi / 180.0));
        const GeoCoordinate centre = endpointA;
        endpointA = {centre.latitudeDegrees - deltaLat, centre.longitudeDegrees - deltaLon};
        endpointB = {centre.latitudeDegrees + deltaLat, centre.longitudeDegrees + deltaLon};
        if (!isValidCoordinate(endpointA) || !isValidCoordinate(endpointB))
            return {{}, QStringLiteral("invalid converted RaceChrono gate endpoints")};
    }
    const QString sourceName = match.captured(1);
    TimingGateType type = TimingGateType::Unknown;
    if (sourceName.compare(QStringLiteral("Start"), Qt::CaseInsensitive) == 0) {
        type = TimingGateType::Start;
    } else if (sourceName.compare(QStringLiteral("Split"), Qt::CaseInsensitive) == 0) {
        type = TimingGateType::Split;
    }
    return {TimingGate{type, sourceName, endpointA, endpointB, description}, {}};
}

QHash<QString, QString> resolveAliases(const QStringList &names)
{
    QHash<QString, QString> aliases;
    for (const AliasPattern &aliasPattern : aliasPatterns) {
        for (const QString &name : names) {
            bool matched = false;
            for (const QRegularExpression &pattern : aliasPattern.patterns) {
                if (pattern.match(name).hasMatch()) {
                    matched = true;
                    break;
                }
            }
            if (matched) {
                aliases.insert(aliasPattern.alias, name);
                break;
            }
        }
    }
    return aliases;
}

} // namespace

VboParseError::VboParseError(const QString &message)
    : std::runtime_error(message.toStdString())
{
}

TelemetrySession VboParser::parseFile(const QString &path, const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw VboParseError(QStringLiteral("Could not open VBO: %1").arg(file.errorString()));
    }
    if (file.size() > kMaximumFileBytes) {
        throw ResourceLimitError("VBO exceeds the supported 128 MiB file size limit.");
    }
    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(file.size()));
    constexpr qint64 chunkSize = 1024 * 1024;
    while (!file.atEnd()) {
        throwIfCancelled(cancelled);
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            throw VboParseError(QStringLiteral("Could not read VBO: %1").arg(file.errorString()));
        }
        bytes.append(chunk);
        if (bytes.size() > kMaximumFileBytes) {
            throw ResourceLimitError("VBO exceeds the supported 128 MiB file size limit.");
        }
    }
    throwIfCancelled(cancelled);
    return parse(QString::fromUtf8(bytes), cancelled);
}

TelemetrySession VboParser::parse(QStringView text, const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
    if (text.size() > kMaximumFileBytes) {
        throw ResourceLimitError("VBO text exceeds the supported complexity limit.");
    }
    QString content = text.toString();
    if (content.startsWith(QChar::ByteOrderMark)) {
        content.removeFirst();
    }
    QHash<QString, QStringList> sections;
    QString section;
    const QStringList lines = content.split(QRegularExpression("\\r?\\n"));
    if (lines.size() > kMaximumLines) {
        throw ResourceLimitError("VBO contains too many lines.");
    }
    qsizetype dataRows = 0;
    for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        if ((lineIndex & 0xff) == 0) throwIfCancelled(cancelled);
        const QString &raw = lines[lineIndex];
        if (raw.size() > kMaximumLineCharacters) {
            throw ResourceLimitError("VBO contains a line longer than the supported 1 MiB limit.");
        }
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
            continue;
        }
        const auto match = QRegularExpression("^\\[([^\\]]+)\\]$").match(line);
        if (match.hasMatch()) {
            section = match.captured(1).trimmed().toLower();
            if (!sections.contains(section)) {
                sections.insert(section, QStringList{});
            }
        } else {
            if (section.startsWith("data") && ++dataRows > kMaximumDataRows) {
                throw ResourceLimitError("VBO contains too many data rows.");
            }
            sections[section].append(line);
        }
    }

    TelemetrySession session;
    constexpr qsizetype warningLimit = 200;
    qsizetype omittedWarnings = 0;
    const auto appendWarning = [&session, &omittedWarnings](QString warning) {
        if (session.warnings.size() < warningLimit) {
            session.warnings.append(std::move(warning));
        } else {
            ++omittedWarnings;
        }
    };
    constexpr qsizetype maximumTimingGates = 128;
    const QStringList timingLines = sections.value(QStringLiteral("laptiming"));
    bool raceChrono = false;
    bool centreDirection = false;
    for (const auto &comment : sections.value(QStringLiteral("comments"))) {
        if (comment.startsWith("Generated by RaceChrono", Qt::CaseInsensitive)) {
            raceChrono = true;
            centreDirection = comment.compare("Generated by RaceChrono Pro v10.2.4", Qt::CaseInsensitive) == 0;
            break;
        }
    }
    if (raceChrono && !centreDirection && !timingLines.isEmpty())
        appendWarning("Timing gates ignored: this RaceChrono exporter version has not been validated. Telemetry remains available.");
    if (centreDirection)
        session.metadata.insert("timingGateFormat", "racechrono-pro-10.2.4-centre-direction");
    bool timingLimitWarningAdded = false;
    for (qsizetype index = 0; index < timingLines.size(); ++index) {
        if ((index & 0x3f) == 0) throwIfCancelled(cancelled);
        if (raceChrono && !centreDirection) break;
        const TimingGateParseResult parsed = parseTimingGate(timingLines[index], centreDirection);
        if (!parsed.gate) {
            appendWarning(QStringLiteral("Timing line %1 ignored: %2.")
                              .arg(index + 1)
                              .arg(parsed.error));
            continue;
        }
        if (session.timingGates.size() >= maximumTimingGates) {
            if (!timingLimitWarningAdded) {
                appendWarning(QStringLiteral(
                    "Additional timing gates ignored after the supported limit of 128."));
                timingLimitWarningAdded = true;
            }
            continue;
        }
        session.timingGates.append(*parsed.gate);
    }
    for (auto iterator = sections.cbegin(); iterator != sections.cend(); ++iterator) {
        if (iterator.key().contains("column") || iterator.key().contains("data")
            || iterator.key() == QStringLiteral("laptiming")) {
            continue;
        }
        for (const QString &entry : iterator.value()) {
            const qsizetype separator = entry.indexOf(QRegularExpression("[:=]"));
            if (separator >= 0) {
                session.metadata.insert(normalizeName(entry.first(separator)), entry.sliced(separator + 1).trimmed());
            } else {
                session.metadata.insert(QStringLiteral("%1.%2").arg(iterator.key()).arg(session.metadata.size()), entry);
            }
        }
    }

    QStringList columnSection;
    QStringList dataSection;
    for (auto iterator = sections.cbegin(); iterator != sections.cend(); ++iterator) {
        if (columnSection.isEmpty() && iterator.key().contains("column")) {
            columnSection = iterator.value();
        }
        if (dataSection.isEmpty() && iterator.key().startsWith("data")) {
            dataSection = iterator.value();
        }
    }
    if (columnSection.isEmpty()) {
        throw VboParseError("VBO has no [column names] section.");
    }
    if (dataSection.isEmpty()) {
        throw VboParseError("VBO has no [data] rows.");
    }

    QStringList names = splitRow(columnSection.join(' '));
    if (names.size() > kMaximumColumns) {
        throw ResourceLimitError("VBO contains too many columns.");
    }
    if (names.isEmpty()) {
        throw VboParseError("VBO contains no column names.");
    }
    for (QString &name : names) {
        if (name.size() > kMaximumFieldCharacters) {
            throw ResourceLimitError("VBO contains a column name longer than the supported field limit.");
        }
        name = normalizeName(name);
    }
    names = uniqueNames(names);
    qsizetype timeIndex = -1;
    for (qsizetype index = 0; index < names.size(); ++index) {
        if (timeName.match(names[index]).hasMatch()) {
            timeIndex = index;
            break;
        }
    }

    QVector<QVector<float>> rawValues(names.size());
    QVector<double> rawTimes;
    std::optional<double> origin;
    std::optional<double> previousAbsoluteTime;
    std::optional<double> previousClockTime;
    double clockDayOffset = 0.0;
    constexpr double lateDayThreshold = 23.0 * 3600.0;
    constexpr double earlyDayThreshold = 1.0 * 3600.0;
    for (qsizetype rowIndex = 0; rowIndex < dataSection.size(); ++rowIndex) {
        if ((rowIndex & 0xff) == 0) throwIfCancelled(cancelled);
        const QStringList cells = splitRow(dataSection[rowIndex]);
        for (const QString &cell : cells) {
            if (cell.size() > kMaximumFieldCharacters) {
                throw ResourceLimitError("VBO contains a field longer than the supported 64 KiB limit.");
            }
        }
        if (cells.size() < names.size()) {
            appendWarning(QStringLiteral("Row %1: missing %2 value(s).")
                              .arg(rowIndex + 1)
                              .arg(names.size() - cells.size()));
        }
        if (cells.size() > names.size()) {
            appendWarning(QStringLiteral("Row %1: ignored %2 extra value(s).")
                              .arg(rowIndex + 1)
                              .arg(cells.size() - names.size()));
        }
        const auto parsedTime = timeIndex >= 0
            ? (timeIndex < cells.size() ? parseTimestamp(cells[timeIndex])
                                        : std::optional<ParsedTimestamp>{})
            : std::optional<ParsedTimestamp>(
                  ParsedTimestamp{static_cast<double>(rowIndex), TimestampFormat::RelativeSeconds});
        if (!parsedTime) {
            const QString timestampText = timeIndex >= 0 && timeIndex < cells.size()
                ? cells[timeIndex] : QString();
            appendWarning(QStringLiteral("Row %1: invalid timestamp \"%2\"; row skipped.")
                              .arg(rowIndex + 1)
                              .arg(timestampText));
            continue;
        }
        double absoluteTime = parsedTime->seconds;
        if (parsedTime->format == TimestampFormat::Clock) {
            if (previousClockTime && previousAbsoluteTime
                && parsedTime->seconds < *previousClockTime
                && *previousClockTime >= lateDayThreshold
                && parsedTime->seconds <= earlyDayThreshold) {
                clockDayOffset += 24.0 * 3600.0;
                appendWarning(QStringLiteral("Row %1: midnight rollover detected.").arg(rowIndex + 1));
            }
            absoluteTime += clockDayOffset;
        }
        if (!origin) {
            origin = absoluteTime;
        }
        if (previousAbsoluteTime) {
            if (absoluteTime == *previousAbsoluteTime) {
                // Keep the first row and skip later duplicates so every
                // emitted channel remains aligned on strictly increasing time.
                appendWarning(QStringLiteral("Row %1: duplicate timestamp %2; later row skipped.")
                                  .arg(rowIndex + 1)
                                  .arg(absoluteTime - *origin, 0, 'f', 3));
                continue;
            }
            if (absoluteTime < *previousAbsoluteTime) {
                appendWarning(QStringLiteral("Row %1: timestamp moved backward from %2 to %3; row skipped.")
                                  .arg(rowIndex + 1)
                                  .arg(*previousAbsoluteTime - *origin, 0, 'f', 3)
                                  .arg(absoluteTime - *origin, 0, 'f', 3));
                continue;
            }
        }
        const double timestamp = absoluteTime - *origin;
        rawTimes.append(timestamp);
        for (qsizetype column = 0; column < names.size(); ++column) {
            bool valid = false;
            const double parsed = column < cells.size() ? cells[column].toDouble(&valid) : 0.0;
            rawValues[column].append(valid && std::isfinite(parsed)
                                         ? static_cast<float>(normalizeChannelValue(names[column], parsed))
                                         : std::numeric_limits<float>::quiet_NaN());
        }
        previousAbsoluteTime = absoluteTime;
        previousClockTime = parsedTime->format == TimestampFormat::Clock
            ? std::optional<double>(parsedTime->seconds) : std::nullopt;
    }
    if (omittedWarnings > 0) {
        session.warnings.append(
            QStringLiteral("… %1 additional parser warnings omitted.").arg(omittedWarnings));
    }
    if (rawTimes.isEmpty()) {
        throw VboParseError("VBO contains no valid timestamped data rows.");
    }
    for (qsizetype index = 1; index < rawTimes.size(); ++index) {
        if ((index & 0xfff) == 0) throwIfCancelled(cancelled);
        if (!(rawTimes[index] > rawTimes[index - 1])) {
            throw VboParseError("VBO parser produced non-monotonic timestamps.");
        }
    }

    for (qsizetype column = 0; column < names.size(); ++column) {
        if ((column & 0x1f) == 0) throwIfCancelled(cancelled);
        if (column == timeIndex) {
            continue;
        }
        bool containsNumericValue = false;
        qsizetype valueIndex = 0;
        for (const float value : rawValues[column]) {
            if ((valueIndex++ & 0xfff) == 0) throwIfCancelled(cancelled);
            if (std::isfinite(value)) {
                containsNumericValue = true;
                break;
            }
        }
        if (!containsNumericValue) {
            continue;
        }
        TelemetryChannel channel;
        channel.name = names[column];
        channel.timestamps = rawTimes;
        channel.values = std::move(rawValues[column]);
        session.channels.insert(channel.name, std::move(channel));
    }
    session.duration = rawTimes.back() - rawTimes.front();
    session.startTime = origin.value_or(0.0);
    session.sampleCount = rawTimes.size();
    session.aliases = resolveAliases(session.channelNames());
    throwIfCancelled(cancelled);
    return session;
}

} // namespace FlappedEar
