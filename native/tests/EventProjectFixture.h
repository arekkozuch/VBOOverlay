#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>

namespace EventProjectFixture {

inline QJsonObject source(const QString &id, const QString &path)
{
    return {{"id", id}, {"reference", QJsonObject{{"relativePath", path}, {"futureReference", 42}}},
            {"provenance", QJsonObject{{"note", "synthetic fixture"}}}};
}

inline QJsonObject run(const QString &id, const QString &path, const double offset)
{
    return {{"id", id}, {"name", id}, {"primaryTelemetrySourceId", id + "-source"},
            {"sources", QJsonObject{{"telemetry", QJsonArray{source(id + "-source", path)}}}},
            {"sync", QJsonObject{{"offset", offset}, {"timeScale", 1.0}, {"futureSync", true}}},
            {"notes", "Retain my setup notes"}};
}

inline QJsonObject project()
{
    QJsonObject first = run("run-a", "run-a.vbo", 2.5);
    QJsonObject sources = first.value("sources").toObject();
    QJsonArray telemetry = sources.value("telemetry").toArray();
    telemetry.append(source("run-a-alternative", "run-a.rcz"));
    sources.insert("telemetry", telemetry);
    sources.insert("video", QJsonObject{{"relativePath", "run-a.mp4"}, {"futureVideo", 7}});
    first.insert("sources", sources);
    return {{"version", 3}, {"scene", QJsonObject{{"widgets", QJsonArray{}}}},
            {"event", QJsonObject{{"id", "event-identity"}, {"name", "Development event"},
                {"activeRunId", "run-a"}, {"runs", QJsonArray{first, run("run-b", "run-b.vbo", -1.5)}},
                {"futureEvent", true}}},
            {"documentState", QJsonObject{{"id", "event-document"}, {"savedRevision", "4"}}},
            {"futureRoot", 8}};
}

inline QJsonArray runs(const QJsonObject &project)
{
    return project.value("event").toObject().value("runs").toArray();
}

inline void setRuns(QJsonObject &project, const QJsonArray &runs)
{
    QJsonObject event = project.value("event").toObject();
    event.insert("runs", runs);
    project.insert("event", event);
}

inline QJsonObject reference(const QJsonObject &run, const qsizetype index = 0)
{
    return run.value("sources").toObject().value("telemetry").toArray()[index].toObject().value("reference").toObject();
}

inline QByteArray lapsVbo()
{
    return "[laptiming]\nStart 21.0000 52.0000 21.0000 52.0002 start\n"
           "[column names]\ntime latitude longitude\n[data]\n"
           "0 52.0001 21.0002\n1 52.0001 21.0002\n2 52.0001 20.9998\n"
           "3 52.0008 20.9998\n4 52.0008 21.0002\n5 52.0001 21.0002\n"
           "6 52.0001 20.9998\n7 52.0008 20.9998\n8 52.0008 21.0002\n"
           "10 52.0001 21.0002\n11 52.0001 20.9998\n12 52.0008 20.9998\n"
           "13 52.0008 21.0002\n14 52.0001 21.0002\n15 52.0001 20.9998\n";
}

} // namespace EventProjectFixture
