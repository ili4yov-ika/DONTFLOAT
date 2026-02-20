#include "../include/timestretchprocessor.h"
#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float computeRMS(const QVector<float>& v)
{
    if (v.isEmpty()) return 0.0f;
    double sum = 0.0;
    for (float x : v) sum += double(x) * double(x);
    return static_cast<float>(std::sqrt(sum / v.size()));
}

QVector<float> TimeStretchProcessor::processSegment(const QVector<float>& input, float stretchFactor, bool preservePitch)
{
    if (input.isEmpty() || stretchFactor <= 0.0f) {
        return input;
    }

    // Если коэффициент равен 1.0, возвращаем исходные данные
    if (qAbs(stretchFactor - 1.0f) < 0.001f) {
        return input;
    }

    QVector<float> output;
    if (preservePitch) {
        output = processWithPitchPreservation(input, stretchFactor);
    } else {
        output = processWithSimpleInterpolation(input, stretchFactor);
    }

    // Тонкомпенсация: приводим громкость выхода к громкости входа (по RMS)
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

QVector<float> TimeStretchProcessor::processWithPitchPreservation(const QVector<float>& input, float stretchFactor)
{
    // WSOLA: time-stretch в временной области с сохранением высоты тона
    const int inputSize = input.size();
    const int targetSize = static_cast<int>(inputSize * stretchFactor);

    if (targetSize <= 0) {
        return QVector<float>();
    }

    const int windowSize = 2048;
    const int analysisHop = windowSize / 2;
    const int outputHop = qMax(1, static_cast<int>(std::round(analysisHop * stretchFactor)));
    const int overlap = windowSize - outputHop;
    const int searchRadius = analysisHop / 2;

    if (inputSize < windowSize * 2 || overlap < 64) {
        return processWithSimpleInterpolation(input, stretchFactor);
    }

    QVector<float> window(windowSize);
    for (int i = 0; i < windowSize; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (windowSize - 1)));
    }

    const int outputCapacity = targetSize + windowSize;
    QVector<float> output(outputCapacity, 0.0f);
    QVector<float> windowSum(outputCapacity, 0.0f);

    auto addWindowedFrame = [&](int inputPos, int outputPos) {
        for (int i = 0; i < windowSize; ++i) {
            int inIdx = inputPos + i;
            int outIdx = outputPos + i;
            if (inIdx >= inputSize || outIdx >= outputCapacity) {
                break;
            }
            float w = window[i];
            output[outIdx] += input[inIdx] * w;
            windowSum[outIdx] += w;
        }
    };

    int inputPos = 0;
    int outputPos = 0;

    // Первый кадр без выравнивания
    addWindowedFrame(inputPos, outputPos);

    while (true) {
        inputPos += analysisHop;
        outputPos += outputHop;

        if (inputPos + windowSize >= inputSize) {
            break;
        }
        if (outputPos + windowSize >= outputCapacity) {
            break;
        }

        const int searchStart = qMax(0, inputPos - searchRadius);
        const int searchEnd = qMin(inputSize - windowSize, inputPos + searchRadius);

        int bestPos = inputPos;
        float bestCorr = -1.0e30f;

        for (int cand = searchStart; cand <= searchEnd; ++cand) {
            float corr = 0.0f;
            for (int i = 0; i < overlap; ++i) {
                float outSample = output[outputPos + i];
                float inSample = input[cand + i];
                corr += outSample * inSample;
            }
            if (corr > bestCorr) {
                bestCorr = corr;
                bestPos = cand;
            }
        }

        addWindowedFrame(bestPos, outputPos);
    }

    // Нормализация по оконной сумме
    for (int i = 0; i < targetSize; ++i) {
        if (windowSum[i] > 0.0001f) {
            output[i] /= windowSum[i];
        }
    }

    output.resize(targetSize);
    return output;
}

QVector<float> TimeStretchProcessor::resample(const QVector<float>& input, float inputSampleRate, float outputSampleRate)
{
    if (input.isEmpty() || inputSampleRate <= 0.0f || outputSampleRate <= 0.0f) {
        return input;
    }

    // Если частоты совпадают, возвращаем исходные данные
    if (qAbs(inputSampleRate - outputSampleRate) < 0.1f) {
        return input;
    }

    float ratio = outputSampleRate / inputSampleRate;
    int outputSize = static_cast<int>(input.size() * ratio);

    if (outputSize <= 0) {
        return QVector<float>();
    }

    QVector<float> output;
    output.reserve(outputSize);

    // Используем кубическую интерполяцию для качественного ресемплинга
    for (int i = 0; i < outputSize; ++i) {
        float inputPos = static_cast<float>(i) / ratio;
        inputPos = qBound(0.0f, inputPos, static_cast<float>(input.size() - 1));

        int index = static_cast<int>(inputPos);
        float fraction = inputPos - static_cast<float>(index);

        // Кубическая интерполяция
        int i0 = qMax(0, index - 1);
        int i1 = index;
        int i2 = qMin(static_cast<int>(input.size()) - 1, index + 1);
        int i3 = qMin(static_cast<int>(input.size()) - 1, index + 2);

        float y0 = input[i0];
        float y1 = input[i1];
        float y2 = input[i2];
        float y3 = input[i3];

        float value = cubicInterpolate(y0, y1, y2, y3, fraction);
        output.append(value);
    }

    return output;
}

QVector<QVector<float>> TimeStretchProcessor::processChannels(const QVector<QVector<float>>& input, float stretchFactor, bool preservePitch)
{
    QVector<QVector<float>> output;
    output.reserve(input.size());

    for (const auto& channel : input) {
        output.append(processSegment(channel, stretchFactor, preservePitch));
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

    qint64 currentOutputPos = 0;

    // Обрабатываем каждый сегмент
    for (int i = 0; i < segments.size(); ++i) {
        const StretchSegment& seg = segments[i];

        qint64 segmentLength = seg.endSample - seg.startSample;
        if (segmentLength <= 0) {
            qDebug() << "Segment" << i << "has zero or negative length, skipping";
            continue;
        }

        qDebug() << "Processing segment" << i << ": start=" << seg.startSample
                 << ", end=" << seg.endSample << ", length=" << segmentLength
                 << ", factor=" << seg.stretchFactor;

        qint64 processedLength = 0;

        // Обрабатываем каждый канал
        for (int ch = 0; ch < audioData.size(); ++ch) {
            // Извлекаем сегмент
            QVector<float> segment;
            segment.reserve(segmentLength);
            for (qint64 j = seg.startSample; j < seg.endSample && j < audioData[ch].size(); ++j) {
                segment.append(audioData[ch][j]);
            }

            // Применяем time stretching
            QVector<float> processedSegment = processSegment(
                segment,
                seg.stretchFactor,
                seg.preservePitch
            );

            result.audioData[ch].append(processedSegment);

            if (ch == 0) {
                processedLength = processedSegment.size();
                qDebug() << "Segment" << i << "processed to" << processedLength << "samples";
            }
        }

        currentOutputPos += processedLength;
    }

    // Обновляем метки под новые позиции
    result.newMarkers.clear();
    result.newMarkers.reserve(sortedMarkers.size());

    currentOutputPos = 0;

    // Первая метка (если есть) в начале
    if (!sortedMarkers.isEmpty() && sortedMarkers.first().originalPosition == 0) {
        MarkerData firstMarker = sortedMarkers.first();
        firstMarker.position = 0;
        firstMarker.originalPosition = 0;
        firstMarker.updateTimeFromSamples(sampleRate);
        result.newMarkers.append(firstMarker);
    }

    // Обновляем позиции остальных меток на основе обработанных сегментов
    for (int i = 0; i < segments.size() && i < sortedMarkers.size(); ++i) {
        const StretchSegment& seg = segments[i];

        qint64 segmentLength = seg.endSample - seg.startSample;
        qint64 processedLength = qint64(segmentLength * seg.stretchFactor);

        currentOutputPos += processedLength;

        // Метка в конце этого сегмента
        if (i + 1 < sortedMarkers.size()) {
            MarkerData newMarker = sortedMarkers[i + 1];
            newMarker.position = currentOutputPos;
            newMarker.originalPosition = newMarker.position;
            newMarker.updateTimeFromSamples(sampleRate);
            result.newMarkers.append(newMarker);
        }
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
