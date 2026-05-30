#include "../include/wavwriter.h"

#include <QtCore/QFile>
#include <QtCore/QRandomGenerator>
#include <QtCore/QtGlobal>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

void writeLE16(QFile& file, quint16 value)
{
    char b[2];
    b[0] = static_cast<char>(value & 0xFF);
    b[1] = static_cast<char>((value >> 8) & 0xFF);
    file.write(b, 2);
}

void writeLE24(QFile& file, qint32 value)
{
    value = qBound(-8388608, value, 8388607);
    char b[3];
    b[0] = static_cast<char>(value & 0xFF);
    b[1] = static_cast<char>((value >> 8) & 0xFF);
    b[2] = static_cast<char>((value >> 16) & 0xFF);
    file.write(b, 3);
}

void writeLE32(QFile& file, quint32 value)
{
    char b[4];
    b[0] = static_cast<char>(value & 0xFF);
    b[1] = static_cast<char>((value >> 8) & 0xFF);
    b[2] = static_cast<char>((value >> 16) & 0xFF);
    b[3] = static_cast<char>((value >> 24) & 0xFF);
    file.write(b, 4);
}

void writeFloat32(QFile& file, float value)
{
    quint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeLE32(file, bits);
}

float clampUnit(float sample)
{
    if (sample > 1.0f)
        return 1.0f;
    if (sample < -1.0f)
        return -1.0f;
    return sample;
}

float tpdfDither()
{
    return static_cast<float>(QRandomGenerator::global()->generateDouble()
                            + QRandomGenerator::global()->generateDouble()
                            - 1.0);
}

qint32 quantizePcm16(float sample, bool dither)
{
    const float scaled = clampUnit(sample) * 32767.0f;
    const float noisy = dither ? scaled + tpdfDither() : scaled;
    return qBound(-32768, static_cast<int>(std::lround(noisy)), 32767);
}

qint32 quantizePcm24(float sample)
{
    const float scaled = clampUnit(sample) * 8388607.0f;
    return qBound(-8388608, static_cast<int>(std::lround(scaled)), 8388607);
}

struct FormatInfo {
    quint16 audioFormat = 1;
    int bitsPerSample = 16;
    int bytesPerSample = 2;
};

FormatInfo formatInfo(WavWriter::SampleFormat format)
{
    switch (format) {
    case WavWriter::SampleFormat::Pcm24:
        return {1, 24, 3};
    case WavWriter::SampleFormat::Float32:
        return {3, 32, 4};
    case WavWriter::SampleFormat::Pcm16:
    default:
        return {1, 16, 2};
    }
}

} // namespace

namespace WavWriter {

bool writeFile(const QString& filePath,
               const QVector<QVector<float>>& channels,
               int sampleRate,
               QString* errorMessage,
               const WriteOptions& options)
{
    auto fail = [&](const QString& msg) -> bool {
        if (errorMessage)
            *errorMessage = msg;
        return false;
    };

    if (channels.isEmpty() || channels[0].isEmpty())
        return fail(QStringLiteral("Нет аудиоданных для сохранения"));
    if (sampleRate <= 0)
        return fail(QStringLiteral("Некорректная частота дискретизации"));

    const int channelCount = channels.size();
    const qint64 frames = channels[0].size();

    for (int ch = 1; ch < channelCount; ++ch) {
        if (channels[ch].size() != frames)
            return fail(QStringLiteral("Каналы имеют разную длину"));
    }

    const FormatInfo fmt = formatInfo(options.format);
    const qint64 byteRate = qint64(sampleRate) * channelCount * fmt.bytesPerSample;
    const int blockAlign = channelCount * fmt.bytesPerSample;
    const qint64 dataChunkSize = frames * channelCount * fmt.bytesPerSample;
    const qint64 riffChunkSize = 36 + dataChunkSize;

    if (dataChunkSize > std::numeric_limits<quint32>::max())
        return fail(QStringLiteral("Файл слишком большой для формата WAV (лимит 4 ГБ)"));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());

    file.write("RIFF", 4);
    writeLE32(file, static_cast<quint32>(riffChunkSize));
    file.write("WAVE", 4);

    file.write("fmt ", 4);
    writeLE32(file, 16);
    writeLE16(file, fmt.audioFormat);
    writeLE16(file, static_cast<quint16>(channelCount));
    writeLE32(file, static_cast<quint32>(sampleRate));
    writeLE32(file, static_cast<quint32>(byteRate));
    writeLE16(file, static_cast<quint16>(blockAlign));
    writeLE16(file, static_cast<quint16>(fmt.bitsPerSample));

    file.write("data", 4);
    writeLE32(file, static_cast<quint32>(dataChunkSize));

    const bool dither = options.dither && options.format == SampleFormat::Pcm16;

    for (qint64 i = 0; i < frames; ++i) {
        for (int ch = 0; ch < channelCount; ++ch) {
            const float sample = channels[ch][static_cast<int>(i)];

            switch (options.format) {
            case SampleFormat::Float32:
                writeFloat32(file, sample);
                break;
            case SampleFormat::Pcm24:
                writeLE24(file, quantizePcm24(sample));
                break;
            case SampleFormat::Pcm16:
            default:
                writeLE16(file, static_cast<quint16>(
                    static_cast<qint16>(quantizePcm16(sample, dither))));
                break;
            }
        }
    }

    file.close();
    return true;
}

} // namespace WavWriter
