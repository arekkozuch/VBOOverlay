#include "export/BoundedProcessOutput.h"

namespace FlappedEar {

BoundedProcessOutput::BoundedProcessOutput(const Mode mode, const qint64 maximumBytes)
    : m_mode(mode), m_maximumBytes(maximumBytes) {}

void BoundedProcessOutput::append(const QByteArray &chunk)
{
    m_observedBytes += chunk.size();
    if (m_mode == Mode::ByteCountOnly) return;
    if (m_mode == Mode::CompletePayload) {
        if (m_bytes.size() + chunk.size() > m_maximumBytes) { m_exceeded = true; return; }
        m_bytes += chunk;
        return;
    }
    m_bytes += chunk;
    if (m_bytes.size() > m_maximumBytes) {
        m_bytes.remove(0, m_bytes.size() - m_maximumBytes);
        m_truncated = true;
    }
}

qint64 BoundedProcessOutput::observedBytes() const { return m_observedBytes; }
bool BoundedProcessOutput::exceeded() const { return m_exceeded; }
bool BoundedProcessOutput::truncated() const { return m_truncated; }
QByteArray BoundedProcessOutput::bytes() const { return m_bytes; }
QString BoundedProcessOutput::text() const { return QString::fromUtf8(m_bytes).trimmed(); }

} // namespace FlappedEar
