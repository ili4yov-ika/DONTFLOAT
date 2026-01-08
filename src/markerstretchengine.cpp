#include "../include/markerstretchengine.h"
#include "../include/timestretchprocessor.h"
#include <QtCore/QDebug>
#include <algorithm>

MarkerStretchResult applyTimeStretchToMarkers(
    const QVector<QVector<float>>& audioData,
    const QVector<Marker>& markers,
    int sampleRate)
{
    MarkerStretchResult result;

    if (audioData.isEmpty() || markers.isEmpty()) {
        qDebug() << "applyTimeStretchToMarkers: audioData or markers is empty";
        result.audioData = audioData;
        result.newMarkers = markers;
        return result;
    }

    qDebug() << "applyTimeStretchToMarkers: processing" << markers.size()
             << "markers, audioSize:" << audioData[0].size();

    // Копируем метки и сортируем по originalPosition для корректной обработки
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(), [](const Marker& a, const Marker& b) {
        return a.originalPosition < b.originalPosition;
    });

    // Отладочный вывод меток
    for (int i = 0; i < sortedMarkers.size(); ++i) {
        qDebug() << "Marker" << i << ": position=" << sortedMarkers[i].position
                 << ", originalPosition=" << sortedMarkers[i].originalPosition
                 << ", isFixed=" << sortedMarkers[i].isFixed
                 << ", isEndMarker=" << sortedMarkers[i].isEndMarker;
    }

    result.audioData.reserve(audioData.size());

    // Инициализируем результат пустыми каналами
    for (int ch = 0; ch < audioData.size(); ++ch) {
        result.audioData.append(QVector<float>());
    }

    qint64 audioSize = audioData[0].size();
    qint64 currentOutputPos = 0; // Текущая позиция в обработанном аудио

    // Обрабатываем участок от начала аудио до первой метки (если первая метка не в позиции 0)
    if (!sortedMarkers.isEmpty() && sortedMarkers.first().originalPosition > 0) {
        qint64 segmentStart = 0;
        qint64 segmentEnd = sortedMarkers.first().originalPosition;

        // Вычисляем коэффициент для начального участка
        // Если первая метка перемещена, растягиваем/сжимаем начальный участок пропорционально
        qint64 originalLength = segmentEnd - segmentStart;
        qint64 targetLength = sortedMarkers.first().position - 0;
        float stretchFactor = (originalLength > 0) ?
            static_cast<float>(targetLength) / static_cast<float>(originalLength) : 1.0f;

        if (originalLength > 0 && segmentStart < segmentEnd) {
            qint64 processedLength = 0;
            for (int ch = 0; ch < audioData.size(); ++ch) {
                QVector<float> segment;
                segment.reserve(segmentEnd - segmentStart);
                for (qint64 j = segmentStart; j < segmentEnd; ++j) {
                    segment.append(audioData[ch][j]);
                }

                QVector<float> processedSegment = TimeStretchProcessor::processSegment(segment, stretchFactor, true);
                result.audioData[ch].append(processedSegment);

                // Сохраняем длину обработанного сегмента (берем из первого канала)
                if (ch == 0) {
                    processedLength = processedSegment.size();
                }
            }

            // Обновляем позицию для следующей метки
            currentOutputPos = processedLength;
        }
    }

    // Добавляем первую метку (если она есть)
    if (!sortedMarkers.isEmpty()) {
        Marker firstMarker = sortedMarkers.first();
        if (firstMarker.originalPosition == 0) {
            firstMarker.position = 0;
            firstMarker.originalPosition = 0;
            firstMarker.updateTimeFromSamples(sampleRate);
            result.newMarkers.append(firstMarker);
        } else {
            // Первая метка не в начале - добавляем её с обновленной позицией
            firstMarker.position = currentOutputPos;
            firstMarker.originalPosition = currentOutputPos;
            firstMarker.updateTimeFromSamples(sampleRate);
            result.newMarkers.append(firstMarker);
        }
    }

    // Обрабатываем каждый сегмент между метками
    for (int i = 0; i < sortedMarkers.size() - 1; ++i) {
        const Marker& startMarker = sortedMarkers[i];
        const Marker& endMarker = sortedMarkers[i + 1];

        // Вычисляем коэффициенты сжатия-растяжения
        qint64 originalDistance = endMarker.originalPosition - startMarker.originalPosition;
        qint64 currentDistance = endMarker.position - startMarker.position;

        if (originalDistance <= 0) {
            // Пропускаем некорректные сегменты
            qDebug() << "Segment" << i << ": skipping (originalDistance <= 0)";
            continue;
        }

        float stretchFactor = static_cast<float>(currentDistance) / static_cast<float>(originalDistance);

        qDebug() << "Segment" << i << ": originalDistance=" << originalDistance
                 << ", currentDistance=" << currentDistance
                 << ", stretchFactor=" << stretchFactor;

        // Извлекаем сегмент из исходных данных (по originalPosition)
        qint64 segmentStart = startMarker.originalPosition;
        qint64 segmentEnd = endMarker.originalPosition;

        // Ограничиваем границами аудио
        segmentStart = qBound(qint64(0), segmentStart, qint64(audioSize - 1));
        segmentEnd = qBound(qint64(0), segmentEnd, qint64(audioSize - 1));

        if (segmentStart >= segmentEnd) {
            continue;
        }

        // Обрабатываем каждый канал
        qint64 processedLength = 0;
        for (int ch = 0; ch < audioData.size(); ++ch) {
            // Извлекаем сегмент
            QVector<float> segment;
            segment.reserve(segmentEnd - segmentStart);
            for (qint64 j = segmentStart; j < segmentEnd; ++j) {
                segment.append(audioData[ch][j]);
            }

            // Применяем time stretching с тонкомпенсацией
            QVector<float> processedSegment = TimeStretchProcessor::processSegment(segment, stretchFactor, true);
            result.audioData[ch].append(processedSegment);

            if (ch == 0) {
                processedLength = processedSegment.size();
            }
        }

        // Обновляем позиции меток в выходном аудио
        Marker newEndMarker = endMarker;
        newEndMarker.position = currentOutputPos + processedLength;
        newEndMarker.originalPosition = newEndMarker.position;
        newEndMarker.updateTimeFromSamples(sampleRate);

        // Добавляем метку конца сегмента (кроме последней — она будет обработана отдельно)
        if (i + 1 < sortedMarkers.size() - 1) {
            result.newMarkers.append(newEndMarker);
        } else {
            // Для последней пары меток — пока только обновляем currentOutputPos,
            // сама конечная метка будет обработана ниже
        }

        currentOutputPos += processedLength;
    }

    // Обрабатываем хвост после последней метки, если он есть
    if (!sortedMarkers.isEmpty()) {
        const Marker& lastMarker = sortedMarkers.last();

        qint64 segmentStart = lastMarker.originalPosition;
        qint64 segmentEnd = audioSize;

        if (segmentStart < segmentEnd) {
            // Вычисляем stretchFactor для хвоста: ориентируемся на смещение последней метки
            qint64 originalLength = audioSize - lastMarker.originalPosition;
            qint64 targetLength = (audioSize - lastMarker.originalPosition) +
                                  (lastMarker.position - lastMarker.originalPosition);

            float stretchFactor = (originalLength > 0) ?
                static_cast<float>(targetLength) / static_cast<float>(originalLength) : 1.0f;

            qint64 processedLength = 0;
            for (int ch = 0; ch < audioData.size(); ++ch) {
                QVector<float> segment;
                segment.reserve(segmentEnd - segmentStart);
                for (qint64 j = segmentStart; j < segmentEnd; ++j) {
                    segment.append(audioData[ch][j]);
                }

                QVector<float> processedSegment = TimeStretchProcessor::processSegment(segment, stretchFactor, true);
                result.audioData[ch].append(processedSegment);

                if (ch == 0) {
                    processedLength = processedSegment.size();
                }
            }

            currentOutputPos += processedLength;
        }

        // Добавляем/обновляем конечную метку
        Marker endMarker = lastMarker;
        endMarker.position = currentOutputPos - 1;
        if (endMarker.position < 0) {
            endMarker.position = 0;
        }
        endMarker.originalPosition = endMarker.position;
        endMarker.updateTimeFromSamples(sampleRate);

        // Гарантируем, что конечная метка присутствует и помечена как isEndMarker
        endMarker.isEndMarker = true;
        result.newMarkers.append(endMarker);
    }

    return result;
}

// Реализация команды undo/redo для тайм-стретча перенесена сюда, чтобы
// логика обработки по меткам и отката/повтора изменений находилась в одном модуле.

TimeStretchCommand::TimeStretchCommand(WaveformView* view,
                                       const QVector<QVector<float>>& oldData,
                                       const QVector<QVector<float>>& newData,
                                       const QVector<Marker>& oldMarkers,
                                       const QVector<Marker>& newMarkers,
                                       const QString& text)
    : QUndoCommand(text)
    , waveformView(view)
    , oldAudioData(oldData)
    , newAudioData(newData)
    , oldMarkers(oldMarkers)
    , newMarkers(newMarkers)
{
}

void TimeStretchCommand::undo()
{
    if (waveformView) {
        waveformView->setAudioData(oldAudioData);
        waveformView->setMarkers(oldMarkers);
        // ВАЖНО: Восстанавливаем originalAudioData из старых данных
        waveformView->updateOriginalData(oldAudioData);
    }
}

void TimeStretchCommand::redo()
{
    if (waveformView) {
        qDebug() << "TimeStretchCommand::redo(): applying new audio data size:"
                 << (newAudioData.isEmpty() ? 0 : newAudioData[0].size())
                 << "and" << newMarkers.size() << "markers";
        waveformView->setAudioData(newAudioData);
        waveformView->setMarkers(newMarkers);
        // ВАЖНО: Обновляем originalAudioData на новые данные
        waveformView->updateOriginalData(newAudioData);
        qDebug() << "TimeStretchCommand::redo(): data applied successfully";
    }
}
