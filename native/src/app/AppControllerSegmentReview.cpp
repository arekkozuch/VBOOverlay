// KAN-48: review of automatic track-segmentation proposals for the open lap.
// Proposals, rejections and edits live only in this review session; approval
// writes an ordinary segment into the run's "trackSegments", which is the
// approved revision every downstream consumer must use.

#include "app/AppController.h"
#include "project/EventProjectCodec.h"
#include "project/ProjectLimits.h"
#include "telemetry/TrackSegmentReview.h"
#include <QJsonArray>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace FlappedEar;

namespace {

QString uncertaintyText(const QString &reason)
{
    if (reason == proposalUncertainConnectedCorners) return QStringLiteral("Corners connect without a straight");
    if (reason == proposalUncertainShortStraight) return QStringLiteral("Short straight");
    if (reason == proposalUncertainGpsGap) return QStringLiteral("Near a GPS gap in this lap");
    return reason;
}

QStringList uncertaintyTexts(const QStringList &reasons)
{
    QStringList texts;
    for (const auto &reason : reasons) texts.append(uncertaintyText(reason));
    return texts;
}

QString unresolvedProposalText(const QString &reason)
{
    if (reason == "continuousCorner")
        return QStringLiteral("No automatic proposal: this lap turns continuously, with no straight between corners.");
    if (reason == "noCorners") return QStringLiteral("No automatic proposal: no corner was detected on this lap.");
    if (reason == "tooManySegments")
        return QStringLiteral("No automatic proposal: the lap would split into more than %1 segments.").arg(maximumTrackSegments);
    return QStringLiteral("No automatic proposal could be made for this lap.");
}

QString apexText(const CornerPhasePoint &apex)
{
    if (apex.resolved()) return {};
    if (apex.unresolvedReason == cornerPhaseMultipleApexes) return QStringLiteral("Multiple apexes — review manually");
    if (apex.unresolvedReason == cornerPhaseCrossesGate) return QStringLiteral("Corner crosses the timing gate");
    return QStringLiteral("Apex unresolved");
}

std::optional<TrackSegmentType> segmentTypeFromName(const QString &name)
{
    for (const auto type : {TrackSegmentType::Sector, TrackSegmentType::Corner, TrackSegmentType::Straight})
        if (trackSegmentTypeName(type) == name) return type;
    return std::nullopt;
}

// Progress ranges the lap's projected trace does not cover.
QVector<ProgressRange> coverageGaps(const QVector<ProgressSegment> &trace, const double length)
{
    QVector<ProgressRange> covered;
    for (const auto &segment : trace)
        if (!segment.samples.isEmpty())
            covered.append({segment.samples.first().progressMeters, segment.samples.last().progressMeters});
    std::sort(covered.begin(), covered.end(),
        [](const ProgressRange &a, const ProgressRange &b) { return a.startMeters < b.startMeters; });
    QVector<ProgressRange> gaps;
    double reached = 0.0;
    const auto addGap = [&gaps](const double start, const double end) {
        if (end - start >= segmentReviewMinimumGapMeters && gaps.size() < maximumProposalGpsGaps) gaps.append({start, end});
    };
    for (const auto &range : covered) {
        addGap(reached, range.startMeters);
        reached = std::max(reached, range.endMeters);
    }
    addGap(reached, length);
    return gaps;
}

} // namespace

void AppController::initializeSegmentReview()
{
    connect(&m_segmentReviewWatcher, &QFutureWatcher<SegmentReviewResult>::finished, this, [this] {
        auto result = m_segmentReviewWatcher.future().takeResult();
        if (result.request != m_segmentReviewRequest) return; // stale: the lap closed or review restarted
        if (!result.error.isEmpty()) {
            m_segmentReviewState = QStringLiteral("error");
            m_segmentReviewMessage = result.error;
        } else if (!result.unavailable.isEmpty()) {
            m_segmentReviewState = QStringLiteral("unavailable");
            m_segmentReviewMessage = result.unavailable;
        } else {
            m_segmentReviewState = QStringLiteral("ready");
            m_segmentReviewMessage.clear();
            m_segmentReviewAxis = std::move(result.axis);
            m_segmentProposals = std::move(result.proposals.proposals);
            m_segmentProposalPhases = std::move(result.phases);
            m_segmentReviewLapTrace = std::move(result.lapTrace);
        }
        m_segmentReviewLayersDirty = true;
        emit segmentReviewChanged();
    });
    // Approvals, and any other edit of the run's stored segments, change the approved revision.
    connect(this, &AppController::documentStateChanged, this, [this] {
        if (m_segmentReviewState == "idle") return;
        m_segmentReviewLayersDirty = true;
        emit segmentReviewChanged();
    });
}

void AppController::resetSegmentReview()
{
    ++m_segmentReviewRequest;
    if (m_segmentReviewCancellation) m_segmentReviewCancellation->store(true);
    const bool changed = m_segmentReviewState != "idle";
    m_segmentReviewState = QStringLiteral("idle");
    m_segmentReviewMessage.clear();
    m_segmentReviewAxis = {};
    m_segmentProposals.clear();
    m_segmentProposalPhases.clear();
    m_segmentReviewLapTrace.clear();
    m_editedSegmentProposals.clear();
    m_rejectedSegmentProposals.clear();
    m_segmentReviewLayerCache.clear();
    m_segmentReviewLayersDirty = true;
    if (changed) emit segmentReviewChanged();
}

QString AppController::segmentReviewConfiguration() const
{
    return m_selectedOutingLap.value("compatibilityGroupId").toString();
}

QString AppController::segmentReviewUnavailableReason() const
{
    if (m_selectedOutingLap.value("type").toString() != "LAP")
        return QStringLiteral("Segment proposals need a complete timed lap.");
    if (!m_selectedOutingLap.value("referenceIssue").toString().isEmpty()
        || !m_selectedOutingLap.value("layoutIssue").toString().isEmpty())
        return QStringLiteral("This lap has incomplete GPS or does not follow the run's layout. Open a clean lap to review segments.");
    if (segmentReviewConfiguration().isEmpty())
        return QStringLiteral("Confirm the track layout, direction and timing gate before reviewing segments.");
    return {};
}

void AppController::requestSegmentReview()
{
    if (m_outingLapDetailState != "ready" || !m_outingLapDetailSession || m_segmentReviewState == "loading") return;
    resetSegmentReview();
    if (const auto reason = segmentReviewUnavailableReason(); !reason.isEmpty()) {
        m_segmentReviewState = QStringLiteral("unavailable");
        m_segmentReviewMessage = reason;
        emit segmentReviewChanged();
        return;
    }
    m_segmentReviewCancellation = std::make_shared<std::atomic_bool>(false);
    m_segmentReviewState = QStringLiteral("loading");
    emit segmentReviewChanged();
    m_segmentReviewWatcher.setFuture(QtConcurrent::run(
        [session = m_outingLapDetailSession, start = m_selectedOutingLap.value("startTime").toDouble(),
            end = m_selectedOutingLap.value("endTime").toDouble(), lap = m_selectedOutingLap.value("lapNumber").toInt(),
            request = m_segmentReviewRequest, cancellation = m_segmentReviewCancellation] {
            return computeSegmentReview(session, start, end, lap, request, cancellation);
        }));
}

AppController::SegmentReviewResult AppController::computeSegmentReview(std::shared_ptr<const TelemetrySession> session,
    const double startTime, const double endTime, const int lapNumber, const quint64 request,
    const std::shared_ptr<std::atomic_bool> &cancellation)
{
    SegmentReviewResult result;
    result.request = request;
    const auto cancelled = [cancellation] { return cancellation->load(); };
    try {
        if (!session) throw std::runtime_error("The lap's recording is not loaded.");
        const auto laps = deriveSourceLapSession(*session, {}, cancelled);
        const auto trace = std::find_if(laps.lapTraces.cbegin(), laps.lapTraces.cend(),
            [lapNumber](const LapTrace &candidate) { return candidate.lapNumber == lapNumber; });
        if (!laps.selectedStartGate || trace == laps.lapTraces.cend()) {
            result.unavailable = QStringLiteral("This lap has no gate-anchored GPS trace to build a track axis from.");
            return result;
        }
        const auto &gate = *laps.selectedStartGate;
        const GeoCoordinate origin{(gate.endpointA.latitudeDegrees + gate.endpointB.latitudeDegrees) / 2.0,
            (gate.endpointA.longitudeDegrees + gate.endpointB.longitudeDegrees) / 2.0};
        result.axis = buildProgressAxis(*trace, origin, gate, cancelled);
        const auto features = result.axis.valid
            ? computeTrackFeatures(result.axis, segmentReviewSmoothingMeters) : TrackFeatures{};
        if (!features.valid) {
            result.unavailable = QStringLiteral("The track axis could not be built from this lap's GPS trace.");
            return result;
        }
        throwIfCancelled(cancelled);
        result.lapTrace = projectLapTrace(result.axis, *session, startTime, endTime, cancelled);
        result.proposals = proposeTrackSegments(result.axis, features, coverageGaps(result.lapTrace, result.axis.lengthMeters));
        if (!result.proposals.valid) throw std::runtime_error("Segment proposals could not be computed for this lap.");
        if (!result.proposals.unresolvedReason.isEmpty()) {
            result.unavailable = unresolvedProposalText(result.proposals.unresolvedReason);
            return result;
        }
        for (const auto &proposal : result.proposals.proposals) {
            throwIfCancelled(cancelled);
            result.phases.append(proposal.type == TrackSegmentType::Corner
                ? proposeCornerGeometryPhases(result.axis, features, proposal) : CornerGeometryPhases{});
        }
    } catch (const OperationCancelled &) {
        result.error = QStringLiteral("Segment review was cancelled.");
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

double AppController::segmentReviewAxisLength() const
{
    return m_segmentReviewAxis.valid ? m_segmentReviewAxis.lengthMeters : 0.0;
}

ApprovedSegmentation AppController::currentApprovedSegmentation() const
{
    const auto runId = m_selectedOutingLap.value("runId").toString();
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray()) {
        const auto run = value.toObject();
        if (run.value("id").toString() == runId)
            return approvedSegmentation(run.value("trackSegments"), segmentReviewConfiguration());
    }
    return {};
}

QVector<SegmentReviewItem> AppController::currentSegmentReviewItems() const
{
    if (m_segmentReviewState != "ready") return {};
    return reviewSegmentProposals(m_segmentProposals, m_editedSegmentProposals, m_rejectedSegmentProposals,
        currentApprovedSegmentation(), m_segmentReviewAxis.lengthMeters);
}

QVariantList AppController::segmentReviewItems() const
{
    QVariantList rows;
    const auto items = currentSegmentReviewItems();
    for (int index = 0; index < items.size(); ++index) {
        const auto &item = items[index];
        const auto &proposal = item.proposal;
        QVariantMap row{{"index", index}, {"name", proposal.name}, {"type", trackSegmentTypeName(proposal.type)},
            {"state", segmentReviewStateName(item.state)}, {"edited", item.edited},
            {"approvedSegmentId", item.approvedSegmentId},
            {"startMeters", proposal.start.progressMeters}, {"endMeters", proposal.end.progressMeters},
            {"lengthMeters", proposal.lengthMeters},
            {"startToleranceMeters", proposal.start.toleranceMeters}, {"endToleranceMeters", proposal.end.toleranceMeters},
            {"startUncertainty", uncertaintyTexts(proposal.start.uncertaintyReasons)},
            {"endUncertainty", uncertaintyTexts(proposal.end.uncertaintyReasons)},
            {"certain", proposal.start.certain() && proposal.end.certain()},
            {"wrapsGate", proposal.end.progressMeters < proposal.start.progressMeters}};
        if (proposal.type == TrackSegmentType::Corner)
            row.insert("turnDegrees", proposal.turnRadians * 180.0 / 3.14159265358979323846);
        // Geometric apex (KAN-46) only for unedited corners: an edit invalidates it.
        if (!item.edited && index < m_segmentProposalPhases.size() && m_segmentProposalPhases[index].valid) {
            const auto &apex = m_segmentProposalPhases[index].apex;
            if (apex.resolved()) {
                row.insert("apexMeters", apex.progressMeters);
                row.insert("apexToleranceMeters", apex.toleranceMeters);
            } else {
                row.insert("apexNote", apexText(apex));
            }
        }
        rows.append(row);
    }
    return rows;
}

QVariantMap AppController::segmentReviewApproved() const
{
    if (m_segmentReviewState == "idle") return {};
    const auto approved = currentApprovedSegmentation();
    const auto items = currentSegmentReviewItems();
    QVariantList segments;
    for (const auto &value : approved.segments) {
        const auto segment = value.toObject();
        const auto id = segment.value("id").toString();
        const bool fromProposal = std::any_of(items.cbegin(), items.cend(),
            [&id](const SegmentReviewItem &item) { return item.approvedSegmentId == id; });
        segments.append(QVariantMap{{"id", id}, {"name", segment.value("name").toString()},
            {"type", segment.value("type").toString()},
            {"startMeters", segment.value("startProgressMeters").toDouble()},
            {"endMeters", segment.value("endProgressMeters").toDouble()}, {"matchesProposal", fromProposal}});
    }
    const auto hash = approved.revision.section(':', 1);
    return {{"valid", approved.valid}, {"revision", approved.revision}, {"shortRevision", hash.left(12)},
        {"trackConfigurationReference", approved.trackConfigurationReference}, {"count", approved.segments.size()},
        {"otherConfigurationCount", approved.otherConfigurationSegments}, {"segments", segments}};
}

QVariantList AppController::mapPolylines(const double startMeters, const double endMeters) const
{
    const double length = m_segmentReviewAxis.lengthMeters;
    if (!m_outingLapDetailSession || !m_segmentReviewAxis.valid || m_segmentReviewLapTrace.isEmpty()
        || !std::isfinite(startMeters) || !std::isfinite(endMeters)) return {};
    const double span = endMeters >= startMeters ? endMeters - startMeters : endMeters + length - startMeters;
    if (!(span > 0.0) || span > length) return {};
    const double step = std::max({2.0, m_segmentReviewAxis.spacingMeters, span / 2000.0});
    QVariantList polylines;
    QVariantList current;
    const auto flush = [&] {
        if (current.size() >= 2) polylines.append(QVariant::fromValue(current));
        current.clear();
    };
    for (double distance = 0.0;; distance = std::min(span, distance + step)) {
        const double progress = std::fmod(startMeters + distance, length);
        const auto time = timeAtProgress(m_segmentReviewLapTrace, progress);
        const auto point = time ? FlappedEar::currentTrackPoint(*m_outingLapDetailSession, *time, m_outingLapDetailGeometry)
                                : std::nullopt;
        if (point) current.append(QVariantMap{{"x", point->x()}, {"y", point->y()}});
        else flush(); // no coverage: never bridge across it
        if (distance >= span) break;
    }
    flush();
    return polylines;
}

QVariantList AppController::segmentReviewMapLayers() const
{
    if (!m_segmentReviewLayersDirty) return m_segmentReviewLayerCache;
    m_segmentReviewLayersDirty = false;
    m_segmentReviewLayerCache.clear();
    if (m_segmentReviewState != "ready") return m_segmentReviewLayerCache;
    const double length = m_segmentReviewAxis.lengthMeters;
    for (const auto &value : currentApprovedSegmentation().segments) {
        const auto segment = value.toObject();
        m_segmentReviewLayerCache.append(QVariantMap{{"kind", "approved"}, {"id", segment.value("id").toString()},
            {"type", segment.value("type").toString()},
            {"polylines", mapPolylines(segment.value("startProgressMeters").toDouble(),
                segment.value("endProgressMeters").toDouble())}});
    }
    const auto items = currentSegmentReviewItems();
    for (int index = 0; index < items.size(); ++index) {
        const auto &item = items[index];
        if (item.state != SegmentReviewState::Proposed) continue;
        const auto &proposal = item.proposal;
        m_segmentReviewLayerCache.append(QVariantMap{{"kind", "proposal"}, {"index", index},
            {"type", trackSegmentTypeName(proposal.type)},
            {"polylines", mapPolylines(proposal.start.progressMeters, proposal.end.progressMeters)}});
        for (const auto *boundary : {&proposal.start, &proposal.end}) {
            if (boundary->certain() || !(boundary->toleranceMeters > 0.0)) continue;
            const double from = std::fmod(boundary->progressMeters - boundary->toleranceMeters + length, length);
            const double to = std::fmod(boundary->progressMeters + boundary->toleranceMeters, length);
            m_segmentReviewLayerCache.append(QVariantMap{{"kind", "uncertain"}, {"index", index},
                {"polylines", mapPolylines(from, to)}});
        }
    }
    return m_segmentReviewLayerCache;
}

bool AppController::replaceRunTrackSegments(const QString &runId, const QJsonArray &segments)
{
    if (!EventProjectCodec::isEvent(m_projectTemplate) || projectLoading() || exporting()
        || recoveryPending() || m_batchPending
        || m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None
        || m_documentState.revision() == std::numeric_limits<quint64>::max()) return false;
    auto project = currentProjectObject();
    auto event = project.value("event").toObject();
    auto runs = event.value("runs").toArray();
    for (qsizetype i = 0; i < runs.size(); ++i) {
        auto run = runs[i].toObject();
        if (run.value("id").toString() != runId) continue;
        if (run.value("trackSegments").toArray() == segments) return true;
        if (segments.isEmpty()) run.remove("trackSegments");
        else run.insert("trackSegments", segments);
        runs[i] = run;
        event.insert("runs", runs);
        project.insert("event", event);
        QString error;
        if (!ProjectLimits::validateProject(project, &error)) return false;
        m_projectTemplate = project;
        markPersistentChange();
        m_segmentReviewLayersDirty = true;
        emit segmentReviewChanged();
        return true;
    }
    return false;
}

QString AppController::approveSegmentProposal(const int index)
{
    const auto items = currentSegmentReviewItems();
    if (index < 0 || index >= items.size()) return QStringLiteral("This proposal is no longer available.");
    const auto &item = items[index];
    if (item.state == SegmentReviewState::Approved) return {};
    if (item.state == SegmentReviewState::Superseded)
        return QStringLiteral("Overlaps an approved segment. Revoke it or edit this proposal first.");
    const auto &proposal = item.proposal;
    const auto segment = makeTrackSegment(proposal.type, proposal.name, proposal.start.progressMeters,
        proposal.end.progressMeters, segmentReviewConfiguration());
    if (segment.isEmpty()) return QStringLiteral("This proposal cannot be stored as a segment.");
    const auto runId = m_selectedOutingLap.value("runId").toString();
    QJsonValue stored;
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray())
        if (value.toObject().value("id").toString() == runId) stored = value.toObject().value("trackSegments");
    QString error;
    const auto next = withApprovedSegment(stored, segment, m_segmentReviewAxis.lengthMeters, &error);
    if (!next) return error;
    if (!replaceRunTrackSegments(runId, *next)) return QStringLiteral("The project cannot be changed right now.");
    m_rejectedSegmentProposals.remove(index);
    return {};
}

int AppController::approveCertainSegmentProposals()
{
    const auto items = currentSegmentReviewItems();
    const auto runId = m_selectedOutingLap.value("runId").toString();
    QJsonValue stored;
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray())
        if (value.toObject().value("id").toString() == runId) stored = value.toObject().value("trackSegments");
    int approved = 0;
    for (const auto &item : items) {
        if (item.state != SegmentReviewState::Proposed || !item.proposal.start.certain() || !item.proposal.end.certain())
            continue;
        const auto segment = makeTrackSegment(item.proposal.type, item.proposal.name,
            item.proposal.start.progressMeters, item.proposal.end.progressMeters, segmentReviewConfiguration());
        const auto next = segment.isEmpty() ? std::nullopt
            : withApprovedSegment(stored, segment, m_segmentReviewAxis.lengthMeters);
        if (!next) continue;
        stored = *next;
        ++approved;
    }
    if (approved == 0 || !replaceRunTrackSegments(runId, stored.toArray())) return 0;
    return approved;
}

bool AppController::setSegmentProposalRejected(const int index, const bool rejected)
{
    const auto items = currentSegmentReviewItems();
    if (index < 0 || index >= items.size()) return false;
    const auto state = items[index].state;
    if (state != SegmentReviewState::Proposed && state != SegmentReviewState::Rejected) return false;
    if (rejected) m_rejectedSegmentProposals.insert(index);
    else m_rejectedSegmentProposals.remove(index);
    m_segmentReviewLayersDirty = true;
    emit segmentReviewChanged();
    return true;
}

QString AppController::editSegmentProposal(const int index, const QString &name, const QString &type,
    const double startMeters, const double endMeters)
{
    const auto items = currentSegmentReviewItems();
    if (index < 0 || index >= items.size()) return QStringLiteral("This proposal is no longer available.");
    if (items[index].state == SegmentReviewState::Approved)
        return QStringLiteral("Approved segments are not edited here. Revoke the approval to change the proposal.");
    const auto segmentType = segmentTypeFromName(type);
    if (!segmentType) return QStringLiteral("Choose corner, straight or sector.");
    QString error;
    if (!validProposalEdit(name, startMeters, endMeters, m_segmentReviewAxis.lengthMeters, &error)) return error;
    auto &proposal = m_segmentProposals[index];
    const auto place = [](SegmentProposalBoundary &boundary, const double meters) {
        if (boundary.progressMeters == meters) return;
        // A reviewer-placed boundary replaces the automatic estimate and its uncertainty.
        boundary = SegmentProposalBoundary{meters, 0.0, {}};
    };
    proposal.name = name.trimmed();
    proposal.type = *segmentType;
    place(proposal.start, startMeters);
    place(proposal.end, endMeters);
    const double span = endMeters - startMeters;
    proposal.lengthMeters = span > 0.0 ? span : span + m_segmentReviewAxis.lengthMeters;
    m_editedSegmentProposals.insert(index);
    m_segmentReviewLayersDirty = true;
    emit segmentReviewChanged();
    return {};
}

bool AppController::revokeApprovedSegment(const QString &id)
{
    const auto runId = m_selectedOutingLap.value("runId").toString();
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray()) {
        const auto run = value.toObject();
        if (run.value("id").toString() != runId) continue;
        const auto next = withoutApprovedSegment(run.value("trackSegments"), id);
        return next && replaceRunTrackSegments(runId, *next);
    }
    return false;
}

bool AppController::discardOtherConfigurationSegments()
{
    const auto configuration = segmentReviewConfiguration();
    if (configuration.isEmpty()) return false;
    const auto runId = m_selectedOutingLap.value("runId").toString();
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray()) {
        const auto run = value.toObject();
        if (run.value("id").toString() != runId) continue;
        if (!validTrackSegments(run.value("trackSegments"))) return false;
        return replaceRunTrackSegments(runId, withoutOtherConfigurations(run.value("trackSegments"), configuration));
    }
    return false;
}
