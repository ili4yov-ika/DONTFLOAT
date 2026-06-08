#include "../include/timestretchprocessor.h"
#include "../include/rubberband_offline.h"
#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <cmath>
#include <algorithm>

namespace {

void appendWithCrossfade(QVector<float>& dst, const QVector<float>& src, int crossfadeSamples)
{
    if (src.isEmpty()) {
        return;
    }
    if (dst.isEmpty() || crossfadeSamples <= 0) {
        dst.append(src);
        return;
    }

    const int overlap = qMin(crossfadeSamples, qMin(dst.size(), src.size()));
    if (overlap <= 1) {
        dst.append(src);
        return;
    }

    const int joinStart = dst.size() - overlap;
    for (int i = 0; i < overlap; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(overlap - 1);
        dst[joinStart + i] = dst[joinStart + i] * (1.0f - t) + src[i] * t;
    }
    for (int i = overlap; i < src.size(); ++i) {
        dst.append(src[i]);
    }
}

} // namespace

static float computeRMS(const QVector<float>& v)
{
    if (v.isEmpty()) return 0.0f;
    double sum = 0.0;
    for (float x : v) sum += double(x) * double(x);
    return static_cast<float>(std::sqrt(sum / v.size()));
}

QVector<float> TimeStretchProcessor::processSegment(const QVector<float>& input, float stretchFactor, bool preservePitch, int sampleRate)
{
    if (input.isEmpty() || stretchFactor <= 0.0f) {
        return input;
    }

    // Если коэффициент равен 1.0, возвращаем исходные данные
    if (qAbs(stretchFactor - 1.0f) < 0.001f) {
        return input;
    }

    const int effectiveSampleRate = sampleRate > 0 ? sampleRate : 44100;

    QVector<float> output;
    if (preservePitch) {
        output = processWithPitchPreservation(input, stretchFactor, effectiveSampleRate);
    } else {
        output = processWithSimpleInterpolation(input, stretchFactor);
    }

    // Нормализация громкости: приводим RMS выхода к RMS входа
    if (!output.isEmpty()) {
        float rmsIn = computeRMS(input);
        float rmsOut = computeRMS(output);
        const float eps = 1e-6f;
        if (rmsOut > eps && rmsIn > eps) {
            float gain = rmsIn / rmsOut;
            for (float& s : output) s *= gain;
        }
    }

    return output;
}

QVector<float> TimeStretchProcessor::processWithSimpleInterpolation(const QVector<float>& input, float stretchFactor)
{
    int inputSize = input.size();
    int outputSize = static_cast<int>(inputSize * stretchFactor);

    if (outputSize <= 0) {
        return QVector<float>();
    }

    QVector<float> output;
    output.reserve(outputSize);

    // Используем кубическую интерполяцию для более качественного результата
    for (int i = 0; i < outputSize; ++i) {
        // Вычисляем позицию во входном массиве
        float inputPos = (static_cast<float>(i) / static_cast<float>(outputSize - 1)) * static_cast<float>(inputSize - 1);

        // Ограничиваем позицию границами массива
        inputPos = qBound(0.0f, inputPos, static_cast<float>(inputSize - 1));

        int index = static_cast<int>(inputPos);
        float fraction = inputPos - static_cast<float>(index);

        // Для кубической интерполяции нужны 4 точки
        int i0 = qMax(0, index - 1);
        int i1 = index;
        int i2 = qMin(inputSize - 1, index + 1);
        int i3 = qMin(inputSize - 1, index + 2);

        float y0 = input[i0];
        float y1 = input[i1];
        float y2 = input[i2];
        float y3 = input[i3];

        float value = cubicInterpolate(y0, y1, y2, y3, fraction);
        output.append(value);
    }

    return output;
}

QVector<float> TimeStretchProcessor::processWithPitchPreservation(const QVector<float>& input, float stretchFactor, int sampleRate)
{
    return RubberBandOffline::stretchMono(input, stretchFactor, sampleRate);
}

QVector<QVector<float>> TimeStretchProcessor::processChannels(const QVector<QVector<float>>& input, float stretchFactor, bool preservePitch, int sampleRate)
{
    QVector<QVector<float>> output;
    output.reserve(input.size());

    for (const auto& channel : input) {
        output.append(processSegment(channel, stretchFactor, preservePitch, sampleRate));
    }

    return output;
}

float TimeStretchProcessor::lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float TimeStretchProcessor::cubicInterpolate(float y0, float y1, float y2, float y3, float t)
{
    // Кубическая интерполяция Catmull-Rom
    float t2 = t * t;
    float t3 = t2 * t;

    float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float a2 = -0.5f * y0 + 0.5f * y2;
    float a3 = y1;

    return a0 * t3 + a1 * t2 + a2 * t + a3;
}

// ============================================================================
// ВЫСОКОУРОВНЕВЫЕ МЕТОДЫ ДЛЯ РАБОТЫ С МЕТКАМИ
// ============================================================================

#include <QtCore/QDebug>
#include <algorithm>

TimeStretchProcessor::StretchResult TimeStretchProcessor::applyMarkerStretch(
    const QVector<QVector<float>>& audioData,
    const QVector<MarkerData>& markers,
    int sampleRate,
    bool preservePitch)
{
    StretchResult result;

    if (audioData.isEmpty() || markers.isEmpty()) {
        qDebug() << "applyMarkerStretch: audioData or markers is empty";
        result.audioData = audioData;
        result.newMarkers = markers;
        return result;
    }

    // Валидация меток
    QString errorMsg;
    if (!validateMarkers(markers, audioData[0].size(), &errorMsg)) {
        qDebug() << "applyMarkerStretch: validation failed:" << errorMsg;
        result.audioData = audioData;
        result.newMarkers = markers;
        return result;
    }

    qDebug() << "applyMarkerStretch: processing" << markers.size()
             << "markers, audioSize:" << audioData[0].size();

    // Копируем метки и сортируем по originalPosition
    QVector<MarkerData> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const MarkerData& a, const MarkerData& b) {
        return a.originalPosition < b.originalPosition;
    });

    // Отладочный вывод меток
    for (int i = 0; i < sortedMarkers.size(); ++i) {
        qDebug() << "Marker" << i << ": position=" << sortedMarkers[i].position
                 << ", originalPosition=" << sortedMarkers[i].originalPosition
                 << ", isFixed=" << sortedMarkers[i].isFixed
                 << ", isEndMarker=" << sortedMarkers[i].isEndMarker;
    }

    // Вычисляем сегменты для обработки
    QVector<StretchSegment> segments = calculateSegments(sortedMarkers, audioData[0].size(), preservePitch);

    qDebug() << "applyMarkerStretch: calculated" << segments.size() << "segments";

    // Инициализируем результат
    result.audioData.reserve(audioData.size());
    for (int ch = 0; ch < audioData.size(); ++ch) {
        result.audioData.append(QVector<float>());
    }

    QVector<qint64> segmentOutputLengths;
    segmentOutputLengths.reserve(segments.size());

    const int crossfadeSamples = sampleRate > 0 ? qMax(64, sampleRate / 100) : 441; // ~10 ms

    // Обрабатываем каждый сегмент
    for (int i = 0; i < segments.size(); ++i) {
        const StretchSegment& seg = segments[i];

        const qint64 segmentLength = seg.endSample - seg.startSample;
        if (segmentLength <= 0) {
            qDebug() << "Segment" << i << "has zero or negative length, skipping";
            segmentOutputLengths.append(segmentOutputLengths.isEmpty() ? 0 : segmentOutputLengths.last());
            continue;
        }

        qDebug() << "Processing segment" << i << ": start=" << seg.startSample
                 << ", end=" << seg.endSample << ", length=" << segmentLength
                 << ", factor=" << seg.stretchFactor;

        const bool isFirstSegment = segmentOutputLengths.isEmpty();
        const qint64 lengthBefore = isFirstSegment ? 0 : result.audioData[0].size();

        // Обрабатываем каждый канал
        for (int ch = 0; ch < audioData.size(); ++ch) {
            QVector<float> segment;
            segment.reserve(static_cast<int>(segmentLength));
            for (qint64 j = seg.startSample; j < seg.endSample && j < audioData[ch].size(); ++j) {
                segment.append(audioData[ch][j]);
            }

            const QVector<float> processedSegment = processSegment(
                segment,
                seg.stretchFactor,
                seg.preservePitch,
                sampleRate
            );

            if (isFirstSegment) {
                result.audioData[ch].append(processedSegment);
            } else {
                appendWithCrossfade(result.audioData[ch], processedSegment, crossfadeSamples);
            }
        }

        const qint64 lengthAfter = result.audioData.isEmpty() ? 0 : result.audioData[0].size();
        const qint64 addedLength = lengthAfter - lengthBefore;
        qDebug() << "Segment" << i << "added" << addedLength << "samples, total" << lengthAfter;
        segmentOutputLengths.append(lengthAfter);
    }

    // Обновляем метки под новые позиции
    result.newMarkers.clear();
    result.newMarkers.reserve(sortedMarkers.size());

    // Первая метка (если есть) в начале
    if (!sortedMarkers.isEmpty() && sortedMarkers.first().originalPosition == 0) {
        MarkerData firstMarker = sortedMarkers.first();
        firstMarker.position = 0;
        firstMarker.originalPosition = 0;
        firstMarker.updateTimeFromSamples(sampleRate);
        result.newMarkers.append(firstMarker);
    }

    // Обновляем позиции остальных меток по фактической длине сегментов
    for (int i = 0; i < segments.size() && i + 1 < sortedMarkers.size(); ++i) {
        if (i >= segmentOutputLengths.size()) {
            break;
        }

        MarkerData newMarker = sortedMarkers[i + 1];
        newMarker.position = segmentOutputLengths[i];
        newMarker.originalPosition = newMarker.position;
        newMarker.updateTimeFromSamples(sampleRate);
        result.newMarkers.append(newMarker);
    }

    qDebug() << "applyMarkerStretch: result audioSize=" << result.audioData[0].size()
             << ", markers=" << result.newMarkers.size();

    return result;
}

QVector<TimeStretchProcessor::StretchSegment> TimeStretchProcessor::calculateSegments(
    const QVector<MarkerData>& markers,
    qint64 audioSize,
    bool preservePitch)
{
    QVector<StretchSegment> segments;

    if (markers.isEmpty() || audioSize <= 0) {
        return segments;
    }

    // Сортируем метки по originalPosition
    QVector<MarkerData> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const MarkerData& a, const MarkerData& b) {
        return a.originalPosition < b.originalPosition;
    });

    // Сегмент от начала до первой метки (если первая метка не в позиции 0)
    if (sortedMarkers.first().originalPosition > 0) {
        StretchSegment seg;
        seg.startSample = 0;
        seg.endSample = sortedMarkers.first().originalPosition;
        seg.stretchFactor = 1.0f; // Без растяжения
        seg.preservePitch = preservePitch;
        segments.append(seg);
        qDebug() << "Segment (initial): 0 ->" << seg.endSample << ", factor=1.0";
    }

    // Сегменты между метками
    for (int i = 0; i < sortedMarkers.size() - 1; ++i) {
        const MarkerData& startMarker = sortedMarkers[i];
        const MarkerData& endMarker = sortedMarkers[i + 1];

        StretchSegment seg;
        seg.startSample = startMarker.originalPosition;
        seg.endSample = endMarker.originalPosition;
        seg.stretchFactor = calculateStretchFactor(startMarker, endMarker);
        seg.preservePitch = preservePitch;
        segments.append(seg);

        qDebug() << "Segment" << i << ":" << seg.startSample << "->" << seg.endSample
                 << ", factor=" << seg.stretchFactor;
    }

    // Сегмент от последней метки до конца
    if (!sortedMarkers.isEmpty() && sortedMarkers.last().originalPosition < audioSize - 1) {
        const MarkerData& lastMarker = sortedMarkers.last();

        qint64 originalLength = audioSize - lastMarker.originalPosition;
        qint64 targetLength = originalLength +
                             (lastMarker.position - lastMarker.originalPosition);

        float stretchFactor = (originalLength > 0) ?
            static_cast<float>(targetLength) / static_cast<float>(originalLength) : 1.0f;

        StretchSegment seg;
        seg.startSample = lastMarker.originalPosition;
        seg.endSample = audioSize;
        seg.stretchFactor = stretchFactor;
        seg.preservePitch = preservePitch;
        segments.append(seg);

        qDebug() << "Segment (tail):" << seg.startSample << "->" << seg.endSample
                 << ", factor=" << seg.stretchFactor;
    }

    return segments;
}

bool TimeStretchProcessor::validateMarkers(
    const QVector<MarkerData>& markers,
    qint64 audioSize,
    QString* errorMsg)
{
    // Проверка минимального количества меток
    if (markers.size() < 2) {
        if (errorMsg) {
            *errorMsg = "Minimum 2 markers required";
        }
        return false;
    }

    // Проверка границ
    for (const MarkerData& marker : markers) {
        if (marker.originalPosition < 0 || marker.originalPosition > audioSize) {
            if (errorMsg) {
                *errorMsg = QString("Marker originalPosition %1 out of bounds [0, %2]")
                    .arg(marker.originalPosition).arg(audioSize);
            }
            return false;
        }
    }

    // Проверка коэффициентов растяжения
    QVector<MarkerData> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const MarkerData& a, const MarkerData& b) {
        return a.originalPosition < b.originalPosition;
    });

    for (int i = 0; i < sortedMarkers.size() - 1; ++i) {
        float factor = calculateStretchFactor(sortedMarkers[i], sortedMarkers[i + 1]);

        // Ограничение: минимум 0.1 (сжатие до 10%), максимум не ограничен
        if (factor < 0.1f) {
            if (errorMsg) {
                *errorMsg = QString("Stretch factor %1 is too small (min 0.1)")
                    .arg(factor);
            }
            return false;
        }
    }

    return true;
}

float TimeStretchProcessor::calculateStretchFactor(
    const MarkerData& startMarker,
    const MarkerData& endMarker)
{
    qint64 originalDistance = endMarker.originalPosition - startMarker.originalPosition;
    qint64 currentDistance = endMarker.position - startMarker.position;

    if (originalDistance <= 0) {
        return 1.0f;
    }

    return static_cast<float>(currentDistance) / static_cast<float>(originalDistance);
}
