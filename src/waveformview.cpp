#include "../include/waveformview.h"
#include "../include/timestretchprocessor.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QtGlobal>
#include <QtGui/QPainter>
#include <QtGui/QPolygon>
#include <QtGui/QBrush>
#include <QtCore/QTime>
#include <QtCore/QDebug>
#include <cmath>
#include <algorithm>

// Определение статических констант
const int WaveformView::minZoom = 1;
const int WaveformView::maxZoom = 1000;
const int WaveformView::markerHeight = 20;  // Высота области маркеров
const int WaveformView::markerSpacing = 60;  // Минимальное расстояние между маркерами в пикселях

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , bpm(120.0f)
    , sampleRate(44100)
    , playbackPosition(0)
    , gridStartSample(0)
    , horizontalOffset(0.0f)
    , verticalOffset(0.0f)
    , zoomLevel(1.0f)
    , isDragging(false)
    , isRightMousePanning(false)
    , loopStartPosition(0)
    , loopEndPosition(0)
    , scrollStep(0.1f)    // 10% от ширины окна
    , zoomStep(1.2f)      // 20% изменение масштаба
    , showTimeDisplay(true)
    , showBarsDisplay(false)
    , showBeatDeviations(true)
    , showBeatWaveform(true) // Показывать силуэт ударных по умолчанию
    , beatsAligned(false) // По умолчанию доли не выровнены
    , beatsPerBar(4)
    , draggingMarkerIndex(-1)
{
    setMinimumHeight(100);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // Разрешаем получение фокуса для обработки клавиш
}

void WaveformView::setAudioData(const QVector<QVector<float>>& channels)
{
    // Проверка на пустые данные
    if (channels.isEmpty()) {
        return;
    }

    // Проверка на корректность данных
    for (const auto& channel : channels) {
        if (channel.isEmpty()) {
            return;
        }
    }

    // Нормализация данных
    audioData.clear();
    for (const auto& channel : channels) {
        QVector<float> normalizedChannel;
        normalizedChannel.reserve(channel.size());
        
        // Находим максимальное значение для нормализации
        float maxValue = 0.0f;
        for (float sample : channel) {
            maxValue = qMax(maxValue, qAbs(sample));
        }
        
        // Нормализуем значения
        if (maxValue > 0.0f) {
            for (float sample : channel) {
                normalizedChannel.append(sample / maxValue);
            }
        } else {
            normalizedChannel = channel;
        }
        
        audioData.append(normalizedChannel);
    }

    // Сбрасываем масштаб и смещение
    zoomLevel = 1.0f;
    horizontalOffset = 0.0f;
    gridStartSample = 0;
    emit zoomChanged(zoomLevel);

    // Запускаем анализ ударных (временно отключено)
    // analyzeBeats();

    update();
}

void WaveformView::setBPM(float newBpm)
{
    bpm = newBpm;
    update();
}

void WaveformView::setZoomLevel(float zoom)
{
    float newZoom = qBound(float(minZoom), zoom, float(maxZoom));
    if (!qFuzzyCompare(newZoom, zoomLevel)) {
        zoomLevel = newZoom;
        emit zoomChanged(zoomLevel);
        update();
    }
}

void WaveformView::setSampleRate(int rate)
{
    sampleRate = rate;
    update();
}

void WaveformView::setBeatInfo(const QVector<BPMAnalyzer::BeatInfo>& newBeats)
{
    beats = newBeats;
    if (!beats.isEmpty()) {
        gridStartSample = beats.first().position;
    }
    update();
}

void WaveformView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Фон
    painter.fillRect(rect(), colors.getBackgroundColor());
    
    // Рисуем сетку
    drawGrid(painter, rect());
    
    // Рисуем волну ПЕРЕД отклонениями, чтобы отклонения были поверх
    if (!audioData.isEmpty()) {
        float channelHeight = height() / float(audioData.size());
        for (int i = 0; i < audioData.size(); ++i) {
            QRectF channelRect(0, i * channelHeight, width(), channelHeight);
            drawWaveform(painter, audioData[i], channelRect);
            
            // Рисуем силуэт ударных поверх основной волны (в стиле VirtualDJ)
            if (showBeatWaveform && !beats.isEmpty()) {
                drawBeatWaveform(painter, audioData[i], channelRect);
            }
        }
    }
    
    // Рисуем отклонения битов ПОСЛЕ волны, чтобы они были поверх с прозрачностью
    drawBeatDeviations(painter, rect());
    
    // Рисуем линии тактов
    drawBeatLines(painter, rect());
    
    // Рисуем маркеры тактов
    QRectF markerRect(0, 0, width(), markerHeight);
    drawBarMarkers(painter, markerRect);
    
    // Рисуем курсор воспроизведения
    drawPlaybackCursor(painter, rect());

    // Рисуем маркеры цикла
    drawLoopMarkers(painter, rect());
    
    // Рисуем метки ПОСЛЕ всех остальных элементов, чтобы они были поверх
    drawMarkers(painter, rect());
}

void WaveformView::drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect)
{
    if (samples.isEmpty()) return;

    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(samples.size()) / (rect.width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, samples.size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Применяем вертикальное смещение
    QRectF adjustedRect = rect;
    adjustedRect.translate(0, -verticalOffset * rect.height());
    
    for (int x = 0; x < rect.width(); ++x) {
        int currentSample = startSample + int(x * samplesPerPixel);
        int nextSample = startSample + int((x + 1) * samplesPerPixel);
        
        if (currentSample >= samples.size()) break;
        
        // Находим минимальное и максимальное значение для текущего пикселя
        float minValue = 0, maxValue = 0;
        for (int s = currentSample; s < qMin(nextSample, samples.size()); ++s) {
            minValue = qMin(minValue, samples[s]);
            maxValue = qMax(maxValue, samples[s]);
        }

        // Определяем цвет в зависимости от частоты
        float frequency = qAbs(maxValue - minValue);
        QColor waveColor;
        if (frequency < 0.3f) {
            waveColor = colors.getLowColor();
        } else if (frequency < 0.6f) {
            waveColor = colors.getMidColor();
        } else {
            waveColor = colors.getHighColor();
        }
        
        painter.setPen(waveColor);
        
        // Рисуем вертикальную линию от минимума до максимума с учетом вертикального смещения
        QPointF top = sampleToPoint(x, maxValue, adjustedRect);
        QPointF bottom = sampleToPoint(x, minValue, adjustedRect);
        painter.drawLine(top, bottom);
    }
}

void WaveformView::drawBeatWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect)
{
    if (samples.isEmpty() || beats.isEmpty()) return;
    
    // Количество сэмплов на пиксель с учетом масштаба (та же логика что и в drawWaveform)
    float samplesPerPixel = float(samples.size()) / (rect.width() * zoomLevel);
    
    // Вычисляем начальный сэмпл
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, samples.size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    int endSample = qMin(samples.size(), startSample + visibleSamples);
    
    // Применяем вертикальное смещение
    QRectF adjustedRect = rect;
    adjustedRect.translate(0, -verticalOffset * rect.height());
    
    // Оптимизация: предвычисляем энергетическую огибающую один раз для видимой области
    // Используем упрощенный алгоритм для лучшей производительности
    const int energyWindowSize = qRound(0.02f * sampleRate); // Окно 20ms для вычисления энергии (уменьшено с 10ms)
    QVector<float> energyEnvelope;
    energyEnvelope.reserve(endSample - startSample);
    
    // Вычисляем энергетическую огибающую для видимой области
    for (int s = startSample; s < endSample; ++s) {
        int localStart = qMax(startSample, s - energyWindowSize / 2);
        int localEnd = qMin(endSample, s + energyWindowSize / 2);
        
        // Быстрое вычисление энергии без полного RMS
        float energy = 0.0f;
        int count = 0;
        for (int i = localStart; i < localEnd; i += 4) { // Пропускаем каждый 4-й сэмпл для скорости
            energy += samples[i] * samples[i];
            count++;
        }
        if (count > 0) {
            energy = std::sqrt(energy / count);
        }
        energyEnvelope.append(energy);
    }
    
    // Создаем вектор для хранения энергии ударных для каждого пикселя
    QVector<float> beatEnvelope(rect.width(), 0.0f);
    
    // Для каждого видимого бита применяем усиление энергии
    const int beatWindowSize = qRound(0.03f * sampleRate); // Уменьшено с 50ms до 30ms
    float maxEnergy = 0.0f;
    
    // Находим максимальную энергию для нормализации
    for (float e : energyEnvelope) {
        maxEnergy = qMax(maxEnergy, e);
    }
    
    if (maxEnergy <= 0.0f) return; // Нет энергии - нечего рисовать
    
    // Оптимизация: обрабатываем только биты в видимой области
    for (int i = 0; i < beats.size(); ++i) {
        const auto& beat = beats[i];
        qint64 beatPos = beat.position;
        
        // Пропускаем биты далеко вне видимой области
        if (beatPos < startSample - beatWindowSize || beatPos > endSample + beatWindowSize) {
            continue;
        }
        
        // Вычисляем область влияния бита в пикселях
        float beatX = (beatPos - startSample) / samplesPerPixel;
        
        if (beatX < -beatWindowSize / samplesPerPixel || beatX > rect.width() + beatWindowSize / samplesPerPixel) {
            continue;
        }
        
        // Вычисляем диапазон влияния в пикселях
        int pixelStart = qMax(0, int(beatX - beatWindowSize / samplesPerPixel));
        int pixelEnd = qMin(int(rect.width()), int(beatX + beatWindowSize / samplesPerPixel));
        
        // Применяем усиление энергии в области бита
        for (int x = pixelStart; x < pixelEnd; ++x) {
            int envelopeIndex = int(x * samplesPerPixel);
            if (envelopeIndex >= 0 && envelopeIndex < energyEnvelope.size()) {
                float energy = energyEnvelope[envelopeIndex];
                
                // Вычисляем реальный индекс сэмпла для вычисления расстояния
                int actualSampleIndex = startSample + envelopeIndex;
                
                // Усиливаем энергию в зависимости от близости к биту и его параметров
                float distance = qAbs((beatPos - actualSampleIndex) / float(sampleRate)); // расстояние в секундах
                float falloff = qMax(0.0f, 1.0f - distance * 20.0f); // затухание на расстоянии
                float enhancedEnergy = energy * (1.0f + beat.energy * beat.confidence * falloff);
                
                beatEnvelope[x] = qMax(beatEnvelope[x], enhancedEnergy);
            }
        }
    }
    
    // Нормализуем энергию относительно максимальной
    if (maxEnergy > 0.0f) {
        float normalizationFactor = 1.0f / maxEnergy;
        for (int x = 0; x < beatEnvelope.size(); ++x) {
            beatEnvelope[x] *= normalizationFactor;
        }
    }
    
    // Оптимизация: рисуем одним полигоном вместо множества линий
    painter.setRenderHint(QPainter::Antialiasing, false); // Отключаем сглаживание для скорости
    
    // Используем один полигон для верхней и нижней части
    QPolygonF beatPolygon;
    beatPolygon.reserve(rect.width() * 2 + 2);
    
    float centerY = adjustedRect.center().y();
    float halfHeight = adjustedRect.height() * 0.5f;
    
    // Верхняя часть силуэта
    bool hasPoints = false;
    for (int x = 0; x < rect.width(); ++x) {
        if (beatEnvelope[x] > 0.001f) { // Порог для отсечения очень малых значений
            float normalizedEnergy = qBound(0.0f, beatEnvelope[x], 1.0f);
            float y = centerY - (normalizedEnergy * halfHeight);
            beatPolygon.append(QPointF(adjustedRect.x() + x, y));
            hasPoints = true;
        } else if (hasPoints && x < rect.width() - 1 && beatEnvelope[x + 1] <= 0.001f) {
            // Добавляем точку для плавного перехода к нулю
            beatPolygon.append(QPointF(adjustedRect.x() + x, centerY));
            hasPoints = false;
        }
    }
    
    // Переход к нижней части через центр
    if (!beatPolygon.isEmpty()) {
        beatPolygon.append(QPointF(adjustedRect.right(), centerY));
    }
    
    // Нижняя часть силуэта (в обратном порядке)
    hasPoints = false;
    for (int x = rect.width() - 1; x >= 0; --x) {
        if (beatEnvelope[x] > 0.001f) {
            float normalizedEnergy = qBound(0.0f, beatEnvelope[x], 1.0f);
            float y = centerY + (normalizedEnergy * halfHeight);
            beatPolygon.append(QPointF(adjustedRect.x() + x, y));
            hasPoints = true;
        } else if (hasPoints && x > 0 && beatEnvelope[x - 1] <= 0.001f) {
            // Добавляем точку для плавного перехода к нулю
            beatPolygon.append(QPointF(adjustedRect.x() + x, centerY));
            hasPoints = false;
        }
    }
    
    // Закрываем полигон
    if (!beatPolygon.isEmpty()) {
        beatPolygon.append(beatPolygon.first());
    }
    
    // Рисуем полигон одним вызовом
    if (beatPolygon.size() >= 3) {
        painter.setBrush(QBrush(QColor(255, 165, 0, 120))); // Полупрозрачная заливка
        painter.setPen(QPen(QColor(255, 165, 0, 200), 1)); // Тонкая обводка
        painter.drawPolygon(beatPolygon);
    }
}

void WaveformView::drawGrid(QPainter& painter, const QRect& rect)
{
    // Горизонтальные линии для разделения каналов
    painter.setPen(QPen(colors.getGridColor()));
    float channelHeight = rect.height() / float(qMax(1, audioData.size()));
    for (int i = 1; i < audioData.size(); ++i) {
        float y = i * channelHeight;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
}

void WaveformView::drawBeatLines(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty()) return;
    
    // Вычисляем длительность одного такта в сэмплах
    float samplesPerBeat = (60.0f * sampleRate) / bpm;
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Находим первый такт, выровненный по первой обнаруженной доле
    int firstBeat = 0;
    if (gridStartSample > 0) {
        // количество ударов от опорной точки до стартового сэмпла
        float beatsFromGridStart = float(startSample - gridStartSample) / samplesPerBeat;
        firstBeat = int(qFloor(beatsFromGridStart));
    } else {
        firstBeat = int(startSample / samplesPerBeat);
    }
    
    // Рисуем линии тактов с разной толщиной для сильных и слабых долей
    for (int beat = firstBeat; ; ++beat) {
        int samplePos = gridStartSample > 0
            ? int(gridStartSample + beat * samplesPerBeat)
            : int(beat * samplesPerBeat);
        if (samplePos < startSample) continue;
        
        float x = (samplePos - startSample) / samplesPerPixel;
        if (x >= rect.width()) break;
        
        // Определяем, является ли это сильной долей (первая доля такта)
        bool isStrongBeat = (beat % beatsPerBar) == 0;
        
        // Устанавливаем толщину и цвет пера в зависимости от типа доли
        if (isStrongBeat) {
            // Сильная доля - более жирная линия
            painter.setPen(QPen(colors.getBeatColor(), 2.0));
        } else {
            // Слабая доля - обычная линия
            painter.setPen(QPen(colors.getBeatColor(), 1.0));
        }
        
        painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));
    }
}

void WaveformView::drawPlaybackCursor(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty()) return;

    // playbackPosition уже в миллисекундах, конвертируем в сэмплы
    qint64 cursorSample = (playbackPosition * sampleRate) / 1000;
    
    // Ограничиваем позицию каретки границами аудио
    cursorSample = qBound(qint64(0), cursorSample, qint64(audioData[0].size() - 1));
    
    // Используем ту же логику, что и в других методах рисования
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Вычисляем позицию каретки в пикселях
    float cursorX = (cursorSample - startSample) / samplesPerPixel;
    

    
    if (cursorX >= 0 && cursorX < rect.width()) {
        painter.setPen(QPen(colors.getPlayPositionColor(), 2));
        painter.drawLine(QPointF(rect.x() + cursorX, rect.top()), QPointF(rect.x() + cursorX, rect.bottom()));
        
        // Рисуем треугольник на курсоре
        QPolygonF triangle;
        triangle << QPointF(cursorX - 5, rect.top() + 5)
                << QPointF(cursorX + 5, rect.top() + 5)
                << QPointF(cursorX, rect.top() + 15);
        painter.setBrush(colors.getPlayPositionColor());
        painter.drawPolygon(triangle);
    }
}

void WaveformView::drawBarMarkers(QPainter& painter, const QRectF& rect)
{
    if (audioData.isEmpty()) return;
    
    painter.setPen(colors.getMarkerTextColor());
    painter.setFont(QFont("Arial", 8));
    
    // Вычисляем длительность одного такта в сэмплах
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    float samplesPerBar = samplesPerBeat * qMax(1, beatsPerBar);
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Находим первый такт перед видимой областью, выровненный по опорной доле
    float firstBar = 0.0f;
    if (gridStartSample > 0) {
        float barsFromGridStart = float(startSample - gridStartSample) / samplesPerBar;
        firstBar = floor(barsFromGridStart);
    } else {
        firstBar = floor(startSample / samplesPerBar);
    }
    
    // Рисуем маркеры тактов
    for (float bar = firstBar; ; bar++) {
        float barSample = gridStartSample > 0
            ? (gridStartSample + bar * samplesPerBar)
            : (bar * samplesPerBar);
        float x = (barSample - startSample) / samplesPerPixel;
        if (x >= rect.width()) break;
        if (x >= 0) {
            // Рисуем фон маркера
            QRectF markerRect(x - markerSpacing/2, rect.top(), markerSpacing, rect.height());
            painter.fillRect(markerRect, colors.getMarkerBackgroundColor());
            
            // Рисуем текст маркера
            QString barText = QString::number(int(bar) + 1);
            QRectF textRect = painter.fontMetrics().boundingRect(barText);
            painter.drawText(QPointF(
                x - textRect.width()/2,
                rect.top() + rect.height() - 4
            ), barText);
            
            // Рисуем вертикальную линию с разной толщиной для сильных долей
            // Линии тактов всегда сильные (начало нового такта)
            painter.setPen(QPen(colors.getBarLineColor(), 2.0));
            painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, height()));
        }
    }
}

void WaveformView::drawBeatDeviations(QPainter& painter, const QRectF& rect)
{
    if (beats.isEmpty() || audioData.isEmpty() || !showBeatDeviations || bpm <= 0.0f) {
        return;
    }

    // Используем ту же логику, что и в drawWaveform
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Вычисляем ожидаемый интервал между битами на основе BPM
    float expectedBeatInterval = (60.0f * sampleRate) / bpm;
    
    // Параметры для рисования полосок
    const float stripeHeight = 4.0f; // Ширина диагональной полоски (уменьшено для более тонких полосок)
    const float stripeSpacing = 3.0f; // Расстояние между полосками (уменьшено для большего количества)
    
    // Рисуем метки отклонений для каждого бита
    for (int i = 0; i < beats.size(); ++i) {
        const auto& beat = beats[i];
        
        // Пропускаем первый бит, так как у него нет отклонения
        if (i == 0) {
            continue;
        }
        
        // Проверяем, есть ли значимое отклонение (больше 2% от интервала)
        if (qAbs(beat.deviation) < 0.02f) {
            continue;
        }
        
        // Определяем ожидаемую позицию бита на основе предыдущего
        float expectedPosition = float(beats[i-1].position) + expectedBeatInterval;
        float actualPosition = float(beat.position);
        
        // Вычисляем координаты для отрисовки
        float expectedX = float(expectedPosition - startSample) / samplesPerPixel;
        float actualX = float(actualPosition - startSample) / samplesPerPixel;
        
        // Проверяем видимость
        if (actualX < 0 || expectedX > rect.width()) {
            continue;
        }
        
        // Определяем, растянута ли доля (положительное отклонение) или сжата (отрицательное)
        bool isStretched = beat.deviation > 0;
        
        // Выбираем цвет в зависимости от типа отклонения и состояния выравнивания
        // Добавляем прозрачность, чтобы волна была отчетливо видна
        QColor baseColor;
        int alpha = 100; // Прозрачность для полигонов (0-255, меньше = более прозрачно) - увеличено для лучшей видимости полосок
        if (isStretched) {
            // Растянутая доля - красный цвет
            if (beatsAligned) {
                baseColor = QColor(255, 80, 80, alpha); // Яркий красный для выровненных с прозрачностью
            } else {
                baseColor = QColor(255, 180, 180, alpha); // Бледно-красный для невыровненных с прозрачностью
            }
        } else {
            // Сжатая доля - синий цвет
            if (beatsAligned) {
                baseColor = QColor(80, 80, 255, alpha); // Яркий синий для выровненных с прозрачностью
            } else {
                baseColor = QColor(180, 180, 255, alpha); // Бледно-синий для невыровненных с прозрачностью
            }
        }
        
        // Вычисляем область между ожидаемой и фактической позицией
        float x1 = qMin(expectedX, actualX);
        float x2 = qMax(expectedX, actualX);
        float width = x2 - x1;
        
        if (width > 0.5f && x2 >= 0 && x1 <= rect.width()) {
            // Ограничиваем координаты видимой областью
            x1 = qMax(0.0f, x1);
            x2 = qMin(rect.width(), x2);
            width = x2 - x1;
            
            if (width > 0.5f) {
                // Рисуем вертикальную линию на фактической позиции бита (более прозрачная)
                if (actualX >= 0 && actualX <= rect.width()) {
                    QColor lineColor = baseColor;
                    lineColor.setAlpha(100); // Линия с умеренной прозрачностью
                    painter.setPen(QPen(lineColor, 1.5f));
                    painter.drawLine(QPointF(actualX, 0), QPointF(actualX, rect.height()));
                }
                
                // Рисуем полигон из диагональных полосок
                float stripeWidth = stripeHeight; // Ширина диагональной полоски
                float stripeGap = stripeSpacing; // Расстояние между полосками
                
                // Используем фиксированный угол наклона для диагоналей (45 градусов)
                #ifndef M_PI
                #define M_PI 3.14159265358979323846
                #endif
                const float diagonalAngle = 45.0f * float(M_PI) / 180.0f; // 45 градусов в радианах
                float cosAngle = qCos(diagonalAngle);
                float sinAngle = qSin(diagonalAngle);
                
                // Направляющий вектор диагонали
                float dirX, dirY;
                if (isStretched) {
                    // Для растянутых долей - диагональ вправо (от левого нижнего к правому верхнему)
                    dirX = cosAngle;
                    dirY = -sinAngle; // Отрицательный, так как Y растет вниз
                } else {
                    // Для сжатых долей - диагональ влево (от правого нижнего к левому верхнему)
                    dirX = -cosAngle;
                    dirY = -sinAngle;
                }
                
                // Перпендикулярный вектор для создания ширины полоски
                float perpX = -dirY * stripeWidth * 0.5f;
                float perpY = dirX * stripeWidth * 0.5f;
                
                // Вычисляем, сколько полосок нужно нарисовать
                // Полоски должны пересекать всю область, поэтому используем проекцию на перпендикуляр
                float perpLength = qAbs(perpX * width + perpY * rect.height());
                float totalStripeWidth = stripeWidth + stripeGap;
                int numStripes = static_cast<int>(perpLength / totalStripeWidth) + 3;
                
                painter.setPen(Qt::NoPen);
                painter.setBrush(QBrush(baseColor));
                
                // Область для обрезания
                QRectF clipRect(x1, 0, width, rect.height());
                painter.save();
                painter.setClipRect(clipRect);
                
                for (int stripe = 0; stripe < numStripes; ++stripe) {
                    // Смещение полоски вдоль перпендикуляра
                    float offset = stripe * totalStripeWidth - perpLength * 0.5f;
                    
                    // Находим две точки пересечения диагональной линии с границами прямоугольника
                    // Используем параметрическое представление линии
                    QPointF p1, p2;
                    
                    if (isStretched) {
                        // Для растянутых: диагональ от левого нижнего к правому верхнему
                        // Базовая точка на левой нижней границе с учетом смещения
                        QPointF basePoint(x1 + offset * perpX, rect.height() + offset * perpY);
                        
                        // Находим пересечения с границами прямоугольника
                        // Параметрическое уравнение: point = basePoint + t * dir
                        QVector<QPointF> intersections;
                        
                        // С верхней границей (y = 0)
                        if (dirY != 0) {
                            float t = (0 - basePoint.y()) / dirY;
                            float x = basePoint.x() + t * dirX;
                            if (t >= 0 && x >= x1 && x <= x2) {
                                intersections << QPointF(x, 0);
                            }
                        }
                        
                        // С правой границей (x = x2)
                        if (dirX != 0) {
                            float t = (x2 - basePoint.x()) / dirX;
                            float y = basePoint.y() + t * dirY;
                            if (t >= 0 && y >= 0 && y <= rect.height()) {
                                intersections << QPointF(x2, y);
                            }
                        }
                        
                        // С левой границей (x = x1)
                        if (dirX != 0) {
                            float t = (x1 - basePoint.x()) / dirX;
                            float y = basePoint.y() + t * dirY;
                            if (t >= 0 && y >= 0 && y <= rect.height()) {
                                intersections << QPointF(x1, y);
                            }
                        }
                        
                        // С нижней границей (y = rect.height())
                        if (dirY != 0) {
                            float t = (rect.height() - basePoint.y()) / dirY;
                            float x = basePoint.x() + t * dirX;
                            if (t >= 0 && x >= x1 && x <= x2) {
                                intersections << QPointF(x, rect.height());
                            }
                        }
                        
                        // Выбираем две точки (должно быть ровно 2 для линии, пересекающей прямоугольник)
                        if (intersections.size() >= 2) {
                            p1 = intersections[0];
                            p2 = intersections[1];
                        } else if (intersections.size() == 1) {
                            p1 = intersections[0];
                            p2 = QPointF(x2, 0);
                        } else {
                            p1 = QPointF(x1, rect.height());
                            p2 = QPointF(x2, 0);
                        }
                    } else {
                        // Для сжатых: диагональ от правого нижнего к левому верхнему
                        QPointF basePoint(x2 + offset * perpX, rect.height() + offset * perpY);
                        
                        QVector<QPointF> intersections;
                        
                        // С верхней границей (y = 0)
                        if (dirY != 0) {
                            float t = (0 - basePoint.y()) / dirY;
                            float x = basePoint.x() + t * dirX;
                            if (t >= 0 && x >= x1 && x <= x2) {
                                intersections << QPointF(x, 0);
                            }
                        }
                        
                        // С левой границей (x = x1)
                        if (dirX != 0) {
                            float t = (x1 - basePoint.x()) / dirX;
                            float y = basePoint.y() + t * dirY;
                            if (t >= 0 && y >= 0 && y <= rect.height()) {
                                intersections << QPointF(x1, y);
                            }
                        }
                        
                        // С правой границей (x = x2)
                        if (dirX != 0) {
                            float t = (x2 - basePoint.x()) / dirX;
                            float y = basePoint.y() + t * dirY;
                            if (t >= 0 && y >= 0 && y <= rect.height()) {
                                intersections << QPointF(x2, y);
                            }
                        }
                        
                        // С нижней границей (y = rect.height())
                        if (dirY != 0) {
                            float t = (rect.height() - basePoint.y()) / dirY;
                            float x = basePoint.x() + t * dirX;
                            if (t >= 0 && x >= x1 && x <= x2) {
                                intersections << QPointF(x, rect.height());
                            }
                        }
                        
                        if (intersections.size() >= 2) {
                            p1 = intersections[0];
                            p2 = intersections[1];
                        } else if (intersections.size() == 1) {
                            p1 = intersections[0];
                            p2 = QPointF(x1, 0);
                        } else {
                            p1 = QPointF(x2, rect.height());
                            p2 = QPointF(x1, 0);
                        }
                    }
                    
                    // Создаём полигон полоски (параллелограмм, пересекающий всю область)
                    QPolygonF polygon;
                    polygon << QPointF(p1.x() + perpX, p1.y() + perpY)
                            << QPointF(p2.x() + perpX, p2.y() + perpY)
                            << QPointF(p2.x() - perpX, p2.y() - perpY)
                            << QPointF(p1.x() - perpX, p1.y() - perpY);
                    
                    painter.drawPolygon(polygon);
                }
                
                painter.restore();
                
                // Добавляем бледный фильтр поверх полигонов для лучшей видимости волны
                // Рисуем полупрозрачный слой цвета фона поверх всей области отклонений
                QColor filterColor = colors.getBackgroundColor();
                filterColor.setAlpha(30); // Слегка бледный фильтр (около 12% непрозрачности) - уменьшено
                painter.fillRect(QRectF(x1, 0, width, rect.height()), filterColor);
            }
        }
    }
}

QString WaveformView::getPositionText(qint64 position) const
{
    QString positionText;
    
    if (showTimeDisplay) {
        // Конвертируем позицию в миллисекунды
        qint64 msPosition = (position * 1000) / sampleRate;
        int minutes = (msPosition / 60000);
        int seconds = (msPosition / 1000) % 60;
        int milliseconds = msPosition % 1000;
        positionText = QString("%1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(milliseconds / 100);
    }
    
    if (showBarsDisplay) {
        // Вычисляем такты и доли
        float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
        float beatsFromStart = float(position) / samplesPerBeat;
        const int bpb = qMax(1, beatsPerBar);
        int bar = int(beatsFromStart / bpb) + 1;  // Такты начинаются с 1
        int beat = int(beatsFromStart) % bpb + 1;  // Доли начинаются с 1
        float subBeat = (beatsFromStart - float(int(beatsFromStart))) * float(bpb); // Доли такта в beat units
        
        QString barText = QString("%1.%2.%3")
            .arg(bar)
            .arg(beat)
            .arg(int(subBeat + 1));
            
        if (!positionText.isEmpty()) {
            positionText += " | ";
        }
        positionText += barText;
    }
    
    return positionText;
}

void WaveformView::setPlaybackPosition(qint64 position)
{
    // position приходит в миллисекундах, сохраняем как есть
    playbackPosition = position;
    update();
}

float WaveformView::getCursorXPosition() const
{
    if (audioData.isEmpty()) return 0.0f;

    // playbackPosition уже в миллисекундах, конвертируем в сэмплы
    qint64 cursorSample = (playbackPosition * sampleRate) / 1000;
    
    // Ограничиваем позицию каретки границами аудио
    cursorSample = qBound(qint64(0), cursorSample, qint64(audioData[0].size() - 1));
    
    // Используем ту же логику, что и в drawPlaybackCursor
    float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Вычисляем позицию каретки в пикселях
    return (cursorSample - startSample) / samplesPerPixel;
}


QPointF WaveformView::sampleToPoint(int sampleIndex, float value, const QRectF& rect) const
{
    return QPointF(
        rect.x() + sampleIndex,
        rect.center().y() + (value * rect.height() * 0.5f)
    );
}

void WaveformView::wheelEvent(QWheelEvent* event)
{
    if (audioData.isEmpty()) {
        event->ignore();
        return;
    }
    
    float delta = event->angleDelta().y() / 120.f;
    float oldZoom = zoomLevel;
    float newZoom = oldZoom;
    
    if (delta > 0) {
        newZoom *= zoomStep;
    } else {
        newZoom /= zoomStep;
    }
    
    // Ограничиваем масштаб
    newZoom = qBound(float(minZoom), newZoom, float(maxZoom));
    
    if (qFuzzyCompare(newZoom, oldZoom)) {
        event->accept();
        return;
    }
    
    // Получаем позицию мыши относительно виджета
    QPoint mousePos = event->position().toPoint();
    
    // Вычисляем текущую позицию в сэмплах под курсором мыши
    float samplesPerPixel = float(audioData[0].size()) / (width() * oldZoom);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    qint64 mouseSample = startSample + qint64(mousePos.x() * samplesPerPixel);
    
    // Обновляем масштаб
    zoomLevel = newZoom;
    
    // Вычисляем новую позицию, чтобы курсор мыши остался над тем же сэмплом
    float newSamplesPerPixel = float(audioData[0].size()) / (width() * newZoom);
    int newVisibleSamples = int(width() * newSamplesPerPixel);
    int newMaxStartSample = qMax(0, audioData[0].size() - newVisibleSamples);
    
    // Вычисляем новое смещение
    float newOffset = 0.0f;
    if (newMaxStartSample > 0) {
        newOffset = float(mouseSample - mousePos.x() * newSamplesPerPixel) / newMaxStartSample;
        newOffset = qBound(0.0f, newOffset, 1.0f);
    }
    
    horizontalOffset = newOffset;
    
    // Эмитируем сигналы для обновления скроллбара
    emit zoomChanged(zoomLevel);
    emit horizontalOffsetChanged(horizontalOffset);
    update();
    
    event->accept();
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Проверяем, кликнули ли на метку
        int markerIndex = getMarkerIndexAt(event->pos());
        qDebug() << "mousePressEvent: markerIndex =" << markerIndex << "at pos:" << event->pos();
        if (markerIndex >= 0) {
            // Проверяем, не является ли метка неподвижной
            if (markers[markerIndex].isFixed) {
                qDebug() << "Marker" << markerIndex << "is fixed, cannot drag";
                // Неподвижная метка - не начинаем перетаскивание
                return;
            }
            
            // Начинаем перетаскивание метки
            draggingMarkerIndex = markerIndex;
            markers[markerIndex].isDragging = true;
            markers[markerIndex].dragStartPos = event->pos();
            markers[markerIndex].dragStartSample = markers[markerIndex].position;
            qDebug() << "Started dragging marker" << markerIndex << "at position:" << markers[markerIndex].position;
            setCursor(Qt::SizeHorCursor);
            return;
        }
        
        isDragging = false; // Начинаем как не перетаскивание
        lastMousePos = event->pos();

        // Устанавливаем позицию воспроизведения по клику
        if (!audioData.isEmpty()) {
            // Используем ту же логику, что и в drawPlaybackCursor
            float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
            int visibleSamples = int(width() * samplesPerPixel);
            int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
            int startSample = int(horizontalOffset * maxStartSample);
            
            // Вычисляем позицию клика в сэмплах
            qint64 clickSample = startSample + qint64(event->pos().x() * samplesPerPixel);
            clickSample = qBound(qint64(0), clickSample, qint64(audioData[0].size() - 1));
            
            // Конвертируем сэмплы в миллисекунды для внутреннего использования
            qint64 newPosition = (clickSample * 1000) / sampleRate;
            playbackPosition = newPosition;
            
            // Показываем всплывающую подсказку
            QString positionText = getPositionText(clickSample);
            if (!positionText.isEmpty()) {
                QToolTip::showText(event->globalPosition().toPoint(), positionText, this);
            }
            
            // Отправляем позицию в миллисекундах для QMediaPlayer
            emit positionChanged(newPosition);
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        // Начинаем панорамирование правой кнопкой мыши
        isRightMousePanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    // Обработка перетаскивания метки
    if (draggingMarkerIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!audioData.isEmpty()) {
            // Вычисляем новую позицию метки
            float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
            int visibleSamples = int(width() * samplesPerPixel);
            int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
            int startSample = int(horizontalOffset * maxStartSample);
            
            qint64 newSample = startSample + qint64(event->pos().x() * samplesPerPixel);
            
            // Ограничиваем позицию границами аудио
            newSample = qBound(qint64(0), newSample, qint64(audioData[0].size() - 1));
            
            // Ограничиваем позицию соседними метками, чтобы метки не перелезали друг через друга
            if (markers.size() > 1) {
                // Сохраняем текущую позицию перетаскиваемой метки
                qint64 currentMarkerPos = markers[draggingMarkerIndex].position;
                
                // Улучшенный поиск соседних меток
                // Используем текущую позицию для определения направления движения
                // (переменные могут использоваться в будущем для оптимизации)
                bool movingRight = (newSample > currentMarkerPos);
                bool movingLeft = (newSample < currentMarkerPos);
                Q_UNUSED(movingRight);
                Q_UNUSED(movingLeft);
                
                // Ищем предыдущую метку (строго меньше текущей позиции)
                qint64 prevMarkerPos = -1;
                int prevMarkerIndex = -1;
                Q_UNUSED(prevMarkerIndex); // Индекс предыдущей метки (может использоваться в будущем)
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        qint64 markerPos = markers[j].position;
                        // Ищем метку слева от текущей позиции (строго меньше)
                        if (markerPos < currentMarkerPos) {
                            if (prevMarkerPos < 0 || markerPos > prevMarkerPos) {
                                prevMarkerPos = markerPos;
                                prevMarkerIndex = j;
                            }
                        }
                    }
                }
                
                // Ищем следующую метку (строго больше текущей позиции)
                qint64 nextMarkerPos = -1;
                int nextMarkerIndex = -1;
                Q_UNUSED(nextMarkerIndex); // Индекс следующей метки (может использоваться в будущем)
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        qint64 markerPos = markers[j].position;
                        // Ищем метку справа от текущей позиции (строго больше)
                        if (markerPos > currentMarkerPos) {
                            if (nextMarkerPos < 0 || markerPos < nextMarkerPos) {
                                nextMarkerPos = markerPos;
                                nextMarkerIndex = j;
                            }
                        }
                    }
                }
                
                // Строго ограничиваем позицию соседними метками
                // Минимальный отступ - 1 сэмпл, чтобы метки не совпадали
                qint64 minAllowedPos = (prevMarkerPos >= 0) ? (prevMarkerPos + 1) : qint64(0);
                qint64 maxAllowedPos = (nextMarkerPos >= 0) ? (nextMarkerPos - 1) : qint64(audioData[0].size() - 1);
                
                // Дополнительная проверка: если метки слишком близко, не позволяем перемещение
                if (prevMarkerPos >= 0 && nextMarkerPos >= 0) {
                    qint64 gapSize = nextMarkerPos - prevMarkerPos;
                    if (gapSize <= 2) {
                        // Нет места между метками (меньше 2 сэмплов) - оставляем на текущей позиции
                        newSample = currentMarkerPos;
                    } else {
                        // Ограничиваем строго между метками
                        newSample = qBound(minAllowedPos, newSample, maxAllowedPos);
                    }
                } else {
                    // Ограничиваем границами аудио или соседними метками
                    newSample = qBound(minAllowedPos, newSample, maxAllowedPos);
                }
                
                // Финальная проверка: убеждаемся, что новая позиция не совпадает с другими метками
                // Проверяем все метки, включая те, что могут быть созданы позже
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        qint64 markerPos = markers[j].position;
                        // Проверяем, не пересекается ли новая позиция с другой меткой
                        if (markerPos == newSample) {
                            // Позиция занята другой меткой - оставляем на текущей позиции
                            newSample = currentMarkerPos;
                            break;
                        }
                    }
                }
                
                // Финальная проверка валидности границ аудио
                newSample = qBound(qint64(0), newSample, qint64(audioData[0].size() - 1));
            }
            
            qDebug() << "Dragging marker" << draggingMarkerIndex << "to sample:" << newSample;
            
            // Сохраняем уникальные характеристики метки перед возможным созданием новых меток
            // Это необходимо для корректного обновления индекса после вставки новых меток
            qint64 savedMarkerOriginalPos = -1;
            bool savedIsEndMarker = false;
            bool savedIsFixed = false;
            qint64 savedDragStartSample = -1;
            if (draggingMarkerIndex >= 0 && draggingMarkerIndex < markers.size()) {
                savedMarkerOriginalPos = markers[draggingMarkerIndex].originalPosition;
                savedIsEndMarker = markers[draggingMarkerIndex].isEndMarker;
                savedIsFixed = markers[draggingMarkerIndex].isFixed;
                savedDragStartSample = markers[draggingMarkerIndex].dragStartSample;
            }
            
            // Сохраняем оригинальную позицию до изменения
            qint64 originalPos = (savedMarkerOriginalPos >= 0) ? savedMarkerOriginalPos : markers[draggingMarkerIndex].originalPosition;
            
            // Проверяем, смещена ли метка от исходной позиции
            bool isMoved = (newSample != originalPos);
            
            // Проверяем, есть ли метки слева от НОВОЙ позиции (с меньшей позицией), исключая неподвижные
            bool hasLeftMarker = false;
            for (int j = 0; j < markers.size(); ++j) {
                if (j != draggingMarkerIndex && markers[j].position < newSample && !markers[j].isFixed) {
                    hasLeftMarker = true;
                    break;
                }
            }
            
            // Если нет метки слева и текущая метка смещена, создаем метку в начале
            if (!hasLeftMarker && isMoved) {
                // Проверяем, что действительно нет метки в начале (позиция 0)
                bool foundStart = false;
                for (int j = 0; j < markers.size(); ++j) {
                    if (markers[j].position == 0) {
                        foundStart = true;
                        break;
                    }
                }
                if (!foundStart) {
                    // Создаем неподвижную метку в начале
                    markers.insert(0, Marker(0, true)); // true = неподвижная
                    draggingMarkerIndex++; // Обновляем индекс перетаскиваемой метки (вставка в начало)
                    qDebug() << "Created fixed start marker at position 0, draggingMarkerIndex now:" << draggingMarkerIndex;
                    
                    // После создания новой метки обновляем индекс перетаскиваемой метки
                    // Ищем метку по сохраненным характеристикам
                    if (draggingMarkerIndex >= 0 && draggingMarkerIndex < markers.size()) {
                        // Проверяем, что это все еще та же метка
                        if (markers[draggingMarkerIndex].originalPosition != savedMarkerOriginalPos ||
                            markers[draggingMarkerIndex].isEndMarker != savedIsEndMarker ||
                            markers[draggingMarkerIndex].isFixed != savedIsFixed ||
                            markers[draggingMarkerIndex].dragStartSample != savedDragStartSample) {
                            // Индекс изменился - ищем метку по характеристикам
                            for (int j = 0; j < markers.size(); ++j) {
                                if (markers[j].originalPosition == savedMarkerOriginalPos &&
                                    markers[j].isEndMarker == savedIsEndMarker &&
                                    markers[j].isFixed == savedIsFixed &&
                                    markers[j].dragStartSample == savedDragStartSample) {
                                    draggingMarkerIndex = j;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            
            // Проверяем, является ли перетаскиваемая метка конечной меткой
            bool isEndMarker = markers[draggingMarkerIndex].isEndMarker;
            qint64 endPosition = audioData[0].size() - 1;
            
            // Если это конечная метка, НЕ создаем новые метки справа от неё
            // Конечная метка - это особая метка, которая обозначает конец таймлайна
            // Она может перемещаться, но всегда остается конечной меткой и не позволяет создавать метки справа
            if (isEndMarker) {
                qDebug() << "End marker is being dragged, not creating new markers to the right";
                // Конечная метка не должна создавать новые метки справа от себя
            } else {
                // Для обычных меток проверяем, нужно ли создать конечную метку
                // Проверяем, есть ли справа обычные метки (не конечная) или конечная метка
                bool hasRightRegularMarker = false;
                bool hasAnyEndMarker = false;
                
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        if (markers[j].isEndMarker) {
                            hasAnyEndMarker = true; // Конечная метка уже существует
                            // Если конечная метка справа от новой позиции, не создаем новую конечную метку
                            if (markers[j].position > newSample) {
                                hasRightRegularMarker = true; // Блокируем создание, если конечная метка справа
                            }
                        } else if (markers[j].position > newSample && !markers[j].isFixed) {
                            hasRightRegularMarker = true; // Есть обычная метка справа
                        }
                    }
                }
                
                // Проверяем, сдвинута ли метка влево от исходной позиции
                bool isMovedLeft = (newSample < originalPos);
                
                // Создаем конечную метку только если:
                // 1. Нет обычных меток справа от новой позиции
                // 2. Нет конечной метки справа от новой позиции
                // 3. Метка смещена и сдвинута вправо (не влево)
                // 4. Нет конечной метки вообще (разрешаем создавать метки между предпоследней и конечной)
                if (!hasRightRegularMarker && isMoved && !isMovedLeft && !hasAnyEndMarker) {
                    // Проверяем, что действительно нет метки в конце
                    bool foundEnd = false;
                    for (int j = 0; j < markers.size(); ++j) {
                        if (markers[j].position == endPosition || markers[j].isEndMarker) {
                            foundEnd = true;
                            break;
                        }
                    }
                    if (!foundEnd) {
                        // Создаем конечную метку с флагом isEndMarker = true (можно двигать, но это особая метка)
                        markers.append(Marker(endPosition, false, true));
                        qDebug() << "Created end marker at position" << endPosition;
                        // Индекс draggingMarkerIndex не меняется при добавлении в конец
                    }
                }
            }
            
            // Финальная проверка перед обновлением позиции
            // Убеждаемся, что новая позиция не нарушает ограничения
            bool canMove = true;
            qint64 currentMarkerPos = (draggingMarkerIndex >= 0 && draggingMarkerIndex < markers.size()) 
                ? markers[draggingMarkerIndex].position : newSample;
            
            // Проверяем, что новая позиция не совпадает с другими метками
            for (int j = 0; j < markers.size(); ++j) {
                if (j != draggingMarkerIndex) {
                    if (markers[j].position == newSample) {
                        canMove = false;
                        qDebug() << "Cannot move marker: position" << newSample << "is occupied by marker" << j;
                        break;
                    }
                }
            }
            
            // Если перемещение невозможно, оставляем на текущей позиции
            if (!canMove) {
                newSample = currentMarkerPos;
            }
            
            // Теперь обновляем позицию метки после всех проверок и возможных вставок
            // Убеждаемся, что индекс все еще валиден
            if (draggingMarkerIndex >= 0 && draggingMarkerIndex < markers.size()) {
                // Используем сохраненные характеристики метки для надежного поиска после сортировки
                // Определяем финальную позицию метки
                qint64 finalPosition = newSample;
                if (savedIsEndMarker && newSample >= endPosition) {
                    finalPosition = endPosition;
                    markers[draggingMarkerIndex].position = endPosition;
                    markers[draggingMarkerIndex].originalPosition = endPosition; // Обновляем исходную позицию
                    qDebug() << "End marker moved to end position:" << endPosition;
                } else {
                    markers[draggingMarkerIndex].position = newSample;
                }
                
                // Сортируем метки по позиции, чтобы они всегда были в порядке
                std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
                    return a.position < b.position;
                });
                
                // Находим индекс перемещенной метки после сортировки
                // Используем комбинацию характеристик для точного совпадения
                draggingMarkerIndex = -1;
                for (int j = 0; j < markers.size(); ++j) {
                    // Ищем метку по всем характеристикам для точного совпадения
                    if (markers[j].originalPosition == savedMarkerOriginalPos &&
                        markers[j].isEndMarker == savedIsEndMarker &&
                        markers[j].isFixed == savedIsFixed &&
                        markers[j].dragStartSample == savedDragStartSample) {
                        draggingMarkerIndex = j;
                        break;
                    }
                }
                
                // Если не нашли по всем признакам, ищем по позиции и dragStartSample
                if (draggingMarkerIndex < 0) {
                    for (int j = 0; j < markers.size(); ++j) {
                        if (markers[j].position == finalPosition &&
                            markers[j].dragStartSample == savedDragStartSample) {
                            draggingMarkerIndex = j;
                            break;
                        }
                    }
                }
                
                // Финальная проверка валидности
                if (draggingMarkerIndex < 0 || draggingMarkerIndex >= markers.size()) {
                    qDebug() << "ERROR: Could not find dragged marker after sort, resetting drag";
                    draggingMarkerIndex = -1;
                }
                
                // Конечная метка сохраняет свой флаг isEndMarker при любом перемещении
            } else {
                qDebug() << "ERROR: draggingMarkerIndex" << draggingMarkerIndex << "is invalid, markers.size():" << markers.size();
                draggingMarkerIndex = -1; // Сбрасываем, если индекс невалиден
            }
            
            update();
        }
        return;
    }
    
    // Проверяем, наведена ли мышь на метку
    if (!(event->buttons() & Qt::LeftButton) && !(event->buttons() & Qt::RightButton)) {
        int markerIndex = getMarkerIndexAt(event->pos());
        if (markerIndex >= 0) {
            setCursor(Qt::SizeHorCursor);
            return;
        }
    }
    
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - lastMousePos;
        
        // Определяем, перетаскиваем ли мы (движение больше определенного порога)
        if (!isDragging && delta.manhattanLength() > 5) {
            isDragging = true;
        }
        
        if (isDragging) {
            // Если перетаскиваем с зажатой кнопкой мыши, обновляем позицию воспроизведения
            if (!audioData.isEmpty()) {
                // Используем ту же логику, что и в drawPlaybackCursor
                float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
                int visibleSamples = int(width() * samplesPerPixel);
                int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
                int startSample = int(horizontalOffset * maxStartSample);
                
                // Вычисляем позицию клика в сэмплах
                qint64 clickSample = startSample + qint64(event->pos().x() * samplesPerPixel);
                clickSample = qBound(qint64(0), clickSample, qint64(audioData[0].size() - 1));
                
                qint64 newPosition = (clickSample * 1000) / sampleRate;
                playbackPosition = newPosition;
                
                emit positionChanged(newPosition);
                update();
            }
        } else {
            // Обычное перетаскивание для прокрутки
            adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        }
        
        lastMousePos = event->pos();
    } else if (event->buttons() & Qt::RightButton && isRightMousePanning) {
        // Панорамирование правой кнопкой мыши
        QPoint delta = event->pos() - lastMousePos;
        adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        lastMousePos = event->pos();
    } else {
        // При наведении мыши без нажатых кнопок показываем курсор "рука"
        if (!isRightMousePanning && draggingMarkerIndex < 0) {
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Завершаем перетаскивание метки
        if (draggingMarkerIndex >= 0) {
            markers[draggingMarkerIndex].isDragging = false;
            draggingMarkerIndex = -1;
            setCursor(Qt::ArrowCursor);
            update();
            return;
        }
        
        isDragging = false;
    } else if (event->button() == Qt::RightButton) {
        isRightMousePanning = false;
        setCursor(Qt::ArrowCursor);
    }
}



void WaveformView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Left:
            adjustHorizontalOffset(-scrollStep);
            break;
        case Qt::Key_Right:
            adjustHorizontalOffset(scrollStep);
            break;
        case Qt::Key_Up:
            setZoomLevel(zoomLevel * zoomStep);
            break;
        case Qt::Key_Down:
            setZoomLevel(zoomLevel / zoomStep);
            break;
        case Qt::Key_M:
            // Добавляем метку в текущей позиции воспроизведения
            if (!audioData.isEmpty()) {
                qint64 playbackPos = playbackPosition; // playbackPosition уже в миллисекундах
                qint64 samplePos = (playbackPos * sampleRate) / 1000;
                qDebug() << "WaveformView: Key M pressed, adding marker at samplePos:" << samplePos;
                addMarker(samplePos);
            }
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

void WaveformView::adjustHorizontalOffset(float delta)
{
    // Применяем смещение с ограничением от 0 до 1
    horizontalOffset = qBound(0.0f, horizontalOffset + delta, 1.0f);
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::adjustZoomLevel(float delta)
{
    if (audioData.isEmpty()) {
        return;
    }
    
    float oldZoom = zoomLevel;
    float newZoom = oldZoom;
    
    if (delta > 0) {
        newZoom *= zoomStep;
    } else {
        newZoom /= zoomStep;
    }
    
    // Ограничиваем масштаб
    newZoom = qBound(float(minZoom), newZoom, float(maxZoom));
    
    if (qFuzzyCompare(newZoom, oldZoom)) {
        return;
    }
    
    // Масштабируем относительно центра экрана
    QPoint centerPos = QPoint(width() / 2, height() / 2);
    
    // Вычисляем текущую позицию в сэмплах под центром экрана
    float samplesPerPixel = float(audioData[0].size()) / (width() * oldZoom);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    qint64 centerSample = startSample + qint64(centerPos.x() * samplesPerPixel);
    
    // Обновляем масштаб
    zoomLevel = newZoom;
    
    // Вычисляем новую позицию, чтобы центр экрана остался над тем же сэмплом
    float newSamplesPerPixel = float(audioData[0].size()) / (width() * newZoom);
    int newVisibleSamples = int(width() * newSamplesPerPixel);
    int newMaxStartSample = qMax(0, audioData[0].size() - newVisibleSamples);
    
    // Вычисляем новое смещение
    float newOffset = 0.0f;
    if (newMaxStartSample > 0) {
        newOffset = float(centerSample - centerPos.x() * newSamplesPerPixel) / newMaxStartSample;
        newOffset = qBound(0.0f, newOffset, 1.0f);
    }
    
    horizontalOffset = newOffset;
    
    // Эмитируем сигналы для обновления скроллбара
    emit zoomChanged(zoomLevel);
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::setHorizontalOffset(float offset)
{
    // Ограничиваем смещение в пределах от 0 до 1
    horizontalOffset = qBound(0.0f, offset, 1.0f);
    
    // При изменении смещения обновляем отображение
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::setVerticalOffset(float offset)
{
    // Ограничиваем смещение в пределах от 0 до 1
    verticalOffset = qBound(0.0f, offset, 1.0f);
    
    // При изменении смещения обновляем отображение
    update();
}

void WaveformView::setTimeDisplayMode(bool showTime)
{
    showTimeDisplay = showTime;
}

void WaveformView::setBarsDisplayMode(bool showBars)
{
    showBarsDisplay = showBars;
}

void WaveformView::setBeatsPerBar(int beats)
{
    beatsPerBar = qBound(1, beats, 32);
    update();
}

void WaveformView::setShowBeatDeviations(bool show)
{
    showBeatDeviations = show;
    update();
}

void WaveformView::setShowBeatWaveform(bool show)
{
    showBeatWaveform = show;
    update();
}

QString WaveformView::getBarText(float beatPosition) const
{
    const int bpb = qMax(1, beatsPerBar);
    int bar = int(beatPosition / bpb) + 1;
    int beat = int(beatPosition) % bpb + 1;
    float subBeat = (beatPosition - float(int(beatPosition))) * float(bpb);
    
    return QString("%1.%2.%3")
        .arg(bar)
        .arg(beat)
        .arg(int(subBeat + 1));
}

void WaveformView::setColorScheme(const QString& scheme)
{
    colors.setColorScheme(scheme);
    update();
}

void WaveformView::setLoopStart(qint64 position)
{
    loopStartPosition = position;
    update();
}

void WaveformView::setLoopEnd(qint64 position)
{
    loopEndPosition = position;
    update();
}

void WaveformView::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    // Устанавливаем курсор "рука" при входе в виджет
    if (!isRightMousePanning) {
        setCursor(Qt::OpenHandCursor);
    }
}

void WaveformView::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    // Возвращаем обычный курсор при выходе из виджета
    if (!isRightMousePanning) {
        setCursor(Qt::ArrowCursor);
    }
}

void WaveformView::drawLoopMarkers(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty() || (loopStartPosition <= 0 && loopEndPosition <= 0)) {
        return;
    }
    
    // Используем ту же логику, что и в drawWaveform
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Настраиваем перо для маркеров цикла
    QPen loopPen;
    loopPen.setWidth(2);
    loopPen.setStyle(Qt::DashLine);

    // Рисуем маркер начала цикла
    if (loopStartPosition > 0) {
        qint64 startSamplePos = (loopStartPosition * sampleRate) / 1000;
        
        // Проверяем, попадает ли маркер в видимую область
        if (startSamplePos >= startSample && startSamplePos < startSample + visibleSamples) {
            loopPen.setColor(QColor(0, 255, 0, 180)); // Зелёный для начала
            painter.setPen(loopPen);
            
            // Вычисляем позицию маркера в пикселях
            float x = (startSamplePos - startSample) / samplesPerPixel;
            
            if (x >= 0 && x < rect.width()) {
                painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));
                
                // Рисуем метку "A"
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(QPointF(rect.x() + x + 5, rect.top() + 20), "A");
            }
        }
    }

    // Рисуем маркер конца цикла
    if (loopEndPosition > 0) {
        qint64 endSamplePos = (loopEndPosition * sampleRate) / 1000;
        
        // Проверяем, попадает ли маркер в видимую область
        if (endSamplePos >= startSample && endSamplePos < startSample + visibleSamples) {
            loopPen.setColor(QColor(255, 0, 0, 180)); // Красный для конца
            painter.setPen(loopPen);
            
            // Вычисляем позицию маркера в пикселях
            float x = (endSamplePos - startSample) / samplesPerPixel;
            
            if (x >= 0 && x < rect.width()) {
                painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));
                
                // Рисуем метку "B"
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(QPointF(rect.x() + x + 5, rect.top() + 20), "B");
            }
        }
    }

    // Если установлены оба маркера, заливаем область между ними
    if (loopStartPosition > 0 && loopEndPosition > 0) {
        qint64 startSamplePos = (loopStartPosition * sampleRate) / 1000;
        qint64 endSamplePos = (loopEndPosition * sampleRate) / 1000;
        
        // Проверяем, попадает ли хотя бы часть области цикла в видимую область
        if (endSamplePos >= startSample && startSamplePos < startSample + visibleSamples) {
            // Вычисляем позиции в пикселях
            float x1 = qMax(0.0f, (startSamplePos - startSample) / samplesPerPixel);
            float x2 = qMin(float(rect.width()), (endSamplePos - startSample) / samplesPerPixel);
            
            if (x2 > x1) {
                QColor fillColor(128, 128, 255, 40); // Полупрозрачный синий
                painter.fillRect(QRectF(rect.x() + x1, rect.top(), x2 - x1, rect.height()), fillColor);
            }
        }
    }
}

// Методы для визуализации ударных

// Новые методы для улучшенной визуализации ударных (временно отключены)
/*
void WaveformView::setBeatVisualizationSettings(const BeatVisualizer::VisualizationSettings& settings)
{
    beatVisualizationSettings = settings;
    update();
}

void WaveformView::analyzeBeats()
{
    // Временно отключено для исправления сборки
    if (audioData.isEmpty()) {
        return;
    }
    
    // Запускаем анализ ударных с текущими настройками
    beatAnalysis = BeatVisualizer::analyzeBeats(audioData, sampleRate, beatVisualizationSettings);
    
    // Обновляем отображение
    update();
}
*/

// Методы для работы с метками
void WaveformView::addMarker(qint64 position)
{
    if (audioData.isEmpty()) {
        qDebug() << "addMarker: audioData is empty";
        return;
    }
    
    // Ограничиваем позицию границами аудио
    position = qBound(qint64(0), position, qint64(audioData[0].size() - 1));
    
    // Проверяем, нет ли уже метки в этой позиции
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].position == position) {
            qDebug() << "addMarker: marker already exists at position:" << position;
            return; // Метка уже существует, не создаём дубликат
        }
    }
    
    // Проверяем, не пытаемся ли создать метку справа от конечной метки
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].isEndMarker && position > markers[i].position) {
            qDebug() << "addMarker: cannot create marker to the right of end marker at position:" << markers[i].position;
            return; // Нельзя создавать метки справа от конечной метки
        }
    }
    
    qDebug() << "addMarker: adding marker at position:" << position << "audioData size:" << audioData[0].size();
    
    // Если это первая метка, автоматически создаем начальную и конечную метки
    bool isFirstMarker = markers.isEmpty();
    
    if (isFirstMarker) {
        qint64 endPosition = audioData[0].size() - 1;
        
        // Создаем начальную метку (неподвижную, в позиции 0)
        markers.append(Marker(0, true)); // true = неподвижная
        
        // Создаем новую метку в указанной позиции
        Marker newMarker(position);
        markers.append(newMarker);
        
        // Создаем конечную метку (можно двигать, но это особая метка)
        markers.append(Marker(endPosition, false, true)); // false = можно двигать, true = isEndMarker
        
        qDebug() << "addMarker: created start marker at 0, user marker at" << position << "and end marker at" << endPosition;
    } else {
        // Находим соседние метки для пересчета коэффициентов
        const Marker* prevMarker = nullptr;
        const Marker* nextMarker = nullptr;
        qint64 maxPrevPos = -1;
        qint64 minNextPos = -1;
        
        for (int j = 0; j < markers.size(); ++j) {
            qint64 markerPos = markers[j].position;
            if (markerPos < position) {
                if (prevMarker == nullptr || markerPos > maxPrevPos) {
                    prevMarker = &markers[j];
                    maxPrevPos = markerPos;
                }
            } else if (markerPos > position) {
                if (nextMarker == nullptr || markerPos < minNextPos) {
                    nextMarker = &markers[j];
                    minNextPos = markerPos;
                }
            }
        }
        
        // Создаем новую метку
        Marker newMarker(position);
        
        // Если есть соседние метки, которые уже были сдвинуты, пересчитываем originalPosition
        // чтобы новая метка корректно отображала коэффициенты сжатия/растяжения
        if (prevMarker != nullptr && nextMarker != nullptr) {
            // Вычисляем пропорциональное положение новой метки между соседними метками
            // на основе их текущих позиций (с учетом сжатия/растяжения)
            qint64 currentLeftDistance = position - prevMarker->position;
            qint64 totalCurrentDistance = nextMarker->position - prevMarker->position;
            
            if (totalCurrentDistance > 0) {
                // Вычисляем пропорцию, которую занимает новая метка в текущем диапазоне
                float proportion = float(currentLeftDistance) / float(totalCurrentDistance);
                
                // Вычисляем originalPosition на основе пропорции в исходном диапазоне
                qint64 originalLeftDistance = nextMarker->originalPosition - prevMarker->originalPosition;
                if (originalLeftDistance > 0) {
                    qint64 calculatedOriginalPos = prevMarker->originalPosition + qint64(originalLeftDistance * proportion);
                    newMarker.originalPosition = calculatedOriginalPos;
                    qDebug() << "addMarker: calculated originalPosition" << calculatedOriginalPos << "for new marker at" << position 
                             << "between markers at" << prevMarker->position << "and" << nextMarker->position;
                }
            }
        } else if (prevMarker != nullptr) {
            // Только предыдущая метка - используем её originalPosition как базу
            newMarker.originalPosition = prevMarker->originalPosition + (position - prevMarker->position);
        } else if (nextMarker != nullptr) {
            // Только следующая метка - используем её originalPosition как базу
            newMarker.originalPosition = nextMarker->originalPosition - (nextMarker->position - position);
        }
        
        markers.append(newMarker);
    }
    
    // Сортируем метки по позиции, чтобы они всегда были в порядке
    std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });
    
    qDebug() << "addMarker: total markers now:" << markers.size();
    update();
}

void WaveformView::removeMarker(int index)
{
    if (index >= 0 && index < markers.size()) {
        markers.removeAt(index);
        if (draggingMarkerIndex == index) {
            draggingMarkerIndex = -1;
        } else if (draggingMarkerIndex > index) {
            draggingMarkerIndex--;
        }
        update();
    }
}

int WaveformView::getMarkerIndexAt(const QPoint& pos) const
{
    if (audioData.isEmpty()) return -1;
    
    // Проверяем каждую метку
    // getMarkerDiamondRect сама вычисляет позицию метки на основе samplePos и rect()
    for (int i = 0; i < markers.size(); ++i) {
        QRectF diamondRect = getMarkerDiamondRect(markers[i].position, rect());
        // Увеличиваем область клика для удобства (добавляем отступ)
        QRectF expandedRect = diamondRect.adjusted(-5, -5, 5, 5);
        if (expandedRect.contains(pos)) {
            qDebug() << "Found marker" << i << "at pos:" << pos << "diamondRect:" << diamondRect;
            return i;
        }
    }
    
    return -1;
}

QRectF WaveformView::getMarkerDiamondRect(qint64 samplePos, const QRect& rect) const
{
    if (audioData.isEmpty()) return QRectF();
    
    // Вычисляем X позицию метки
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    float x = (samplePos - startSample) / samplesPerPixel;
    
    // Размер ромбика (обновлено для нового ромбика в центре)
    const float diamondSize = 10.0f;
    float centerY = rect.height() / 2.0f;
    
    return QRectF(x - diamondSize / 2, centerY - diamondSize / 2, diamondSize, diamondSize);
}

void WaveformView::drawMarkers(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty() || markers.isEmpty()) {
        return;
    }
    
    // Отладочный вывод (можно закомментировать после проверки)
    // qDebug() << "drawMarkers: drawing" << markers.size() << "markers, rect:" << rect.width() << "x" << rect.height();
    
    // Вычисляем позицию в сэмплах для видимой области
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    Q_UNUSED(visibleSamples); // Используется неявно через samplesPerPixel
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    for (int i = 0; i < markers.size(); ++i) {
        const Marker& marker = markers[i];
        
        // Вычисляем X позицию метки
        float x = (marker.position - startSample) / samplesPerPixel;
        
        // Проверяем, видна ли метка в видимой области
        if (x < -10 || x > rect.width() + 10) {
            // Метка вне видимой области, пропускаем
            continue;
        }
        
        // Отладочный вывод (можно закомментировать после проверки)
        // qDebug() << "Drawing marker" << i << "at x:" << x << "position:" << marker.position << "startSample:" << startSample;
        
        // Рисуем белую вертикальную линию (более толстую для видимости)
        painter.setPen(QPen(Qt::white, 2.0f));
        painter.drawLine(QPointF(x, 0), QPointF(x, rect.height()));
        
        // Рисуем ромбик в центре высоты виджета
        const float diamondSize = 10.0f; // Размер ромбика
        float centerY = rect.height() / 2.0f; // Центр по вертикали
        
        // Определяем цвета углов в зависимости от сжатия/растяжения участков между метками
        QColor leftColor = Qt::white;  // Левый уголок указывает на участок слева
        QColor rightColor = Qt::white; // Правый уголок указывает на участок справа
        
        // Находим предыдущую метку по позиции (не по индексу, на случай если метки не отсортированы)
        const Marker* prevMarker = nullptr;
        qint64 maxPrevPos = -1;
        for (int j = 0; j < markers.size(); ++j) {
            if (j != i && markers[j].position < marker.position) {
                if (prevMarker == nullptr || markers[j].position > maxPrevPos) {
                    prevMarker = &markers[j];
                    maxPrevPos = markers[j].position;
                }
            }
        }
        
        // Проверяем участок слева (между предыдущей и текущей меткой)
        if (prevMarker != nullptr) {
            qint64 originalLeftDistance = marker.originalPosition - prevMarker->originalPosition;
            qint64 currentLeftDistance = marker.position - prevMarker->position;
            
            if (originalLeftDistance > 0) {
                if (currentLeftDistance < originalLeftDistance) {
                    // Участок слева сжат - левый уголок синий
                    leftColor = Qt::blue;
                } else if (currentLeftDistance > originalLeftDistance) {
                    // Участок слева растянут - левый уголок красный
                    leftColor = Qt::red;
                }
            }
        }
        
        // Находим следующую метку по позиции (не по индексу, на случай если метки не отсортированы)
        const Marker* nextMarker = nullptr;
        qint64 minNextPos = -1;
        for (int j = 0; j < markers.size(); ++j) {
            if (j != i && markers[j].position > marker.position) {
                if (nextMarker == nullptr || markers[j].position < minNextPos) {
                    nextMarker = &markers[j];
                    minNextPos = markers[j].position;
                }
            }
        }
        
        // Проверяем участок справа (между текущей и следующей меткой)
        if (nextMarker != nullptr) {
            qint64 originalRightDistance = nextMarker->originalPosition - marker.originalPosition;
            qint64 currentRightDistance = nextMarker->position - marker.position;
            
            if (originalRightDistance > 0) {
                if (currentRightDistance < originalRightDistance) {
                    // Участок справа сжат - правый уголок синий
                    rightColor = Qt::blue;
                } else if (currentRightDistance > originalRightDistance) {
                    // Участок справа растянут - правый уголок красный
                    rightColor = Qt::red;
                }
            }
        }
        
        // Рисуем ромбик: четыре точки (верх, право, низ, лево)
        // Отраженный по горизонтали: левая часть - левый цвет, правая часть - правый цвет
        QPointF top(x, centerY - diamondSize / 2);      // Верх
        QPointF right(x + diamondSize / 2, centerY);   // Право
        QPointF bottom(x, centerY + diamondSize / 2);  // Низ
        QPointF left(x - diamondSize / 2, centerY);    // Лево
        
        // Рисуем левую половину ромбика (верх-лево-низ)
        painter.setPen(QPen(leftColor, 2.0f));
        painter.drawLine(top, left);
        painter.drawLine(left, bottom);
        
        // Рисуем правую половину ромбика (верх-право-низ)
        painter.setPen(QPen(rightColor, 2.0f));
        painter.drawLine(top, right);
        painter.drawLine(right, bottom);
        
        // Если это конечная метка (isEndMarker), показываем "Конец таймлайна" справа
        // Конечная метка может быть перемещена, но она всегда обозначает конец таймлайна
        if (marker.isEndMarker) {
            const float lineY = centerY; // По центру высоты
            const float textOffset = 15.0f; // Отступ текста от линии
            
            // Горизонтальная линия справа от метки
            const float lineLength = 30.0f;
            painter.setPen(QPen(Qt::white, 1.0f));
            painter.drawLine(QPointF(x + 5, lineY), QPointF(x + lineLength, lineY));
            
            // Текст "Конец таймлайна" справа от метки
            QString timelineEndText = "Конец таймлайна";
            QFontMetrics fm(painter.font());
            QRect textRect = fm.boundingRect(timelineEndText);
            painter.setPen(Qt::white);
            painter.drawText(QRectF(x + textOffset, lineY - textRect.height() - 5, 
                                   textRect.width(), textRect.height()), 
                            Qt::AlignLeft | Qt::AlignTop, timelineEndText);
        } else if (nextMarker != nullptr) {
            // Обычная логика для меток с следующей меткой справа
            // Используем найденную ранее nextMarker (по позиции, а не по индексу)
            
            // Вычисляем позицию следующей метки
            float nextX = (nextMarker->position - startSample) / samplesPerPixel;
            
            // Вычисляем коэффициент сжатия-растяжения между метками
            qint64 originalDistance = nextMarker->originalPosition - marker.originalPosition;
            qint64 currentDistance = nextMarker->position - marker.position;
            
            // Показываем линии только если есть изменение
            if (originalDistance > 0 && currentDistance != originalDistance) {
                float coefficient = float(currentDistance) / float(originalDistance);
                
                // Горизонтальные линии между метками
                const float lineY = centerY; // По центру высоты
                const float midX = (x + nextX) / 2.0f; // Середина между метками
                
                // Линия слева (от текущей метки до середины)
                painter.setPen(QPen(Qt::white, 1.0f));
                painter.drawLine(QPointF(x + 5, lineY), QPointF(midX - 20, lineY));
                
                // Линия справа (от середины до следующей метки)
                painter.drawLine(QPointF(midX + 20, lineY), QPointF(nextX - 5, lineY));
                
                // Коэффициент сверху посередине между метками
                QString coeffText = QString::number(coefficient, 'f', 3);
                QFontMetrics fm(painter.font());
                QRect textRect = fm.boundingRect(coeffText);
                painter.setPen(Qt::white);
                painter.drawText(QRectF(midX - textRect.width() / 2, lineY - textRect.height() - 5, 
                                       textRect.width(), textRect.height()), 
                                Qt::AlignCenter, coeffText);
            }
        }
    }
    
    painter.setRenderHint(QPainter::Antialiasing, false);
}

QVector<QVector<float>> WaveformView::applyTimeStretch(const QVector<Marker>& markers) const
{
    if (audioData.isEmpty() || markers.isEmpty()) {
        return audioData;
    }
    
    // Создаем копию меток и сортируем по originalPosition для корректной обработки
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(), [](const Marker& a, const Marker& b) {
        return a.originalPosition < b.originalPosition;
    });
    
    QVector<QVector<float>> result;
    result.reserve(audioData.size());
    
    // Инициализируем результат пустыми каналами
    for (int ch = 0; ch < audioData.size(); ++ch) {
        result.append(QVector<float>());
    }
    
    qint64 audioSize = audioData[0].size();
    
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
            for (int ch = 0; ch < audioData.size(); ++ch) {
                QVector<float> segment;
                segment.reserve(segmentEnd - segmentStart);
                for (qint64 j = segmentStart; j < segmentEnd; ++j) {
                    segment.append(audioData[ch][j]);
                }
                
                QVector<float> processedSegment = TimeStretchProcessor::processSegment(segment, stretchFactor);
                result[ch].append(processedSegment);
            }
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
            continue;
        }
        
        float stretchFactor = static_cast<float>(currentDistance) / static_cast<float>(originalDistance);
        
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
        for (int ch = 0; ch < audioData.size(); ++ch) {
            // Извлекаем сегмент
            QVector<float> segment;
            segment.reserve(segmentEnd - segmentStart);
            for (qint64 j = segmentStart; j < segmentEnd; ++j) {
                segment.append(audioData[ch][j]);
            }
            
            // Применяем сжатие-растяжение
            QVector<float> processedSegment = TimeStretchProcessor::processSegment(segment, stretchFactor);
            
            // Добавляем к результату
            result[ch].append(processedSegment);
        }
    }
    
    // Обрабатываем участок от последней метки до конца аудио
    if (!sortedMarkers.isEmpty()) {
        const Marker& lastMarker = sortedMarkers.last();
        qint64 segmentStart = lastMarker.originalPosition;
        qint64 segmentEnd = audioSize;
        
        if (segmentStart < segmentEnd) {
            // Вычисляем коэффициент для конечного участка
            qint64 originalLength = segmentEnd - segmentStart;
            qint64 targetLength = originalLength;
            
            if (lastMarker.isEndMarker) {
                // Конечная метка - вычисляем целевую длину на основе позиции метки
                // Вычисляем текущую длину выходного аудио (после обработки всех предыдущих сегментов)
                qint64 currentOutputSize = result.isEmpty() ? 0 : result[0].size();
                // Позиция конечной метки должна быть в конце выходного аудио
                qint64 targetOutputSize = lastMarker.position + 1; // +1 потому что position это индекс
                
                if (targetOutputSize > currentOutputSize) {
                    // Вычисляем, сколько нужно добавить
                    targetLength = targetOutputSize - currentOutputSize;
                } else {
                    // Если целевой размер меньше текущего, это некорректная ситуация
                    // Используем исходную длину
                    targetLength = originalLength;
                }
            } else {
                // Обычная метка - конечный участок обрабатывается без изменений (коэффициент 1.0)
                // или можно использовать коэффициент на основе перемещения метки
                // Для простоты используем коэффициент 1.0
                targetLength = originalLength;
            }
            
            float stretchFactor = (originalLength > 0) ? 
                static_cast<float>(targetLength) / static_cast<float>(originalLength) : 1.0f;
            
            for (int ch = 0; ch < audioData.size(); ++ch) {
                QVector<float> segment;
                segment.reserve(segmentEnd - segmentStart);
                for (qint64 j = segmentStart; j < segmentEnd; ++j) {
                    segment.append(audioData[ch][j]);
                }
                
                QVector<float> processedSegment = TimeStretchProcessor::processSegment(segment, stretchFactor);
                result[ch].append(processedSegment);
            }
        }
    }
    
    return result;
} 