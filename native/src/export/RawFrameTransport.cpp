#include "export/RawFrameTransport.h"

#include <QElapsedTimer>
#include <QProcess>

#include <algorithm>

namespace FlappedEar {

RawFrameTransport::RawFrameTransport(
    QProcess &process, RawFrameTransportConfig config,
    CancellationCheck cancellationCheck, PumpCallback pumpCallback,
    QueueCallback queueCallback)
    : m_process(process)
    , m_config(config)
    , m_cancellationCheck(std::move(cancellationCheck))
    , m_pumpCallback(std::move(pumpCallback))
    , m_queueCallback(std::move(queueCallback))
{
    Q_ASSERT(m_config.highWaterBytes > 0);
    Q_ASSERT(m_config.lowWaterBytes >= 0);
    Q_ASSERT(m_config.lowWaterBytes < m_config.highWaterBytes);
    Q_ASSERT(m_config.writeChunkBytes > 0);
    Q_ASSERT(m_config.waitIntervalMilliseconds > 0);
    Q_ASSERT(m_config.stallTimeoutMilliseconds > 0);
}

RawFrameTransportResult RawFrameTransport::writeFrame(const QByteArrayView frame)
{
    qsizetype offset = 0;
    QElapsedTimer progressTimer;
    progressTimer.start();
    qint64 previousQueuedBytes = m_process.bytesToWrite();
    observeQueue();

    const auto pumpAndTrackProgress = [&] {
        if (m_pumpCallback) m_pumpCallback();
        const qint64 queuedBytes = m_process.bytesToWrite();
        if (queuedBytes < previousQueuedBytes) progressTimer.restart();
        previousQueuedBytes = queuedBytes;
        observeQueue();
    };
    const auto waitForProgress = [&]() -> RawFrameTransportResult {
        const bool wroteBytes = m_process.waitForBytesWritten(m_config.waitIntervalMilliseconds);
        pumpAndTrackProgress();
        if (m_cancellationCheck && m_cancellationCheck()) {
            return {RawFrameTransportResult::Status::Cancelled, QString(), m_maximumQueuedBytes};
        }
        if (m_process.state() != QProcess::Running) {
            return failure(QStringLiteral("Encoder process exited during raw-frame transport: %1")
                               .arg(m_process.errorString()));
        }
        if (!wroteBytes && m_process.error() == QProcess::WriteError) {
            return failure(QStringLiteral("Raw-frame transport device error: %1")
                               .arg(m_process.errorString()));
        }
        if (progressTimer.elapsed() >= m_config.stallTimeoutMilliseconds) {
            return failure(QStringLiteral("Raw-frame transport stalled for %1 ms with %2 bytes queued.")
                               .arg(m_config.stallTimeoutMilliseconds)
                               .arg(m_process.bytesToWrite()));
        }
        return {RawFrameTransportResult::Status::Success, QString(), m_maximumQueuedBytes};
    };

    while (offset < frame.size()) {
        if (m_cancellationCheck && m_cancellationCheck()) {
            return {RawFrameTransportResult::Status::Cancelled, QString(), m_maximumQueuedBytes};
        }
        if (m_process.state() != QProcess::Running) {
            return failure(QStringLiteral("Encoder process is not running during raw-frame transport: %1")
                               .arg(m_process.errorString()));
        }

        if (m_process.bytesToWrite() >= m_config.highWaterBytes) {
            while (m_process.bytesToWrite() > m_config.lowWaterBytes) {
                const RawFrameTransportResult waitResult = waitForProgress();
                if (!waitResult.succeeded()) return waitResult;
            }
            progressTimer.restart();
        }

        const qsizetype remaining = frame.size() - offset;
        const qsizetype requested = std::min(remaining, m_config.writeChunkBytes);
        const qint64 accepted = m_process.write(frame.data() + offset, requested);
        observeQueue();
        if (accepted < 0) {
            return failure(QStringLiteral("Raw-frame transport write failed: %1")
                               .arg(m_process.errorString()));
        }
        if (accepted == 0) {
            const RawFrameTransportResult waitResult = waitForProgress();
            if (!waitResult.succeeded()) return waitResult;
            continue;
        }
        offset += static_cast<qsizetype>(accepted);
        progressTimer.restart();
        previousQueuedBytes = m_process.bytesToWrite();
    }

    return {RawFrameTransportResult::Status::Success, QString(), m_maximumQueuedBytes};
}

qint64 RawFrameTransport::maximumQueuedBytes() const { return m_maximumQueuedBytes; }

const RawFrameTransportConfig &RawFrameTransport::config() const { return m_config; }

RawFrameTransportResult RawFrameTransport::failure(const QString &message) const
{
    return {RawFrameTransportResult::Status::Failure, message, m_maximumQueuedBytes};
}

void RawFrameTransport::observeQueue()
{
    const qint64 queuedBytes = m_process.bytesToWrite();
    m_maximumQueuedBytes = qMax(m_maximumQueuedBytes, queuedBytes);
    if (m_queueCallback) m_queueCallback(queuedBytes, m_maximumQueuedBytes);
}

} // namespace FlappedEar
