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

std::optional<double> parseClockTime(const QString &value)
{
    bool valid = false;
    const double numeric = value.toDouble(&valid);
    if (!valid || !std::isfinite(numeric)) {
        return std::nullopt;
    }
    if (!value.contains(':') && numeric >= 10000.0 && numeric < 240000.0) {
        const int hours = static_cast<int>(numeric / 10000.0);
        const int minutes = static_cast<int>((numeric - hours * 10000.0) / 100.0);
        return hours * 3600.0 + minutes * 60.0 + std::fmod(numeric, 100.0);
    }
    if (value.contains(':')) {
        const QStringList pieces = value.split(':');
        if (pieces.size() != 3) {
            return std::nullopt;
        }
        bool hoursValid = false;
        bool minutesValid = false;
        bool secondsValid = false;
        const double hours = pieces[0].toDouble(&hoursValid);
        const double minutes = pieces[1].toDouble(&minutesValid);
        const double seconds = pieces[2].toDouble(&secondsValid);
        if (hoursValid && minutesValid && secondsValid) {
            return hours * 3600.0 + minutes * 60.0 + seconds;
        }
        return std::nullopt;
    }
    return numeric;
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
            sections.tryInsert(section, {});
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
    for (qsizetype rowIndex = 0; rowIndex < dataSection.size(); ++rowIndex) {
        const QStringList cells = splitRow(dataSection[rowIndex]);
        if (cells.size() < names.size()) {
            session.warnings.append(QStringLiteral("Row %1: missing %2 value(s).")
                                        .arg(rowIndex + 1)
                                        .arg(names.size() - cells.size()));
        }
        if (cells.size() > names.size()) {
            session.warnings.append(QStringLiteral("Row %1: ignored %2 extra value(s).")
                                        .arg(rowIndex + 1)
                                        .arg(cells.size() - names.size()));
        }
        const auto parsedTime = timeIndex >= 0 && timeIndex < cells.size()
            ? parseClockTime(cells[timeIndex])
            : std::optional<double>(rowIndex);
        if (!parsedTime) {
            session.warnings.append(QStringLiteral("Row %1: invalid timestamp; row skipped.").arg(rowIndex + 1));
            continue;
        }
        if (!origin) {
            origin = *parsedTime;
        }
        double timestamp = *parsedTime - *origin;
        if (timestamp < 0.0) {
            timestamp += 24.0 * 3600.0;
        }
        rawTimes.append(timestamp);
        for (qsizetype column = 0; column < names.size(); ++column) {
            bool valid = false;
            const double parsed = column < cells.size() ? cells[column].toDouble(&valid) : 0.0;
            rawValues[column].append(valid && std::isfinite(parsed)
                                         ? static_cast<float>(normalizeCoordinate(names[column], parsed))
                                         : std::numeric_limits<float>::quiet_NaN());
        }
    }
    if (rawTimes.isEmpty()) {
        throw VboParseError("VBO contains no valid timestamped data rows.");
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
