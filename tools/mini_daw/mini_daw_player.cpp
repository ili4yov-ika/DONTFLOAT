#include "mini_daw_player.h"

#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>

#include <algorithm>
#include <cstring>

namespace MiniDaw {

namespace {
constexpr int kChannels = 2;
constexpr int kBytesPerFrame = kChannels * int(sizeof(float));
} // namespace

Player::Player(QObject* parent)
    : QObject(parent)
{
}

Player::~Player()
{
    destroySink();
}

qint64 Player::StreamDevice::bytesAvailable() const
{
    const qint64 left = owner_->totalFrames_ - owner_->position_.load();
    return std::max<qint64>(0, left) * kBytesPerFrame + QIODevice::bytesAvailable();
}

qint64 Player::StreamDevice::readData(char* data, qint64 maxSize)
{
    if (!owner_ || maxSize <= 0) {
        return 0;
    }
    const qint64 pos = owner_->position_.load();
    const qint64 framesLeft = owner_->totalFrames_ - pos;
    if (framesLeft <= 0) {
        return 0;
    }
    const qint64 frames = std::min<qint64>(framesLeft, maxSize / kBytesPerFrame);
    if (frames <= 0) {
        return 0;
    }
    const qint64 bytes = frames * kBytesPerFrame;
    std::memcpy(data, owner_->interleaved_.constData() + pos * kBytesPerFrame, std::size_t(bytes));
    owner_->position_.store(pos + frames);
    return bytes;
}

void Player::setAudio(const QVector<float>& left, const QVector<float>& right, int sampleRate)
{
    stop();
    totalFrames_ = std::min<qint64>(left.size(), right.size());
    sampleRate_ = sampleRate > 0 ? sampleRate : 44100;
    interleaved_.resize(int(totalFrames_ * kBytesPerFrame));

    auto* out = reinterpret_cast<float*>(interleaved_.data());
    for (qint64 i = 0; i < totalFrames_; ++i) {
        out[i * 2] = left[int(i)];
        out[i * 2 + 1] = right[int(i)];
    }
    position_.store(0);
}

void Player::clear()
{
    stop();
    interleaved_.clear();
    totalFrames_ = 0;
    position_.store(0);
}

void Player::play()
{
    if (totalFrames_ <= 0 || isPlaying()) {
        return;
    }
    if (position_.load() >= totalFrames_) {
        position_.store(0);
    }

    QAudioFormat format;
    format.setSampleRate(sampleRate_);
    format.setChannelCount(kChannels);
    format.setSampleFormat(QAudioFormat::Float);

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull() || !output.isFormatSupported(format)) {
        // Без устройства (или без float32) транспорт молча не стартует —
        // окно продолжает работать, плагин остаётся живым
        return;
    }

    destroySink();
    sink_ = new QAudioSink(output, format, this);
    device_ = new StreamDevice(this);
    device_->open(QIODevice::ReadOnly);
    connect(sink_, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState || state == QAudio::StoppedState) {
            if (position_.load() >= totalFrames_) {
                emit finished();
            }
        }
    });
    sink_->start(device_);
}

void Player::stop()
{
    destroySink();
}

void Player::seek(qint64 frame)
{
    const qint64 clamped = std::clamp<qint64>(frame, 0, std::max<qint64>(0, totalFrames_));
    const bool wasPlaying = isPlaying();
    if (wasPlaying) {
        destroySink();
    }
    position_.store(clamped);
    if (wasPlaying) {
        play();
    }
}

bool Player::isPlaying() const
{
    return sink_ != nullptr && sink_->state() == QAudio::ActiveState;
}

qint64 Player::positionFrames() const
{
    return position_.load();
}

void Player::destroySink()
{
    if (sink_) {
        sink_->stop();
        sink_->deleteLater();
        sink_ = nullptr;
    }
    if (device_) {
        device_->close();
        device_->deleteLater();
        device_ = nullptr;
    }
}

} // namespace MiniDaw
