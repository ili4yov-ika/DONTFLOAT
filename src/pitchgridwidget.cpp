#include "pitchgridwidget.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtCore/QTime>
#include <QtWidgets/QApplication>

PitchGridWidget::PitchGridWidget(QWidget *parent)
    : QWidget(parent)
    , sampleRate(44100)
    , playbackPosition(0)
    , horizontalOffset(0.0f)
    , verticalOffset(0.0f)
    , zoomLevel(1.0f)
    , bpm(120.0f)
    , beatsPerBar(4)
    , minPitch(minPitchDefault)
    , maxPitch(maxPitchDefault)
    , isDragging(false)
    , colorScheme("default")
    , selectedPitch(-1)
    , backgroundColor(Qt::white)
    , gridColor(Qt::lightGray)
    , cursorColor(Qt::red)
    , pitchLabelColor(Qt::black)
    , timeLabelColor(Qt::darkGray)
    , selectionColor(Qt::blue)
{
    setMinimumHeight((maxPitch - minPitch + 1) * pitchHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
}

void PitchGridWidget::setAudioData(const QVector<QVector<float>>& data)
{
    audioData = data;
    update();
}

void PitchGridWidget::setSampleRate(int rate)
{
    sampleRate = rate;
    update();
}

void PitchGridWidget::setPlaybackPosition(qint64 position)
{
    playbackPosition = position;
    update();
}

void PitchGridWidget::setHorizontalOffset(float offset)
{
    horizontalOffset = offset;
    update();
}

void PitchGridWidget::setVerticalOffset(float offset)
{
    verticalOffset = offset;
    update();
}

void PitchGridWidget::setZoomLevel(float zoom)
{
    zoomLevel = zoom;
    update();
}

void PitchGridWidget::setBPM(float newBpm)
{
    bpm = newBpm;
    update();
}

void PitchGridWidget::setBeatsPerBar(int beats)
{
    beatsPerBar = beats;
    update();
}

void PitchGridWidget::setPitchRange(int min, int max)
{
    minPitch = min;
    maxPitch = max;
    setMinimumHeight((maxPitch - minPitch + 1) * pitchHeight);
    update();
}

void PitchGridWidget::setColorScheme(const QString& scheme)
{
    colorScheme = scheme;
    
    if (scheme == "dark") {
        backgroundColor = QColor(40, 40, 40);
        gridColor = QColor(80, 80, 80);
        cursorColor = QColor(255, 100, 100);
        pitchLabelColor = QColor(220, 220, 220);
        timeLabelColor = QColor(180, 180, 180);
        selectionColor = QColor(100, 150, 255);
    } else {
        backgroundColor = Qt::white;
        gridColor = Qt::lightGray;
        cursorColor = Qt::red;
        pitchLabelColor = Qt::black;
        timeLabelColor = Qt::darkGray;
        selectionColor = Qt::blue;
    }
    
    update();
}

void PitchGridWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Фон
    painter.fillRect(rect(), backgroundColor);
    
    // Рисуем сетку питчей
    drawPitchGrid(painter, rect());
    
    // Рисуем временную сетку
    drawTimeGrid(painter, rect());
    
    // Рисуем метки питчей
    drawPitchLabels(painter, rect());
    
    // Рисуем курсор воспроизведения
    drawPlaybackCursor(painter, rect());
}

void PitchGridWidget::drawPitchGrid(QPainter& painter, const QRect& rect)
{
    painter.setPen(gridColor);
    
    // Применяем вертикальное смещение
    int verticalOffsetPixels = int(verticalOffset * rect.height());
    
    // Горизонтальные линии для нот
    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        int y = (maxPitch - pitch) * pitchHeight - verticalOffsetPixels;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
}

void PitchGridWidget::drawTimeGrid(QPainter& painter, const QRect& rect)
{
    
    painter.setPen(timeLabelColor);
    painter.setFont(QFont("Arial", 8));
    
    // Рисуем временные метки только если есть аудио данные
    if (!audioData.isEmpty()) {
        float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
        int visibleSamples = int(rect.width() * samplesPerPixel);
        int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
        int startSample = int(horizontalOffset * maxStartSample);
        
        // Рисуем временные метки
        for (int x = 0; x < rect.width(); x += timeGridSpacing) {
            int sampleIndex = startSample + int(x * samplesPerPixel);
            qint64 timeMs = (sampleIndex * 1000) / sampleRate;
            
            QString timeText = QString("%1:%2")
                .arg(timeMs / 60000)
                .arg((timeMs % 60000) / 1000, 2, 10, QChar('0'));
            
            painter.drawText(QPointF(x + 2, 12), timeText);
        }
        
        // Рисуем вертикальные линии тактов, если установлен BPM
        if (bpm > 0.0f && beatsPerBar > 0) {
            
            // Делаем линии тактов более заметными
            painter.setPen(QPen(Qt::red, 3, Qt::SolidLine));
            
            // Вычисляем количество сэмплов на один такт
            float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
            float samplesPerBar = samplesPerBeat * float(beatsPerBar);
            
            // Находим первый такт, который попадает в видимую область
            int firstBar = int(startSample / samplesPerBar);
            float firstBarSample = firstBar * samplesPerBar;
            
            // Рисуем такты, начиная с первого видимого
            for (int bar = firstBar; bar * samplesPerBar < startSample + visibleSamples; ++bar) {
                float barSample = bar * samplesPerBar;
                
                if (barSample >= startSample) {
                    // Вычисляем позицию такта в пикселях
                    float barX = (barSample - startSample) / samplesPerPixel;
                    
                    if (barX >= 0 && barX < rect.width()) {
                        // Рисуем вертикальную линию такта
                        painter.drawLine(QPointF(barX, rect.top()), QPointF(barX, rect.bottom()));
                        
                        // Подписываем номер такта
                        QString barText = QString::number(bar + 1);
                        painter.drawText(QPointF(barX + 2, 20), barText);
                    }
                }
            }
        }
    } else {
        // Если нет аудио данных, но установлены BPM и размер такта, рисуем такты по времени
        if (bpm > 0.0f && beatsPerBar > 0) {
            
            // Делаем линии тактов более заметными
            painter.setPen(QPen(Qt::red, 3, Qt::SolidLine));
            
            // Вычисляем время на один такт
            float secondsPerBeat = 60.0f / bpm;
            float secondsPerBar = secondsPerBeat * float(beatsPerBar);
            
            // Рисуем такты каждые 2 секунды для видимости
            float timeStep = qMax(2.0f, secondsPerBar);
            int maxBars = int(rect.width() / 50); // Максимум 50 пикселей на такт
            
            for (int bar = 0; bar < maxBars; ++bar) {
                float barTime = bar * secondsPerBar;
                float barX = (barTime / timeStep) * rect.width();
                
                if (barX < rect.width()) {
                    // Рисуем вертикальную линию такта
                    painter.drawLine(QPointF(barX, rect.top()), QPointF(barX, rect.bottom()));
                    
                    // Подписываем номер такта
                    QString barText = QString::number(bar + 1);
                    painter.drawText(QPointF(barX + 2, 20), barText);
                }
            }
        } else {
        }
    }
}

void PitchGridWidget::drawPitchLabels(QPainter& painter, const QRect& rect)
{
    painter.setPen(pitchLabelColor);
    painter.setFont(QFont("Arial", 8));
    
    // Применяем вертикальное смещение
    int verticalOffsetPixels = int(verticalOffset * rect.height());
    
    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        int y = (maxPitch - pitch) * pitchHeight + pitchHeight / 2 - verticalOffsetPixels;
        QString pitchName = getPitchName(pitch);
        painter.drawText(QPointF(rect.right() - 30, y + 4), pitchName);
    }
}

void PitchGridWidget::drawPlaybackCursor(QPainter& painter, const QRect& rect)
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
        painter.setPen(QPen(cursorColor, 2));
        painter.drawLine(QPointF(rect.x() + cursorX, rect.top()), QPointF(rect.x() + cursorX, rect.bottom()));
        
        // Рисуем треугольник на курсоре
        QPolygonF triangle;
        triangle << QPointF(cursorX - 5, rect.top() + 5)
                << QPointF(cursorX + 5, rect.top() + 5)
                << QPointF(cursorX, rect.top() + 15);
        painter.setBrush(cursorColor);
        painter.drawPolygon(triangle);
    }
}

QString PitchGridWidget::getPitchName(int midiNote) const
{
    static const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midiNote / 12) - 1;
    int note = midiNote % 12;
    return QString("%1%2").arg(noteNames[note]).arg(octave);
}

int PitchGridWidget::getPitchFromY(int y, const QRect& rect) const
{
    // Учитываем вертикальное смещение
    int verticalOffsetPixels = int(verticalOffset * rect.height());
    int adjustedY = y + verticalOffsetPixels;
    int pitch = maxPitch - (adjustedY / pitchHeight);
    return qBound(minPitch, pitch, maxPitch);
}

qint64 PitchGridWidget::getPositionFromX(int x, const QRect& rect) const
{
    if (audioData.isEmpty()) return 0;
    
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    int sampleIndex = startSample + int(x * samplesPerPixel);
    return (sampleIndex * 1000) / sampleRate;
}

void PitchGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
        
        // Обновляем выбранный питч
        selectedPitch = getPitchFromY(event->pos().y(), rect());
        emit pitchChanged(selectedPitch);
        
        // Обновляем позицию воспроизведения
        qint64 newPosition = getPositionFromX(event->pos().x(), rect());
        emit positionChanged(newPosition);
    }
}

void PitchGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        // Обновляем позицию воспроизведения при перетаскивании
        qint64 newPosition = getPositionFromX(event->pos().x(), rect());
        emit positionChanged(newPosition);
        
        // Обновляем выбранный питч
        selectedPitch = getPitchFromY(event->pos().y(), rect());
        emit pitchChanged(selectedPitch);
        
        lastMousePos = event->pos();
    }
}

void PitchGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    }
}

void PitchGridWidget::wheelEvent(QWheelEvent *event)
{
    // Масштабирование при прокрутке колесика мыши
    if (event->modifiers() & Qt::ControlModifier) {
        float zoomDelta = event->angleDelta().y() > 0 ? 1.2f : 0.833f;
        setZoomLevel(zoomLevel * zoomDelta);
    }
} 