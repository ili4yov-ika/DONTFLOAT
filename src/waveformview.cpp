#include "waveformview.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtCore/QTime>

// Определение статических констант
const int WaveformView::minZoom = 1;
const int WaveformView::maxZoom = 1000;
const int WaveformView::markerHeight = 20;  // Высота области маркеров
const int WaveformView::markerSpacing = 60;  // Минимальное расстояние между маркерами в пикселях

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , bpm(120.0f)
    , zoomLevel(1.0f)
    , horizontalOffset(0.0f)
    , verticalOffset(0.0f)
    , isDragging(false)
    , isRightMousePanning(false)
    , sampleRate(44100)
    , playbackPosition(0)
    , gridStartSample(0)
    , beatsPerBar(4)
    , scrollStep(0.1f)    // 10% от ширины окна
    , zoomStep(1.2f)      // 20% изменение масштаба
    , showTimeDisplay(true)
    , showBarsDisplay(false)
    , loopStartPosition(0)
    , loopEndPosition(0)
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
    
    // Рисуем отклонения битов
    drawBeatDeviations(painter, rect());
    
    // Рисуем волну
    if (!audioData.isEmpty()) {
        float channelHeight = height() / float(audioData.size());
        for (int i = 0; i < audioData.size(); ++i) {
            QRectF channelRect(0, i * channelHeight, width(), channelHeight);
            drawWaveform(painter, audioData[i], channelRect);
        }
    }
    
    // Рисуем линии тактов
    drawBeatLines(painter, rect());
    
    // Рисуем маркеры тактов
    QRectF markerRect(0, 0, width(), markerHeight);
    drawBarMarkers(painter, markerRect);
    
    // Рисуем курсор воспроизведения
    drawPlaybackCursor(painter, rect());

    // Рисуем маркеры цикла
    drawLoopMarkers(painter, rect());
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
    
    // Рисуем линии тактов
    painter.setPen(QPen(colors.getBeatColor()));
    for (int beat = firstBeat; ; ++beat) {
        int samplePos = gridStartSample > 0
            ? int(gridStartSample + beat * samplesPerBeat)
            : int(beat * samplesPerBeat);
        if (samplePos < startSample) continue;
        
        float x = (samplePos - startSample) / samplesPerPixel;
        if (x >= rect.width()) break;
        
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
            
            // Рисуем вертикальную линию
            painter.setPen(colors.getBarLineColor());
            painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, height()));
        }
    }
}

void WaveformView::drawBeatDeviations(QPainter& painter, const QRectF& rect)
{
    if (beats.isEmpty() || audioData.isEmpty()) {
        return;
    }

    // Используем ту же логику, что и в drawWaveform
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    for (int i = 1; i < beats.size(); ++i) {
        const auto& prevBeat = beats[i - 1];
        const auto& currentBeat = beats[i];
        
        if (qAbs(currentBeat.deviation) > 0.1f) {
            // Определяем координаты для отрисовки
            float x1 = float(prevBeat.position - startSample) / samplesPerPixel;
            float x2 = float(currentBeat.position - startSample) / samplesPerPixel;
            
            if (x2 < 0 || x1 > rect.width()) {
                continue;
            }
            
            // Выбираем цвет в зависимости от типа отклонения
            QColor color = currentBeat.deviation > 0 ? 
                QColor(255, 100, 100, 50) :  // Полупрозрачный красный
                QColor(100, 255, 100, 50);   // Полупрозрачный зеленый
            
            // Рисуем прямоугольник отклонения
            QRectF deviationRect(x1, 0, x2 - x1, rect.height());
            painter.fillRect(deviationRect, color);
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
        if (!isRightMousePanning) {
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
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