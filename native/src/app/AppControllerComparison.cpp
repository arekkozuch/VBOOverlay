#include "AppController.h"

#include <QJsonArray>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

using namespace FlappedEar;

namespace {
bool eligible(const QVariantMap &row)
{
    return row.value("type") == "LAP" && row.value("referenceEligible").toBool()
        && row.value("compatibilityResolved").toBool() && !row.value("excluded").toBool()
        && !row.value("compatibilityReasons").toStringList().contains("stale-source");
}
}

QVariantList AppController::comparisonSlots() const
{
    QVariantList result;
    for (const auto &slot : m_comparisonSlots)
        result.append(QVariantMap{{"lap", slot.row}, {"state", slot.state}, {"error", slot.error}});
    return result;
}

QVariantList AppController::comparisonLaps() const
{
    QVariantList result;
    if (outingLapsLoading()) return result;
    for (const auto &value : m_outingLapRows) {
        auto row = value.toMap();
        if (!eligible(row)) continue;
        row.insert("label", QStringLiteral("%1 · LAP %2 · %3 s").arg(row.value("runName").toString())
            .arg(row.value("lapNumber").toInt()).arg(row.value("durationSeconds").toDouble(), 0, 'f', 3));
        result.append(row);
    }
    return result;
}

bool AppController::comparisonPairReady() const
{
    return !outingLapsLoading() && m_comparisonSlots[0].state == "ready" && m_comparisonSlots[1].state == "ready"
        && m_comparisonSlots[0].row.value("compatibilityGroupId") == m_comparisonSlots[1].row.value("compatibilityGroupId");
}

bool AppController::selectComparisonLap(const int index, const QVariantMap &reference)
{
    if (index < 0 || index > 1 || recoveryPending()
        || m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None) return false;
    const auto resolved = resolveOutingLapReference(reference);
    if (resolved.value("state") != "resolved") return false;
    const auto row = m_outingLapRows[resolved.value("index").toInt()].toMap();
    if (!eligible(row)) return false;
    const auto &other = m_comparisonSlots[1 - index];
    if ((other.state == "ready" || other.state == "loading")
        && row.value("compatibilityGroupId") != other.row.value("compatibilityGroupId")) return false;
    QJsonObject source;
    for (const auto &value : outingLapSources())
        if (value.toObject().value("runId") == row.value("runId").toString()) source = value.toObject();
    if (source.isEmpty()) return false;
    clearComparisonLap(index);
    auto &slot = m_comparisonSlots[index];
    slot.row = row;
    slot.source = source;
    slot.key = outingRunKey(row.value("runId").toString());
    slot.state = "loading";
    m_comparisonTimer.start();
    emit comparisonSlotsChanged();
    return true;
}

void AppController::clearComparisonLap(const int index)
{
    if (index < 0 || index > 1) return;
    if (m_comparisonPending && m_comparisonLoadingSlot == index && m_comparisonCancellation)
        m_comparisonCancellation->store(true);
    m_comparisonSlots[index] = {};
    m_comparisonSlots[index].request = ++m_comparisonRequest;
    emit comparisonSlotsChanged();
}

void AppController::failComparisonLap(const int index, const QString &reason)
{
    auto &slot = m_comparisonSlots[index];
    if (slot.state == "error" && slot.error == reason) return;
    if (m_comparisonPending && m_comparisonLoadingSlot == index && m_comparisonCancellation)
        m_comparisonCancellation->store(true);
    slot.request = ++m_comparisonRequest;
    slot.session.reset(); slot.geometry = {}; slot.track.clear();
    slot.state = "error"; slot.error = reason;
}

void AppController::invalidateComparisonLaps()
{
    for (int i = 0; i < 2; ++i) {
        auto &slot = m_comparisonSlots[i];
        if (slot.row.isEmpty()) continue;
        if (slot.key != outingRunKey(slot.row.value("runId").toString())) {
            failComparisonLap(i, QStringLiteral("Recording or track configuration changed. Select this lap again."));
            continue;
        }
        // An unrelated run refresh temporarily hides the row list, not this slot.
        if (outingLapsLoading()) continue;
        const auto resolved = resolveOutingLapReference(slot.row.value("reference").toMap());
        if (resolved.value("state") != "resolved") {
            failComparisonLap(i, resolved.value("reason").toString());
            continue;
        }
        const auto row = m_outingLapRows[resolved.value("index").toInt()].toMap();
        if (!eligible(row)) failComparisonLap(i, QStringLiteral("This lap is no longer eligible. Select an eligible lap."));
        slot.row = row;
    }
    if (m_comparisonSlots[0].state == "ready" && m_comparisonSlots[1].state == "ready"
        && m_comparisonSlots[0].row.value("compatibilityGroupId") != m_comparisonSlots[1].row.value("compatibilityGroupId"))
        failComparisonLap(1, QStringLiteral("These laps no longer share a compatible track configuration."));
    emit comparisonSlotsChanged();
}

bool AppController::swapComparisonLaps()
{
    if (m_comparisonSlots[0].row.isEmpty() || m_comparisonSlots[1].row.isEmpty()) return false;
    if (m_comparisonCancellation) m_comparisonCancellation->store(true);
    std::swap(m_comparisonSlots[0], m_comparisonSlots[1]);
    // In-flight results still carry their old slot address and must be rejected.
    for (auto &slot : m_comparisonSlots) slot.request = ++m_comparisonRequest;
    m_comparisonTimer.start();
    emit comparisonSlotsChanged();
    return true;
}

bool AppController::useBestComparisonLap(const bool wholeDay)
{
    if (outingLapsLoading()) return false;
    const auto &a = m_comparisonSlots[0];
    if (!wholeDay && a.row.isEmpty()) return false;
    const auto groupId = a.row.isEmpty() ? outingComparisonGroupId() : a.row.value("compatibilityGroupId").toString();
    for (const auto &value : outingCompatibilityGroups()) {
        const auto group = value.toMap();
        if (group.value("id") != groupId) continue;
        const auto ranking = group.value("ranking").toMap();
        if (wholeDay) return selectComparisonLap(1, ranking.value("bestOfDay").toMap().value("reference").toMap());
        for (const auto &runValue : ranking.value("runs").toList()) {
            const auto run = runValue.toMap();
            if (run.value("runId") == a.row.value("runId"))
                return selectComparisonLap(1, run.value("bestLap").toMap().value("reference").toMap());
        }
    }
    return false;
}

bool AppController::inspectComparisonLap(const int index)
{
    return index >= 0 && index < 2 && m_comparisonSlots[index].state == "ready"
        && selectOutingLapReference(m_comparisonSlots[index].row.value("reference").toMap());
}

void AppController::setComparisonViewOpen(const bool open)
{
    if (m_comparisonViewOpen == open) return;
    m_comparisonViewOpen = open;
    emit comparisonViewOpenChanged();
}

QVariantMap AppController::comparisonLapSeries(
    const int slot, const QString &channel, const int maximumPoints) const
{
    if (slot < 0 || slot > 1 || maximumPoints < 2) return {};
    const auto &comparisonSlot = m_comparisonSlots[slot];
    if (!comparisonSlot.session || comparisonSlot.state != "ready") return {};
    return sessionSeries(*comparisonSlot.session, channel,
        comparisonSlot.row.value("startTime").toDouble(),
        comparisonSlot.row.value("endTime").toDouble(), maximumPoints);
}

QVariantList AppController::comparisonLapTrack(const int slot) const
{
    if (slot < 0 || slot > 1) return {};
    return m_comparisonSlots[slot].track;
}

void AppController::initializeComparisonLaps()
{
    m_comparisonTimer.setSingleShot(true);
    m_comparisonTimer.setInterval(0);
    connect(&m_comparisonTimer, &QTimer::timeout, this, &AppController::loadComparisonLap);
    connect(this, &AppController::documentStateChanged, this, &AppController::invalidateComparisonLaps);
    connect(this, &AppController::sourceLoadStateChanged, this, &AppController::invalidateComparisonLaps);
    connect(this, &AppController::outingLapsChanged, this, &AppController::invalidateComparisonLaps);
    connect(&m_comparisonWatcher, &QFutureWatcher<OutingLapDetailResult>::finished, this, [this] {
        auto result = m_comparisonWatcher.future().takeResult();
        const int index = m_comparisonLoadingSlot;
        m_comparisonPending = false;
        m_comparisonLoadingSlot = -1;
        auto &slot = m_comparisonSlots[index];
        if (slot.request == result.request && slot.state == "loading") {
            if (slot.key != outingRunKey(slot.row.value("runId").toString())) {
                failComparisonLap(index, QStringLiteral("Recording changed while opening this lap. Select it again."));
            } else {
                slot.session = std::move(result.session);
                slot.geometry = std::move(result.geometry);
                slot.track = std::move(result.track);
                slot.error = result.error;
                slot.state = slot.session ? "ready" : "error";
                if (result.staleReference) {
                    m_outingStaleRunIds.insert(slot.row.value("runId").toString());
                    refreshOutingCompatibility();
                    emit outingLapsChanged();
                }
            }
        }
        invalidateComparisonLaps();
        m_comparisonTimer.start();
    });
}

void AppController::loadComparisonLap()
{
    // One bounded parser for the pair. Repeated selections cancel and coalesce;
    // the other slot's verified session is retained throughout.
    if (m_comparisonPending) return;
    for (int i = 0; i < 2; ++i) {
        const auto &slot = m_comparisonSlots[i];
        if (slot.state != "loading") continue;
        const auto source = slot.source;
        const auto row = slot.row;
        const auto request = slot.request;
        const auto projectPath = m_documentState.projectPath();
        m_comparisonCancellation = std::make_shared<std::atomic_bool>(false);
        const auto cancellation = m_comparisonCancellation;
        m_comparisonPending = true;
        m_comparisonLoadingSlot = i;
        m_comparisonWatcher.setFuture(QtConcurrent::run([source, projectPath, row, request, cancellation, cache = m_analysisSourceCache] {
            return readOutingLapDetail(source, projectPath, row, request, cancellation, cache);
        }));
        return;
    }
}
