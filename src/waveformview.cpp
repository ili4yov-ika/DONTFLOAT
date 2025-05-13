#include "waveformview.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QPainter>

// Определение статических констант
const QColor WaveformView::waveformColor = QColor(0, 255, 0);  // Зеленый цвет как в Reaper
const QColor WaveformView::beatLineColor = QColor(255, 255, 255, 128);  // Полупрозрачный белый
const QColor WaveformView::barLineColor = QColor(255, 255, 255, 200);   // Более яркий белый
const int WaveformView::minZoom = 1;
const int WaveformView::maxZoom = 1000;

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , bpm(120.0f)
    , zoomLevel(1.0f)
    , horizontalOffset(0.0f)
    , isDragging(false)
    , sampleRate(44100)
{
    setMinimumHeight(100);
    setMouseTracking(true);
}

void WaveformView::setAudioData(const QVector<QVector<float>>& channels)
{
    audioChannels = channels;
    update();
}

void WaveformView::setBPM(float newBpm)
{
    bpm = newBpm;
    update();
}

void WaveformView::setZoomLevel(float zoom)
{
    zoomLevel = qBound(float(minZoom), zoom, float(maxZoom));
    update();
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
    
    // Рисуем сетку и линии тактов
    drawGrid(painter);
    drawBeatLines(painter);
    
    if (audioChannels.isEmpty()) return;
    
    // Вычисляем высоту для каждого канала
    float channelHeight = height() / float(audioChannels.size());
    
    // Отрисовка каждого канала
    for (int i = 0; i < audioChannels.size(); ++i) {
        QRectF channelRect(0, i * channelHeight, width(), channelHeight);
        drawWaveform(painter, audioChannels[i], channelRect);
    }
}

void WaveformView::drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect)
{
    if (samples.isEmpty()) return;

    painter.setPen(waveformColor);
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = qMax(1.0f, float(samples.size()) / (rect.width() * zoomLevel));
    
    float xOffset = horizontalOffset * rect.width();
    
    for (int x = 0; x < rect.width(); ++x) {
        int startSample = int((x + xOffset) * samplesPerPixel);
        int endSample = int((x + 1 + xOffset) * samplesPerPixel);
        
        if (startSample >= samples.size()) break;
        
        // Находим минимальное и максимальное значение для текущего пикселя
        float minValue = 0, maxValue = 0;
        for (int s = startSample; s < qMin(endSample, samples.size()); ++s) {
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
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;  // Используем актуальную частоту дискретизации
    float samplesPerBar = samplesPerBeat * 4;  // 4 удара в такте
    
    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioChannels[0].size()) / (width() * zoomLevel);
    
    float xOffset = horizontalOffset * width();
    
    // Рисуем линии тактов
    painter.setPen(QPen(barLineColor));
    for (float x = -xOffset; x < width(); x += (samplesPerBar / samplesPerPixel)) {
        painter.drawLine(QPointF(x, 0), QPointF(x, height()));
    }
    
    // Рисуем линии битов
    painter.setPen(QPen(beatLineColor));
    for (float x = -xOffset; x < width(); x += (samplesPerBeat / samplesPerPixel)) {
        painter.drawLine(QPointF(x, 0), QPointF(x, height()));
    }
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
    if (event->modifiers() & Qt::ControlModifier) {
        // Масштабирование
        float delta = event->angleDelta().y() / 120.f;
        float newZoom = zoomLevel * (delta > 0 ? 1.1f : 0.9f);
        setZoomLevel(newZoom);
    }
    event->accept();
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (isDragging) {
        QPoint delta = event->pos() - lastMousePos;
        horizontalOffset -= float(delta.x()) / (width() * zoomLevel);
        horizontalOffset = qBound(0.0f, horizontalOffset, 1.0f - (1.0f / zoomLevel));
        lastMousePos = event->pos();
        update();
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    }
} 