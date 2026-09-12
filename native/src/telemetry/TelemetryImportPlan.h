#pragma once

#include "telemetry/LapTiming.h"

#include <QByteArray>
#include <QStringList>
#include <memory>

namespace FlappedEar {

// A proposal, not a committed event. Review must resolve grouping and select
// source policy before changing the editor or an authoritative project.
struct TelemetryRunProposal {
    QString id;
    QString sourceId;
    QString sourcePath;
    QString format;
    QByteArray contentSha256;
    std::shared_ptr<const TelemetrySession> telemetry;
    LapSession laps;
};

enum class TelemetryImportFileStatus { Ready, Duplicate, Error };

struct TelemetryImportFileResult {
    QString requestedPath;
    TelemetryImportFileStatus status = TelemetryImportFileStatus::Error;
    // Ready and Duplicate refer to the retained proposal; errors have no ID.
    QString runId;
    QString message;
};

struct TelemetryRunMatchCandidate {
    QString firstRunId;
    QString secondRunId;
    int comparedGpsSamples = 0;
    double maximumSeparationMeters = 0.0;
    double gpsDurationDifferenceSeconds = 0.0;
    // Similar elapsed GPS traces are evidence, not proof of the same recording.
    // In particular this does not establish a common date or absolute clock.
    QString reviewReason;
};

struct TelemetryImportPlan {
    QVector<TelemetryRunProposal> runs;
    QVector<TelemetryImportFileResult> files;
    QVector<TelemetryRunMatchCandidate> possibleSameRuns;
};

struct TelemetryImportLimits {
    // Callers may lower, but never raise, these safety ceilings.
    qsizetype maximumFiles = 64;
    qint64 maximumFileBytes = 128LL * 1024 * 1024;
    qint64 maximumBatchBytes = 256LL * 1024 * 1024;
    qsizetype maximumRetainedChannelSamples = 16'000'000;
};

// Synchronous worker API. Files are parsed sequentially, retaining the existing
// parser limits. The sample budget bounds admitted sessions, not parser scratch
// allocations. Per-file errors preserve other successes; cancellation and an
// invalid/oversized request throw without publishing any partial plan.
[[nodiscard]] TelemetryImportPlan prepareTelemetryImport(
    const QStringList &paths,
    const TelemetryImportLimits &limits = {},
    const CancellationCheck &cancelled = {});

} // namespace FlappedEar
