#include "telemetry/VboParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <cmath>
#include <limits>

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

double normalizeCoordinate(const QString &name, const double value)
{
    const bool latitude = name.compare("lat", Qt::CaseInsensitive) == 0
        || name.compare("latitude", Qt::CaseInsensitive) == 0;
    const bool longitude = name.compare("lon", Qt::CaseInsensitive) == 0
        || name.compare("long", Qt::CaseInsensitive) == 0
        || name.compare("longitude", Qt::CaseInsensitive) == 0;
    if (latitude && std::abs(value) > 90.0
        && std::abs(value) <= 5400.0) {
        return value / 60.0;
    }
    if (longitude && std::abs(value) > 180.0
        && std::abs(value) <= 10800.0) {
        return value / 60.0;
    }
    return value;
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

TelemetrySession VboParser::parseFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw VboParseError(QStringLiteral("Could not open VBO: %1").arg(file.errorString()));
    }
    return parse(QString::fromUtf8(file.readAll()));
}

TelemetrySession VboParser::parse(QStringView text)
{
    QString content = text.toString();
    if (content.startsWith(QChar::ByteOrderMark)) {
        content.removeFirst();
    }
    QHash<QString, QStringList> sections;
    QString section;
    const QStringList lines = content.split(QRegularExpression("\\r?\\n"));
    for (const QString &raw : lines) {
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
            sections[section].append(line);
        }
    }

    TelemetrySession session;
    for (auto iterator = sections.cbegin(); iterator != sections.cend(); ++iterator) {
        if (iterator.key().contains("column") || iterator.key().contains("data")) {
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
    for (QString &name : names) {
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
    constexpr qsizetype warningLimit = 200;
    qsizetype omittedWarnings = 0;
    const auto appendWarning = [&session, &omittedWarnings](QString warning) {
        if (session.warnings.size() < warningLimit) {
            session.warnings.append(std::move(warning));
        } else {
            ++omittedWarnings;
        }
    };
    for (qsizetype rowIndex = 0; rowIndex < dataSection.size(); ++rowIndex) {
        const QStringList cells = splitRow(dataSection[rowIndex]);
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
                                         ? static_cast<float>(normalizeCoordinate(names[column], parsed))
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
        if (!(rawTimes[index] > rawTimes[index - 1])) {
            throw VboParseError("VBO parser produced non-monotonic timestamps.");
        }
    }

    for (qsizetype column = 0; column < names.size(); ++column) {
        if (column == timeIndex) {
            continue;
        }
        bool containsNumericValue = false;
        for (const float value : rawValues[column]) {
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
    return session;
}

} // namespace FlappedEar
