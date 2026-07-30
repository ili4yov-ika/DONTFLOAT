#include "../include/notepreviewplayer.h"

#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaDevices>
#include <QtCore/QMutexLocker>
#include <cmath>

namespace {

constexpr int kFadeSamples = 128; // короткий фейд на краях цикла против щелчков

} // namespace

// ---------------------------------------------------------------------------
// LoopDevice

void NotePreviewPlayer::LoopDevice::setData(const QByteArray& bytes)
{
    QMutexLocker locker(&m_mutex);
    m_data = bytes;
    if (!m_data.isEmpty()) {
        m_pos %= m_data.size();
        m_pos -= m_pos % int(sizeof(float)); // не рвать float по границе
    } else {
        m_pos = 0;
    }
}

qint64 NotePreviewPlayer::LoopDevice::bytesAvailable() const
{
    QMutexLocker locker(&m_mutex);
    // Цикл бесконечный: сообщаем sink'у, что данные всегда есть
    return m_data.isEmpty() ? 0 : m_data.size() + QIODevice::bytesAvailable();
}

qint64 NotePreviewPlayer::LoopDevice::readData(char* out, qint64 maxLen)
{
    QMutexLocker locker(&m_mutex);
    if (m_data.isEmpty() || maxLen <= 0) {
        return 0;
    }

    qint64 written = 0;
    while (written < maxLen) {
        const qint64 chunk = qMin(maxLen - written, qint64(m_data.size()) - m_pos);
        memcpy(out + written, m_data.constData() + m_pos, size_t(chunk));
        written += chunk;
        m_pos += chunk;
        if (m_pos >= m_data.size()) {
            m_pos = 0;
        }
    }
    return written;
}

// ---------------------------------------------------------------------------
// NotePreviewPlayer

NotePreviewPlayer::NotePreviewPlayer(QObject* parent)
    : QObject(parent)
{
}

NotePreviewPlayer::~NotePreviewPlayer()
{
    stop();
}

QByteArray NotePreviewPlayer::renderLoopBuffer() const
{
    if (m_segment.isEmpty()) {
        return {};
    }

    // Varispeed-ресемплинг: выше нота — быстрее чтение (короче цикл).
    const float ratio = std::pow(2.0f, m_semitones / 12.0f);
    const int outLength = qMax(kFadeSamples * 2, int(m_segment.size() / ratio));

    QByteArray bytes(outLength * int(sizeof(float)), Qt::Uninitialized);
    float* out = reinterpret_cast<float*>(bytes.data());

    const double step = double(m_segment.size() - 1) / qMax(1, outLength - 1);
    for (int i = 0; i < outLength; ++i) {
        const double pos = i * step;
        const int idx = qMin(int(pos), m_segment.size() - 2);
        const float t = float(pos - idx);
        float sample = (idx + 1 < m_segment.size())
            ? m_segment[idx] * (1.0f - t) + m_segment[idx + 1] * t
            : m_segment[idx];

        // Фейды на краях цикла, чтобы стык не щёлкал
        if (i < kFadeSamples) {
            sample *= float(i) / float(kFadeSamples);
        } else if (i >= outLength - kFadeSamples) {
            sample *= float(outLength - 1 - i) / float(kFadeSamples);
        }
        out[i] = sample;
    }
    return bytes;
}

void NotePreviewPlayer::start(const QVector<float>& monoSegment, int sampleRate, float semitoneOffset)
{
    stop();

    if (monoSegment.size() < kFadeSamples * 2 || sampleRate <= 0) {
        return;
    }

    m_segment = monoSegment;
    m_sampleRate = sampleRate;
    m_semitones = semitoneOffset;

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(format)) {
        return;
    }

    m_device = new LoopDevice(this);
    m_device->setData(renderLoopBuffer());
    m_device->open(QIODevice::ReadOnly);

    m_sink = new QAudioSink(device, format, this);
    m_sink->start(m_device);
}

void NotePreviewPlayer::setSemitoneOffset(float semitones)
{
    if (std::abs(semitones - m_semitones) < 1.0e-4f) {
        return;
    }
    m_semitones = semitones;
    if (m_device) {
        m_device->setData(renderLoopBuffer());
    }
}

void NotePreviewPlayer::stop()
{
    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    if (m_device) {
        m_device->close();
        m_device->deleteLater();
        m_device = nullptr;
    }
    m_segment.clear();
}
