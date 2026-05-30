#include "../include/audiofileservice.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QtGlobal>
#include <QtMultimedia/QAudioBuffer>
#include <QtMultimedia/QAudioDecoder>
#include <QtMultimedia/QAudioFormat>

namespace {

// Раскладывает кадры QAudioBuffer по каналам (моно/стерео), конвертируя любой
// формат сэмплов в float [-1; 1]. Берутся первые два канала.
void appendAudioBuffer(const QAudioBuffer& buffer, QVector<float>& left, QVector<float>& right)
{
    const int frames = buffer.frameCount();
    const int ch = buffer.format().channelCount();
    if (frames <= 0 || ch <= 0)
        return;

    const bool stereo = ch > 1;
    auto push = [&](float l, float r) {
        left.append(l);
        if (stereo)
            right.append(r);
    };

    switch (buffer.format().sampleFormat()) {
    case QAudioFormat::Float: {
        const float* d = buffer.constData<float>();
        for (int i = 0; i < frames; ++i)
            push(d[i * ch], stereo ? d[i * ch + 1] : 0.f);
        break;
    }
    case QAudioFormat::Int16: {
        const qint16* d = buffer.constData<qint16>();
        constexpr float k = 1.0f / 32768.0f;
        for (int i = 0; i < frames; ++i)
            push(d[i * ch] * k, stereo ? d[i * ch + 1] * k : 0.f);
        break;
    }
    case QAudioFormat::Int32: {
        const qint32* d = buffer.constData<qint32>();
        constexpr float k = 1.0f / 2147483648.0f;
        for (int i = 0; i < frames; ++i)
            push(float(d[i * ch]) * k, stereo ? float(d[i * ch + 1]) * k : 0.f);
        break;
    }
    case QAudioFormat::UInt8: {
        const quint8* d = buffer.constData<quint8>();
        constexpr float k = 1.0f / 128.0f;
        for (int i = 0; i < frames; ++i)
            push((int(d[i * ch]) - 128) * k, stereo ? (int(d[i * ch + 1]) - 128) * k : 0.f);
        break;
    }
    default:
        // Неизвестный формат сэмпла — пропускаем буфер.
        break;
    }
}

QString decoderErrorText(QAudioDecoder::Error error)
{
    switch (error) {
    case QAudioDecoder::ResourceError:
        return QCoreApplication::translate("AudioFileService", "файл не найден или недоступен");
    case QAudioDecoder::FormatError:
        return QCoreApplication::translate("AudioFileService", "неподдерживаемый формат");
    case QAudioDecoder::AccessDeniedError:
        return QCoreApplication::translate("AudioFileService", "нет доступа к файлу");
    default:
        return QCoreApplication::translate("AudioFileService", "неизвестная ошибка");
    }
}

} // namespace

namespace AudioFileService {

DecodeResult decode(const QString& filePath, const std::function<void(int)>& onProgress)
{
    DecodeResult result;

    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(filePath));

    QEventLoop loop;
    QVector<float> leftChannel;
    QVector<float> rightChannel;
    bool decodeError = false;
    QString errorMsg;
    int detectedSampleRate = 0;
    bool reserved = false;
    int lastReportedPercent = -1;

    // Сторожевой таймер: прерываем ожидание, если декодер «завис» (нет данных > 15 c).
    QTimer stallTimer;
    stallTimer.setSingleShot(true);
    stallTimer.setInterval(15000);
    QObject::connect(&stallTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    stallTimer.start();

    QObject::connect(&decoder,
        static_cast<void(QAudioDecoder::*)(QAudioDecoder::Error)>(&QAudioDecoder::error),
        [&](QAudioDecoder::Error error) {
            decodeError = true;
            errorMsg = decoderErrorText(error);
            loop.quit();
        });

    QObject::connect(&decoder, &QAudioDecoder::bufferReady, [&]() {
        const QAudioBuffer buffer = decoder.read();
        if (!buffer.isValid() || buffer.frameCount() == 0)
            return;

        stallTimer.start(); // данные пришли — перезапускаем сторож

        const int rate = buffer.format().sampleRate();
        if (rate > 0 && detectedSampleRate == 0)
            detectedSampleRate = rate; // нативная частота, без ресемплинга

        // Однократно резервируем память по оценке длительности файла.
        if (!reserved && detectedSampleRate > 0) {
            const qint64 durMs = decoder.duration();
            if (durMs > 0) {
                const qint64 estFrames = (durMs * detectedSampleRate) / 1000 + buffer.frameCount();
                leftChannel.reserve(estFrames);
                if (buffer.format().channelCount() > 1)
                    rightChannel.reserve(estFrames);
                reserved = true;
            }
        }

        appendAudioBuffer(buffer, leftChannel, rightChannel);

        if (onProgress && detectedSampleRate > 0) {
            const qint64 durMs = decoder.duration();
            if (durMs > 0) {
                const qint64 doneMs = (qint64(leftChannel.size()) * 1000) / detectedSampleRate;
                const int percent = int(qBound(qint64(0), doneMs * 100 / durMs, qint64(99)));
                if (percent != lastReportedPercent) {
                    lastReportedPercent = percent;
                    onProgress(percent);
                }
            }
        }
    });

    QObject::connect(&decoder, &QAudioDecoder::finished, &loop, &QEventLoop::quit);

    decoder.start();
    loop.exec();
    decoder.stop();

    if (decodeError) {
        result.error = errorMsg;
        return result;
    }

    if (leftChannel.isEmpty()) {
        result.error = QCoreApplication::translate("AudioFileService",
                                                   "не удалось декодировать аудиоданные");
        return result;
    }

    result.channels.append(leftChannel);
    if (!rightChannel.isEmpty())
        result.channels.append(rightChannel);
    result.sampleRate = detectedSampleRate;
    result.ok = true;
    return result;
}

QVector<float> toMono(const QVector<QVector<float>>& channels)
{
    if (channels.isEmpty() || channels[0].isEmpty())
        return {};
    if (channels.size() == 1)
        return channels[0];

    const int n = qMin(channels[0].size(), channels[1].size());
    QVector<float> mono;
    mono.reserve(n);
    for (int i = 0; i < n; ++i)
        mono.append(0.5f * (channels[0][i] + channels[1][i]));
    return mono;
}

} // namespace AudioFileService
