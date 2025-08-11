#include "waveformview.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtCore/QTime>

// Определение статических констант
const QColor WaveformView::waveformColor = QColor(75, 0, 130);  // Индиго
const QColor WaveformView::beatLineColor = QColor(255, 255, 255, 128);  // Полупрозрачный белый
const QColor WaveformView::barLineColor = QColor(255, 255, 255, 200);   // Более яркий белый
const QColor WaveformView::cursorColor = QColor(0, 0, 0);  // Чёрный цвет для курсора
const QColor WaveformView::markerTextColor = QColor(200, 200, 200);  // Светло-серый для текста
const QColor WaveformView::markerBackgroundColor = QColor(60, 60, 60);  // Темно-серый для фона
const int WaveformView::minZoom = 1;
const int WaveformView::maxZoom = 1000;
const int WaveformView::markerHeight = 20;  // Высота области маркеров
const int WaveformView::markerSpacing = 60;  // Минимальное расстояние между маркерами в пикселях

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , bpm(120.0f)
    , zoomLevel(1.0f)
    , horizontalOffset(0.0f)
    , isDragging(false)
    , sampleRate(44100)
    , playbackPosition(0)
    , scrollStep(0.1f)    // 10% от ширины окна
    , zoomStep(1.2f)      // 20% изменение масштаба
    , showTimeDisplay(true)
    , showBarsDisplay(false)
    , firstBeatSample(0)
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
    audioChannels.clear();
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
        
        audioChannels.append(normalizedChannel);
    }

    // Сбрасываем масштаб и смещение
    zoomLevel = 1.0f;
    horizontalOffset = 0.0f;
    emit zoomChanged(zoomLevel);

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

void WaveformView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Фон
    painter.fillRect(rect(), QColor(43, 43, 43));  // Темно-серый фон как в Reaper
    
    // Область для маркеров тактов
    QRectF markerRect(0, 0, width(), markerHeight);
    drawBarMarkers(painter, markerRect);
    
    // Область для волны
    QRectF waveformRect(0, markerHeight, width(), height() - markerHeight);
    
    // Рисуем сетку и линии тактов
    drawGrid(painter);
    drawBeatLines(painter);
    
    if (audioChannels.isEmpty()) return;
    
    // Вычисляем высоту для каждого канала
    float channelHeight = (height() - markerHeight) / float(audioChannels.size());
    
    // Отрисовка каждого канала
    for (int i = 0; i < audioChannels.size(); ++i) {
        QRectF channelRect(0, markerHeight + i * channelHeight, width(), channelHeight);
        drawWaveform(painter, audioChannels[i], channelRect);
    }

    // Рисуем курсор воспроизведения
    drawPlaybackCursor(painter);
}

void WaveformView::drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect)
{
    if (samples.isEmpty()) return;

    painter.setPen(waveformColor);
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(samples.size()) / (rect.width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, samples.size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
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
        
        // Рисуем вертикальную линию от минимума до максимума
        QPointF top = sampleToPoint(x, maxValue, rect);
        QPointF bottom = sampleToPoint(x, minValue, rect);
        painter.drawLine(top, bottom);
    }
}

void WaveformView::drawGrid(QPainter& painter)
{
    // Горизонтальные линии для разделения каналов
    painter.setPen(QPen(QColor(70, 70, 70)));  // Немного светлее фона
    float channelHeight = height() / float(qMax(1, audioChannels.size()));
    for (int i = 1; i < audioChannels.size(); ++i) {
        float y = i * channelHeight;
        painter.drawLine(QPointF(0, y), QPointF(width(), y));
    }
}

void WaveformView::drawBeatLines(QPainter& painter)
{
    if (audioChannels.isEmpty()) return;
    
    // Вычисляем длительность одного такта в сэмплах
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    float samplesPerBar = samplesPerBeat * 4;  // 4 удара в такте
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioChannels[0].size()) / (width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioChannels[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Сдвиг сетки по первому detected beat
    qint64 gridOffset = firstBeatSample;

    // Находим первый бит и такт перед видимой областью, учитывая gridOffset
    float firstBeat = floor((startSample - gridOffset) / samplesPerBeat);
    float firstBar = floor((startSample - gridOffset) / samplesPerBar);
    
    // Рисуем линии тактов
    painter.setPen(QPen(barLineColor));
    for (float bar = firstBar; ; bar++) {
        float x = ((bar * samplesPerBar) + gridOffset - startSample) / samplesPerPixel;
        if (x >= width()) break;
        if (x >= 0) {
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
    }
    
    // Рисуем линии битов
    painter.setPen(QPen(beatLineColor));
    for (float beat = firstBeat; ; beat++) {
        float x = ((beat * samplesPerBeat) + gridOffset - startSample) / samplesPerPixel;
        if (x >= width()) break;
        if (x >= 0) {
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
    }

    // Отрисуем явно обнаруженные биты (если есть)
    if (!detectedBeatSamples.isEmpty()) {
        painter.setPen(QPen(QColor(0, 200, 255, 200))); // Голубые маркеры обнаруженных битов
        for (qint64 beatSample : detectedBeatSamples) {
            float x = float(beatSample - startSample) / samplesPerPixel;
            if (x < 0) continue;
            if (x >= width()) break;
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
    }
}

void WaveformView::drawPlaybackCursor(QPainter& painter)
{
    if (audioChannels.isEmpty()) return;

    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioChannels[0].size()) / (width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioChannels[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Вычисляем позицию курсора относительно видимой области
    float cursorX = float(playbackPosition - startSample) / samplesPerPixel;

    if (cursorX >= 0 && cursorX < width()) {
        painter.setPen(QPen(cursorColor, 2));  // Толщина линии 2 пикселя
        painter.drawLine(QPointF(cursorX, 0), QPointF(cursorX, height()));
    }
}

void WaveformView::drawBarMarkers(QPainter& painter, const QRectF& rect)
{
    if (audioChannels.isEmpty()) return;
    
    painter.setPen(markerTextColor);
    painter.setFont(QFont("Arial", 8));
    
    // Вычисляем длительность одного такта в сэмплах
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    float samplesPerBar = samplesPerBeat * 4;  // 4 удара в такте
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioChannels[0].size()) / (width() * zoomLevel);
    
    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioChannels[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // Сдвиг сетки
    qint64 gridOffset = firstBeatSample;

    // Находим первый такт перед видимой областью
    float firstBar = floor((startSample - gridOffset) / samplesPerBar);
    
    // Рисуем маркеры тактов
    for (float bar = firstBar; ; bar++) {
        float x = ((bar * samplesPerBar) + gridOffset - startSample) / samplesPerPixel;
        if (x >= width()) break;
        if (x >= 0) {
            // Рисуем фон маркера
            QRectF markerRect(x - markerSpacing/2, rect.top(), markerSpacing, rect.height());
            painter.fillRect(markerRect, markerBackgroundColor);
            
            // Рисуем текст маркера
            QString barText = QString::number(int(bar) + 1);
            QRectF textRect = painter.fontMetrics().boundingRect(barText);
            painter.drawText(QPointF(
                x - textRect.width()/2,
                rect.top() + rect.height() - 4
            ), barText);
            
            // Рисуем вертикальную линию
            painter.setPen(barLineColor);
            painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, height()));
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
        float beatsFromStart = float(position - firstBeatSample) / samplesPerBeat;
        int bar = int(beatsFromStart / 4) + 1;  // Такты начинаются с 1
        int beat = int(beatsFromStart) % 4 + 1;  // Доли начинаются с 1
        float subBeat = (beatsFromStart - float(int(beatsFromStart))) * 4.0f; // Доли такта в четвертях
        
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
    playbackPosition = position;
    update();
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
    float delta = event->angleDelta().y() / 120.f;
    float newZoom = zoomLevel;
    if (delta > 0) {
        newZoom *= zoomStep;
    } else {
        newZoom /= zoomStep;
    }
    setZoomLevel(newZoom);
    event->accept();
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();

        // Устанавливаем позицию воспроизведения по клику
        if (!audioChannels.isEmpty()) {
            float samplesPerPixel = float(audioChannels[0].size()) / (width() * zoomLevel);
            float clickX = event->pos().x() + (horizontalOffset * width());
            qint64 newPosition = qint64(clickX * samplesPerPixel);
            playbackPosition = newPosition;
            
            // Показываем всплывающую подсказку
            QString positionText = getPositionText(newPosition);
            if (!positionText.isEmpty()) {
                QToolTip::showText(event->globalPosition().toPoint(), positionText, this);
            }
            
            emit positionChanged(newPosition);
            update();
        }
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (isDragging) {
        QPoint delta = event->pos() - lastMousePos;
        adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        lastMousePos = event->pos();
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
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
        default:
            QWidget::keyPressEvent(event);
    }
}

void WaveformView::adjustHorizontalOffset(float delta)
{
    // Применяем смещение с ограничением от 0 до 1
    horizontalOffset = qBound(0.0f, horizontalOffset + delta, 1.0f);
    update();
}

void WaveformView::adjustZoomLevel(float delta)
{
    float newZoom = zoomLevel;
    if (delta > 0) {
        newZoom *= zoomStep;
    } else {
        newZoom /= zoomStep;
    }
    setZoomLevel(newZoom);
}

void WaveformView::setHorizontalOffset(float offset)
{
    // Ограничиваем смещение в пределах от 0 до 1
    horizontalOffset = qBound(0.0f, offset, 1.0f);
    
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

QString WaveformView::getBarText(float beatPosition) const
{
    int bar = int(beatPosition / 4) + 1;
    int beat = int(beatPosition) % 4 + 1;
    float subBeat = (beatPosition - float(int(beatPosition))) * 4.0f;
    
    return QString("%1.%2.%3")
        .arg(bar)
        .arg(beat)
        .arg(int(subBeat + 1));
}

void WaveformView::setBeatGrid(float newBpm, qint64 newFirstBeatSample)
{
    bpm = newBpm;
    firstBeatSample = qMax<qint64>(0, newFirstBeatSample);
    update();
}

void WaveformView::setDetectedBeats(const QVector<qint64>& beatSamples)
{
    detectedBeatSamples = beatSamples;
    update();
} 