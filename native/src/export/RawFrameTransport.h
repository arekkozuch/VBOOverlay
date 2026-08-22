#pragma once

#include <QByteArrayView>
#include <QString>

#include <functional>

class QProcess;

namespace FlappedEar {

struct RawFrameTransportConfig {
    qint64 highWaterBytes = 8LL * 1024LL * 1024LL;
    qint64 lowWaterBytes = 4LL * 1024LL * 1024LL;
    qsizetype writeChunkBytes = 1LL * 1024LL * 1024LL;
    int waitIntervalMilliseconds = 100;
    qint64 stallTimeoutMilliseconds = 300'000;
};

struct RawFrameTransportResult {
    enum class Status { Success, Cancelled, Failure };
    Status status = Status::Failure;
    QString error;
    qint64 maximumQueuedBytes = 0;

    [[nodiscard]] bool succeeded() const { return status == Status::Success; }
};

class RawFrameTransport final {
public:
    using CancellationCheck = std::function<bool()>;
    using PumpCallback = std::function<void()>;
    using QueueCallback = std::function<void(qint64 queuedBytes, qint64 maximumQueuedBytes)>;

    explicit RawFrameTransport(
        QProcess &process, RawFrameTransportConfig config = {},
        CancellationCheck cancellationCheck = {}, PumpCallback pumpCallback = {},
        QueueCallback queueCallback = {});

    [[nodiscard]] RawFrameTransportResult writeFrame(QByteArrayView frame);
    [[nodiscard]] qint64 maximumQueuedBytes() const;
    [[nodiscard]] const RawFrameTransportConfig &config() const;

private:
    [[nodiscard]] RawFrameTransportResult failure(const QString &message) const;
    void observeQueue();

    QProcess &m_process;
    RawFrameTransportConfig m_config;
    CancellationCheck m_cancellationCheck;
    PumpCallback m_pumpCallback;
    QueueCallback m_queueCallback;
    qint64 m_maximumQueuedBytes = 0;
};

} // namespace FlappedEar
