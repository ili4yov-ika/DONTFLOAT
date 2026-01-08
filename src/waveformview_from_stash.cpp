#include "waveformview.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtCore/QTime>

// ╨Ю╨┐╤А╨╡╨┤╨╡╨╗╨╡╨╜╨╕╨╡ ╤Б╤В╨░╤В╨╕╤З╨╡╤Б╨║╨╕╤Е ╨║╨╛╨╜╤Б╤В╨░╨╜╤В
const int WaveformView::minZoom = 1;
const int WaveformView::maxZoom = 1000;
const int WaveformView::markerHeight = 20;  // ╨Т╤Л╤Б╨╛╤В╨░ ╨╛╨▒╨╗╨░╤Б╤В╨╕ ╨╝╨░╤А╨║╨╡╤А╨╛╨▓
const int WaveformView::markerSpacing = 60;  // ╨Ь╨╕╨╜╨╕╨╝╨░╨╗╤М╨╜╨╛╨╡ ╤А╨░╤Б╤Б╤В╨╛╤П╨╜╨╕╨╡ ╨╝╨╡╨╢╨┤╤Г ╨╝╨░╤А╨║╨╡╤А╨░╨╝╨╕ ╨▓ ╨┐╨╕╨║╤Б╨╡╨╗╤П╤Е

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
    , scrollStep(0.1f)    // 10% ╨╛╤В ╤И╨╕╤А╨╕╨╜╤Л ╨╛╨║╨╜╨░
    , zoomStep(1.2f)      // 20% ╨╕╨╖╨╝╨╡╨╜╨╡╨╜╨╕╨╡ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    , showTimeDisplay(true)
    , showBarsDisplay(false)
    , loopStartPosition(0)
    , loopEndPosition(0)
{
    setMinimumHeight(100);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // ╨а╨░╨╖╤А╨╡╤И╨░╨╡╨╝ ╨┐╨╛╨╗╤Г╤З╨╡╨╜╨╕╨╡ ╤Д╨╛╨║╤Г╤Б╨░ ╨┤╨╗╤П ╨╛╨▒╤А╨░╨▒╨╛╤В╨║╨╕ ╨║╨╗╨░╨▓╨╕╤И
}

void WaveformView::setAudioData(const QVector<QVector<float>>& channels)
{
    // ╨Я╤А╨╛╨▓╨╡╤А╨║╨░ ╨╜╨░ ╨┐╤Г╤Б╤В╤Л╨╡ ╨┤╨░╨╜╨╜╤Л╨╡
    if (channels.isEmpty()) {
        return;
    }

    // ╨Я╤А╨╛╨▓╨╡╤А╨║╨░ ╨╜╨░ ╨║╨╛╤А╤А╨╡╨║╤В╨╜╨╛╤Б╤В╤М ╨┤╨░╨╜╨╜╤Л╤Е
    for (const auto& channel : channels) {
        if (channel.isEmpty()) {
            return;
        }
    }

    // ╨Э╨╛╤А╨╝╨░╨╗╨╕╨╖╨░╤Ж╨╕╤П ╨┤╨░╨╜╨╜╤Л╤Е
    audioData.clear();
    for (const auto& channel : channels) {
        QVector<float> normalizedChannel;
        normalizedChannel.reserve(channel.size());
        
        // ╨Э╨░╤Е╨╛╨┤╨╕╨╝ ╨╝╨░╨║╤Б╨╕╨╝╨░╨╗╤М╨╜╨╛╨╡ ╨╖╨╜╨░╤З╨╡╨╜╨╕╨╡ ╨┤╨╗╤П ╨╜╨╛╤А╨╝╨░╨╗╨╕╨╖╨░╤Ж╨╕╨╕
        float maxValue = 0.0f;
        for (float sample : channel) {
            maxValue = qMax(maxValue, qAbs(sample));
        }
        
        // ╨Э╨╛╤А╨╝╨░╨╗╨╕╨╖╤Г╨╡╨╝ ╨╖╨╜╨░╤З╨╡╨╜╨╕╤П
        if (maxValue > 0.0f) {
            for (float sample : channel) {
                normalizedChannel.append(sample / maxValue);
            }
        } else {
            normalizedChannel = channel;
        }
        
        audioData.append(normalizedChannel);
    }

    // ╨б╨▒╤А╨░╤Б╤Л╨▓╨░╨╡╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒ ╨╕ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡
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
    
    // ╨д╨╛╨╜
    painter.fillRect(rect(), colors.getBackgroundColor());
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╤Б╨╡╤В╨║╤Г
    drawGrid(painter, rect());
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╛╤В╨║╨╗╨╛╨╜╨╡╨╜╨╕╤П ╨▒╨╕╤В╨╛╨▓
    drawBeatDeviations(painter, rect());
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨▓╨╛╨╗╨╜╤Г
    if (!audioData.isEmpty()) {
        float channelHeight = height() / float(audioData.size());
        for (int i = 0; i < audioData.size(); ++i) {
            QRectF channelRect(0, i * channelHeight, width(), channelHeight);
            drawWaveform(painter, audioData[i], channelRect);
        }
    }
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╗╨╕╨╜╨╕╨╕ ╤В╨░╨║╤В╨╛╨▓
    drawBeatLines(painter, rect());
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨░╤А╨║╨╡╤А╤Л ╤В╨░╨║╤В╨╛╨▓
    QRectF markerRect(0, 0, width(), markerHeight);
    drawBarMarkers(painter, markerRect);
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨║╤Г╤А╤Б╨╛╤А ╨▓╨╛╤Б╨┐╤А╨╛╨╕╨╖╨▓╨╡╨┤╨╡╨╜╨╕╤П
    drawPlaybackCursor(painter, rect());

    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨░╤А╨║╨╡╤А╤Л ╤Ж╨╕╨║╨╗╨░
    drawLoopMarkers(painter, rect());
}

void WaveformView::drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect)
{
    if (samples.isEmpty()) return;

    // ╨Ъ╨╛╨╗╨╕╤З╨╡╤Б╤В╨▓╨╛ ╤Б╤Н╨╝╨┐╨╗╨╛╨▓ ╨╜╨░ ╨┐╨╕╨║╤Б╨╡╨╗╤М ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    float samplesPerPixel = float(samples.size()) / (rect.width() * zoomLevel);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨░╤З╨░╨╗╤М╨╜╤Л╨╣ ╤Б╤Н╨╝╨┐╨╗ ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╤П ╨╕ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, samples.size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // ╨Я╤А╨╕╨╝╨╡╨╜╤П╨╡╨╝ ╨▓╨╡╤А╤В╨╕╨║╨░╨╗╤М╨╜╨╛╨╡ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡
    QRectF adjustedRect = rect;
    adjustedRect.translate(0, -verticalOffset * rect.height());
    
    for (int x = 0; x < rect.width(); ++x) {
        int currentSample = startSample + int(x * samplesPerPixel);
        int nextSample = startSample + int((x + 1) * samplesPerPixel);
        
        if (currentSample >= samples.size()) break;
        
        // ╨Э╨░╤Е╨╛╨┤╨╕╨╝ ╨╝╨╕╨╜╨╕╨╝╨░╨╗╤М╨╜╨╛╨╡ ╨╕ ╨╝╨░╨║╤Б╨╕╨╝╨░╨╗╤М╨╜╨╛╨╡ ╨╖╨╜╨░╤З╨╡╨╜╨╕╨╡ ╨┤╨╗╤П ╤В╨╡╨║╤Г╤Й╨╡╨│╨╛ ╨┐╨╕╨║╤Б╨╡╨╗╤П
        float minValue = 0, maxValue = 0;
        for (int s = currentSample; s < qMin(nextSample, samples.size()); ++s) {
            minValue = qMin(minValue, samples[s]);
            maxValue = qMax(maxValue, samples[s]);
        }

        // ╨Ю╨┐╤А╨╡╨┤╨╡╨╗╤П╨╡╨╝ ╤Ж╨▓╨╡╤В ╨▓ ╨╖╨░╨▓╨╕╤Б╨╕╨╝╨╛╤Б╤В╨╕ ╨╛╤В ╤З╨░╤Б╤В╨╛╤В╤Л
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
        
        // ╨а╨╕╤Б╤Г╨╡╨╝ ╨▓╨╡╤А╤В╨╕╨║╨░╨╗╤М╨╜╤Г╤О ╨╗╨╕╨╜╨╕╤О ╨╛╤В ╨╝╨╕╨╜╨╕╨╝╤Г╨╝╨░ ╨┤╨╛ ╨╝╨░╨║╤Б╨╕╨╝╤Г╨╝╨░ ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╨▓╨╡╤А╤В╨╕╨║╨░╨╗╤М╨╜╨╛╨│╨╛ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╤П
        QPointF top = sampleToPoint(x, maxValue, adjustedRect);
        QPointF bottom = sampleToPoint(x, minValue, adjustedRect);
        painter.drawLine(top, bottom);
    }
}

void WaveformView::drawGrid(QPainter& painter, const QRect& rect)
{
    // ╨У╨╛╤А╨╕╨╖╨╛╨╜╤В╨░╨╗╤М╨╜╤Л╨╡ ╨╗╨╕╨╜╨╕╨╕ ╨┤╨╗╤П ╤А╨░╨╖╨┤╨╡╨╗╨╡╨╜╨╕╤П ╨║╨░╨╜╨░╨╗╨╛╨▓
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
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┤╨╗╨╕╤В╨╡╨╗╤М╨╜╨╛╤Б╤В╤М ╨╛╨┤╨╜╨╛╨│╨╛ ╤В╨░╨║╤В╨░ ╨▓ ╤Б╤Н╨╝╨┐╨╗╨░╤Е
    float samplesPerBeat = (60.0f * sampleRate) / bpm;
    
    // ╨Ъ╨╛╨╗╨╕╤З╨╡╤Б╤В╨▓╨╛ ╤Б╤Н╨╝╨┐╨╗╨╛╨▓ ╨╜╨░ ╨┐╨╕╨║╤Б╨╡╨╗╤М ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨░╤З╨░╨╗╤М╨╜╤Л╨╣ ╤Б╤Н╨╝╨┐╨╗ ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╤П ╨╕ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // ╨Э╨░╤Е╨╛╨┤╨╕╨╝ ╨┐╨╡╤А╨▓╤Л╨╣ ╤В╨░╨║╤В, ╨▓╤Л╤А╨╛╨▓╨╜╨╡╨╜╨╜╤Л╨╣ ╨┐╨╛ ╨┐╨╡╤А╨▓╨╛╨╣ ╨╛╨▒╨╜╨░╤А╤Г╨╢╨╡╨╜╨╜╨╛╨╣ ╨┤╨╛╨╗╨╡
    int firstBeat = 0;
    if (gridStartSample > 0) {
        // ╨║╨╛╨╗╨╕╤З╨╡╤Б╤В╨▓╨╛ ╤Г╨┤╨░╤А╨╛╨▓ ╨╛╤В ╨╛╨┐╨╛╤А╨╜╨╛╨╣ ╤В╨╛╤З╨║╨╕ ╨┤╨╛ ╤Б╤В╨░╤А╤В╨╛╨▓╨╛╨│╨╛ ╤Б╤Н╨╝╨┐╨╗╨░
        float beatsFromGridStart = float(startSample - gridStartSample) / samplesPerBeat;
        firstBeat = int(qFloor(beatsFromGridStart));
    } else {
        firstBeat = int(startSample / samplesPerBeat);
    }
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╗╨╕╨╜╨╕╨╕ ╤В╨░╨║╤В╨╛╨▓
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

    // playbackPosition ╤Г╨╢╨╡ ╨▓ ╨╝╨╕╨╗╨╗╨╕╤Б╨╡╨║╤Г╨╜╨┤╨░╤Е, ╨║╨╛╨╜╨▓╨╡╤А╤В╨╕╤А╤Г╨╡╨╝ ╨▓ ╤Б╤Н╨╝╨┐╨╗╤Л
    qint64 cursorSample = (playbackPosition * sampleRate) / 1000;
    
    // ╨Ю╨│╤А╨░╨╜╨╕╤З╨╕╨▓╨░╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨║╨░╤А╨╡╤В╨║╨╕ ╨│╤А╨░╨╜╨╕╤Ж╨░╨╝╨╕ ╨░╤Г╨┤╨╕╨╛
    cursorSample = qBound(qint64(0), cursorSample, qint64(audioData[0].size() - 1));
    
    // ╨Ш╤Б╨┐╨╛╨╗╤М╨╖╤Г╨╡╨╝ ╤В╤Г ╨╢╨╡ ╨╗╨╛╨│╨╕╨║╤Г, ╤З╤В╨╛ ╨╕ ╨▓ ╨┤╤А╤Г╨│╨╕╤Е ╨╝╨╡╤В╨╛╨┤╨░╤Е ╤А╨╕╤Б╨╛╨▓╨░╨╜╨╕╤П
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨║╨░╤А╨╡╤В╨║╨╕ ╨▓ ╨┐╨╕╨║╤Б╨╡╨╗╤П╤Е
    float cursorX = (cursorSample - startSample) / samplesPerPixel;
    

    
    if (cursorX >= 0 && cursorX < rect.width()) {
        painter.setPen(QPen(colors.getPlayPositionColor(), 2));
        painter.drawLine(QPointF(rect.x() + cursorX, rect.top()), QPointF(rect.x() + cursorX, rect.bottom()));
        
        // ╨а╨╕╤Б╤Г╨╡╨╝ ╤В╤А╨╡╤Г╨│╨╛╨╗╤М╨╜╨╕╨║ ╨╜╨░ ╨║╤Г╤А╤Б╨╛╤А╨╡
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
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┤╨╗╨╕╤В╨╡╨╗╤М╨╜╨╛╤Б╤В╤М ╨╛╨┤╨╜╨╛╨│╨╛ ╤В╨░╨║╤В╨░ ╨▓ ╤Б╤Н╨╝╨┐╨╗╨░╤Е
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    float samplesPerBar = samplesPerBeat * qMax(1, beatsPerBar);
    
    // ╨Ъ╨╛╨╗╨╕╤З╨╡╤Б╤В╨▓╨╛ ╤Б╤Н╨╝╨┐╨╗╨╛╨▓ ╨╜╨░ ╨┐╨╕╨║╤Б╨╡╨╗╤М ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨░╤З╨░╨╗╤М╨╜╤Л╨╣ ╤Б╤Н╨╝╨┐╨╗ ╤Б ╤Г╤З╨╡╤В╨╛╨╝ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╤П ╨╕ ╨╝╨░╤Б╤И╤В╨░╨▒╨░
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // ╨Э╨░╤Е╨╛╨┤╨╕╨╝ ╨┐╨╡╤А╨▓╤Л╨╣ ╤В╨░╨║╤В ╨┐╨╡╤А╨╡╨┤ ╨▓╨╕╨┤╨╕╨╝╨╛╨╣ ╨╛╨▒╨╗╨░╤Б╤В╤М╤О, ╨▓╤Л╤А╨╛╨▓╨╜╨╡╨╜╨╜╤Л╨╣ ╨┐╨╛ ╨╛╨┐╨╛╤А╨╜╨╛╨╣ ╨┤╨╛╨╗╨╡
    float firstBar = 0.0f;
    if (gridStartSample > 0) {
        float barsFromGridStart = float(startSample - gridStartSample) / samplesPerBar;
        firstBar = floor(barsFromGridStart);
    } else {
        firstBar = floor(startSample / samplesPerBar);
    }
    
    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨░╤А╨║╨╡╤А╤Л ╤В╨░╨║╤В╨╛╨▓
    for (float bar = firstBar; ; bar++) {
        float barSample = gridStartSample > 0
            ? (gridStartSample + bar * samplesPerBar)
            : (bar * samplesPerBar);
        float x = (barSample - startSample) / samplesPerPixel;
        if (x >= rect.width()) break;
        if (x >= 0) {
            // ╨а╨╕╤Б╤Г╨╡╨╝ ╤Д╨╛╨╜ ╨╝╨░╤А╨║╨╡╤А╨░
            QRectF markerRect(x - markerSpacing/2, rect.top(), markerSpacing, rect.height());
            painter.fillRect(markerRect, colors.getMarkerBackgroundColor());
            
            // ╨а╨╕╤Б╤Г╨╡╨╝ ╤В╨╡╨║╤Б╤В ╨╝╨░╤А╨║╨╡╤А╨░
            QString barText = QString::number(int(bar) + 1);
            QRectF textRect = painter.fontMetrics().boundingRect(barText);
            painter.drawText(QPointF(
                x - textRect.width()/2,
                rect.top() + rect.height() - 4
            ), barText);
            
            // ╨а╨╕╤Б╤Г╨╡╨╝ ╨▓╨╡╤А╤В╨╕╨║╨░╨╗╤М╨╜╤Г╤О ╨╗╨╕╨╜╨╕╤О
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

    // ╨Ш╤Б╨┐╨╛╨╗╤М╨╖╤Г╨╡╨╝ ╤В╤Г ╨╢╨╡ ╨╗╨╛╨│╨╕╨║╤Г, ╤З╤В╨╛ ╨╕ ╨▓ drawWaveform
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    for (int i = 1; i < beats.size(); ++i) {
        const auto& prevBeat = beats[i - 1];
        const auto& currentBeat = beats[i];
        
        if (qAbs(currentBeat.deviation) > 0.1f) {
            // ╨Ю╨┐╤А╨╡╨┤╨╡╨╗╤П╨╡╨╝ ╨║╨╛╨╛╤А╨┤╨╕╨╜╨░╤В╤Л ╨┤╨╗╤П ╨╛╤В╤А╨╕╤Б╨╛╨▓╨║╨╕
            float x1 = float(prevBeat.position - startSample) / samplesPerPixel;
            float x2 = float(currentBeat.position - startSample) / samplesPerPixel;
            
            if (x2 < 0 || x1 > rect.width()) {
                continue;
            }
            
            // ╨Т╤Л╨▒╨╕╤А╨░╨╡╨╝ ╤Ж╨▓╨╡╤В ╨▓ ╨╖╨░╨▓╨╕╤Б╨╕╨╝╨╛╤Б╤В╨╕ ╨╛╤В ╤В╨╕╨┐╨░ ╨╛╤В╨║╨╗╨╛╨╜╨╡╨╜╨╕╤П
            QColor color = currentBeat.deviation > 0 ? 
                QColor(255, 100, 100, 50) :  // ╨Я╨╛╨╗╤Г╨┐╤А╨╛╨╖╤А╨░╤З╨╜╤Л╨╣ ╨║╤А╨░╤Б╨╜╤Л╨╣
                QColor(100, 255, 100, 50);   // ╨Я╨╛╨╗╤Г╨┐╤А╨╛╨╖╤А╨░╤З╨╜╤Л╨╣ ╨╖╨╡╨╗╨╡╨╜╤Л╨╣
            
            // ╨а╨╕╤Б╤Г╨╡╨╝ ╨┐╤А╤П╨╝╨╛╤Г╨│╨╛╨╗╤М╨╜╨╕╨║ ╨╛╤В╨║╨╗╨╛╨╜╨╡╨╜╨╕╤П
            QRectF deviationRect(x1, 0, x2 - x1, rect.height());
            painter.fillRect(deviationRect, color);
        }
    }
}

QString WaveformView::getPositionText(qint64 position) const
{
    QString positionText;
    
    if (showTimeDisplay) {
        // ╨Ъ╨╛╨╜╨▓╨╡╤А╤В╨╕╤А╤Г╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨▓ ╨╝╨╕╨╗╨╗╨╕╤Б╨╡╨║╤Г╨╜╨┤╤Л
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
        // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╤В╨░╨║╤В╤Л ╨╕ ╨┤╨╛╨╗╨╕
        float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
        float beatsFromStart = float(position) / samplesPerBeat;
        const int bpb = qMax(1, beatsPerBar);
        int bar = int(beatsFromStart / bpb) + 1;  // ╨в╨░╨║╤В╤Л ╨╜╨░╤З╨╕╨╜╨░╤О╤В╤Б╤П ╤Б 1
        int beat = int(beatsFromStart) % bpb + 1;  // ╨Ф╨╛╨╗╨╕ ╨╜╨░╤З╨╕╨╜╨░╤О╤В╤Б╤П ╤Б 1
        float subBeat = (beatsFromStart - float(int(beatsFromStart))) * float(bpb); // ╨Ф╨╛╨╗╨╕ ╤В╨░╨║╤В╨░ ╨▓ beat units
        
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
    // position ╨┐╤А╨╕╤Е╨╛╨┤╨╕╤В ╨▓ ╨╝╨╕╨╗╨╗╨╕╤Б╨╡╨║╤Г╨╜╨┤╨░╤Е, ╤Б╨╛╤Е╤А╨░╨╜╤П╨╡╨╝ ╨║╨░╨║ ╨╡╤Б╤В╤М
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
    
    // ╨Ю╨│╤А╨░╨╜╨╕╤З╨╕╨▓╨░╨╡╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒
    newZoom = qBound(float(minZoom), newZoom, float(maxZoom));
    
    if (qFuzzyCompare(newZoom, oldZoom)) {
        event->accept();
        return;
    }
    
    // ╨Я╨╛╨╗╤Г╤З╨░╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨╝╤Л╤И╨╕ ╨╛╤В╨╜╨╛╤Б╨╕╤В╨╡╨╗╤М╨╜╨╛ ╨▓╨╕╨┤╨╢╨╡╤В╨░
    QPoint mousePos = event->position().toPoint();
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╤В╨╡╨║╤Г╤Й╤Г╤О ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨▓ ╤Б╤Н╨╝╨┐╨╗╨░╤Е ╨┐╨╛╨┤ ╨║╤Г╤А╤Б╨╛╤А╨╛╨╝ ╨╝╤Л╤И╨╕
    float samplesPerPixel = float(audioData[0].size()) / (width() * oldZoom);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    qint64 mouseSample = startSample + qint64(mousePos.x() * samplesPerPixel);
    
    // ╨Ю╨▒╨╜╨╛╨▓╨╗╤П╨╡╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒
    zoomLevel = newZoom;
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨╛╨▓╤Г╤О ╨┐╨╛╨╖╨╕╤Ж╨╕╤О, ╤З╤В╨╛╨▒╤Л ╨║╤Г╤А╤Б╨╛╤А ╨╝╤Л╤И╨╕ ╨╛╤Б╤В╨░╨╗╤Б╤П ╨╜╨░╨┤ ╤В╨╡╨╝ ╨╢╨╡ ╤Б╤Н╨╝╨┐╨╗╨╛╨╝
    float newSamplesPerPixel = float(audioData[0].size()) / (width() * newZoom);
    int newVisibleSamples = int(width() * newSamplesPerPixel);
    int newMaxStartSample = qMax(0, audioData[0].size() - newVisibleSamples);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨╛╨▓╨╛╨╡ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡
    float newOffset = 0.0f;
    if (newMaxStartSample > 0) {
        newOffset = float(mouseSample - mousePos.x() * newSamplesPerPixel) / newMaxStartSample;
        newOffset = qBound(0.0f, newOffset, 1.0f);
    }
    
    horizontalOffset = newOffset;
    
    // ╨н╨╝╨╕╤В╨╕╤А╤Г╨╡╨╝ ╤Б╨╕╨│╨╜╨░╨╗╤Л ╨┤╨╗╤П ╨╛╨▒╨╜╨╛╨▓╨╗╨╡╨╜╨╕╤П ╤Б╨║╤А╨╛╨╗╨╗╨▒╨░╤А╨░
    emit zoomChanged(zoomLevel);
    emit horizontalOffsetChanged(horizontalOffset);
    update();
    
    event->accept();
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false; // ╨Э╨░╤З╨╕╨╜╨░╨╡╨╝ ╨║╨░╨║ ╨╜╨╡ ╨┐╨╡╤А╨╡╤В╨░╤Б╨║╨╕╨▓╨░╨╜╨╕╨╡
        lastMousePos = event->pos();

        // ╨г╤Б╤В╨░╨╜╨░╨▓╨╗╨╕╨▓╨░╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨▓╨╛╤Б╨┐╤А╨╛╨╕╨╖╨▓╨╡╨┤╨╡╨╜╨╕╤П ╨┐╨╛ ╨║╨╗╨╕╨║╤Г
        if (!audioData.isEmpty()) {
            // ╨Ш╤Б╨┐╨╛╨╗╤М╨╖╤Г╨╡╨╝ ╤В╤Г ╨╢╨╡ ╨╗╨╛╨│╨╕╨║╤Г, ╤З╤В╨╛ ╨╕ ╨▓ drawPlaybackCursor
            float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
            int visibleSamples = int(width() * samplesPerPixel);
            int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
            int startSample = int(horizontalOffset * maxStartSample);
            
            // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨║╨╗╨╕╨║╨░ ╨▓ ╤Б╤Н╨╝╨┐╨╗╨░╤Е
            qint64 clickSample = startSample + qint64(event->pos().x() * samplesPerPixel);
            clickSample = qBound(qint64(0), clickSample, qint64(audioData[0].size() - 1));
            
            // ╨Ъ╨╛╨╜╨▓╨╡╤А╤В╨╕╤А╤Г╨╡╨╝ ╤Б╤Н╨╝╨┐╨╗╤Л ╨▓ ╨╝╨╕╨╗╨╗╨╕╤Б╨╡╨║╤Г╨╜╨┤╤Л ╨┤╨╗╤П ╨▓╨╜╤Г╤В╤А╨╡╨╜╨╜╨╡╨│╨╛ ╨╕╤Б╨┐╨╛╨╗╤М╨╖╨╛╨▓╨░╨╜╨╕╤П
            qint64 newPosition = (clickSample * 1000) / sampleRate;
            playbackPosition = newPosition;
            
            // ╨Я╨╛╨║╨░╨╖╤Л╨▓╨░╨╡╨╝ ╨▓╤Б╨┐╨╗╤Л╨▓╨░╤О╤Й╤Г╤О ╨┐╨╛╨┤╤Б╨║╨░╨╖╨║╤Г
            QString positionText = getPositionText(clickSample);
            if (!positionText.isEmpty()) {
                QToolTip::showText(event->globalPosition().toPoint(), positionText, this);
            }
            
            // ╨Ю╤В╨┐╤А╨░╨▓╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨▓ ╨╝╨╕╨╗╨╗╨╕╤Б╨╡╨║╤Г╨╜╨┤╨░╤Е ╨┤╨╗╤П QMediaPlayer
            emit positionChanged(newPosition);
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        // ╨Э╨░╤З╨╕╨╜╨░╨╡╨╝ ╨┐╨░╨╜╨╛╤А╨░╨╝╨╕╤А╨╛╨▓╨░╨╜╨╕╨╡ ╨┐╤А╨░╨▓╨╛╨╣ ╨║╨╜╨╛╨┐╨║╨╛╨╣ ╨╝╤Л╤И╨╕
        isRightMousePanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - lastMousePos;
        
        // ╨Ю╨┐╤А╨╡╨┤╨╡╨╗╤П╨╡╨╝, ╨┐╨╡╤А╨╡╤В╨░╤Б╨║╨╕╨▓╨░╨╡╨╝ ╨╗╨╕ ╨╝╤Л (╨┤╨▓╨╕╨╢╨╡╨╜╨╕╨╡ ╨▒╨╛╨╗╤М╤И╨╡ ╨╛╨┐╤А╨╡╨┤╨╡╨╗╨╡╨╜╨╜╨╛╨│╨╛ ╨┐╨╛╤А╨╛╨│╨░)
        if (!isDragging && delta.manhattanLength() > 5) {
            isDragging = true;
        }
        
        if (isDragging) {
            // ╨Х╤Б╨╗╨╕ ╨┐╨╡╤А╨╡╤В╨░╤Б╨║╨╕╨▓╨░╨╡╨╝ ╤Б ╨╖╨░╨╢╨░╤В╨╛╨╣ ╨║╨╜╨╛╨┐╨║╨╛╨╣ ╨╝╤Л╤И╨╕, ╨╛╨▒╨╜╨╛╨▓╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨▓╨╛╤Б╨┐╤А╨╛╨╕╨╖╨▓╨╡╨┤╨╡╨╜╨╕╤П
            if (!audioData.isEmpty()) {
                // ╨Ш╤Б╨┐╨╛╨╗╤М╨╖╤Г╨╡╨╝ ╤В╤Г ╨╢╨╡ ╨╗╨╛╨│╨╕╨║╤Г, ╤З╤В╨╛ ╨╕ ╨▓ drawPlaybackCursor
                float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
                int visibleSamples = int(width() * samplesPerPixel);
                int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
                int startSample = int(horizontalOffset * maxStartSample);
                
                // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨║╨╗╨╕╨║╨░ ╨▓ ╤Б╤Н╨╝╨┐╨╗╨░╤Е
                qint64 clickSample = startSample + qint64(event->pos().x() * samplesPerPixel);
                clickSample = qBound(qint64(0), clickSample, qint64(audioData[0].size() - 1));
                
                qint64 newPosition = (clickSample * 1000) / sampleRate;
                playbackPosition = newPosition;
                
                emit positionChanged(newPosition);
                update();
            }
        } else {
            // ╨Ю╨▒╤Л╤З╨╜╨╛╨╡ ╨┐╨╡╤А╨╡╤В╨░╤Б╨║╨╕╨▓╨░╨╜╨╕╨╡ ╨┤╨╗╤П ╨┐╤А╨╛╨║╤А╤Г╤В╨║╨╕
            adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        }
        
        lastMousePos = event->pos();
    } else if (event->buttons() & Qt::RightButton && isRightMousePanning) {
        // ╨Я╨░╨╜╨╛╤А╨░╨╝╨╕╤А╨╛╨▓╨░╨╜╨╕╨╡ ╨┐╤А╨░╨▓╨╛╨╣ ╨║╨╜╨╛╨┐╨║╨╛╨╣ ╨╝╤Л╤И╨╕
        QPoint delta = event->pos() - lastMousePos;
        adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        lastMousePos = event->pos();
    } else {
        // ╨Я╤А╨╕ ╨╜╨░╨▓╨╡╨┤╨╡╨╜╨╕╨╕ ╨╝╤Л╤И╨╕ ╨▒╨╡╨╖ ╨╜╨░╨╢╨░╤В╤Л╤Е ╨║╨╜╨╛╨┐╨╛╨║ ╨┐╨╛╨║╨░╨╖╤Л╨▓╨░╨╡╨╝ ╨║╤Г╤А╤Б╨╛╤А "╤А╤Г╨║╨░"
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
    // ╨Я╤А╨╕╨╝╨╡╨╜╤П╨╡╨╝ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡ ╤Б ╨╛╨│╤А╨░╨╜╨╕╤З╨╡╨╜╨╕╨╡╨╝ ╨╛╤В 0 ╨┤╨╛ 1
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
    
    // ╨Ю╨│╤А╨░╨╜╨╕╤З╨╕╨▓╨░╨╡╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒
    newZoom = qBound(float(minZoom), newZoom, float(maxZoom));
    
    if (qFuzzyCompare(newZoom, oldZoom)) {
        return;
    }
    
    // ╨Ь╨░╤Б╤И╤В╨░╨▒╨╕╤А╤Г╨╡╨╝ ╨╛╤В╨╜╨╛╤Б╨╕╤В╨╡╨╗╤М╨╜╨╛ ╤Ж╨╡╨╜╤В╤А╨░ ╤Н╨║╤А╨░╨╜╨░
    QPoint centerPos = QPoint(width() / 2, height() / 2);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╤В╨╡╨║╤Г╤Й╤Г╤О ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨▓ ╤Б╤Н╨╝╨┐╨╗╨░╤Е ╨┐╨╛╨┤ ╤Ж╨╡╨╜╤В╤А╨╛╨╝ ╤Н╨║╤А╨░╨╜╨░
    float samplesPerPixel = float(audioData[0].size()) / (width() * oldZoom);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    qint64 centerSample = startSample + qint64(centerPos.x() * samplesPerPixel);
    
    // ╨Ю╨▒╨╜╨╛╨▓╨╗╤П╨╡╨╝ ╨╝╨░╤Б╤И╤В╨░╨▒
    zoomLevel = newZoom;
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨╛╨▓╤Г╤О ╨┐╨╛╨╖╨╕╤Ж╨╕╤О, ╤З╤В╨╛╨▒╤Л ╤Ж╨╡╨╜╤В╤А ╤Н╨║╤А╨░╨╜╨░ ╨╛╤Б╤В╨░╨╗╤Б╤П ╨╜╨░╨┤ ╤В╨╡╨╝ ╨╢╨╡ ╤Б╤Н╨╝╨┐╨╗╨╛╨╝
    float newSamplesPerPixel = float(audioData[0].size()) / (width() * newZoom);
    int newVisibleSamples = int(width() * newSamplesPerPixel);
    int newMaxStartSample = qMax(0, audioData[0].size() - newVisibleSamples);
    
    // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨╜╨╛╨▓╨╛╨╡ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡
    float newOffset = 0.0f;
    if (newMaxStartSample > 0) {
        newOffset = float(centerSample - centerPos.x() * newSamplesPerPixel) / newMaxStartSample;
        newOffset = qBound(0.0f, newOffset, 1.0f);
    }
    
    horizontalOffset = newOffset;
    
    // ╨н╨╝╨╕╤В╨╕╤А╤Г╨╡╨╝ ╤Б╨╕╨│╨╜╨░╨╗╤Л ╨┤╨╗╤П ╨╛╨▒╨╜╨╛╨▓╨╗╨╡╨╜╨╕╤П ╤Б╨║╤А╨╛╨╗╨╗╨▒╨░╤А╨░
    emit zoomChanged(zoomLevel);
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::setHorizontalOffset(float offset)
{
    // ╨Ю╨│╤А╨░╨╜╨╕╤З╨╕╨▓╨░╨╡╨╝ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡ ╨▓ ╨┐╤А╨╡╨┤╨╡╨╗╨░╤Е ╨╛╤В 0 ╨┤╨╛ 1
    horizontalOffset = qBound(0.0f, offset, 1.0f);
    
    // ╨Я╤А╨╕ ╨╕╨╖╨╝╨╡╨╜╨╡╨╜╨╕╨╕ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╤П ╨╛╨▒╨╜╨╛╨▓╨╗╤П╨╡╨╝ ╨╛╤В╨╛╨▒╤А╨░╨╢╨╡╨╜╨╕╨╡
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::setVerticalOffset(float offset)
{
    // ╨Ю╨│╤А╨░╨╜╨╕╤З╨╕╨▓╨░╨╡╨╝ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╨╡ ╨▓ ╨┐╤А╨╡╨┤╨╡╨╗╨░╤Е ╨╛╤В 0 ╨┤╨╛ 1
    verticalOffset = qBound(0.0f, offset, 1.0f);
    
    // ╨Я╤А╨╕ ╨╕╨╖╨╝╨╡╨╜╨╡╨╜╨╕╨╕ ╤Б╨╝╨╡╤Й╨╡╨╜╨╕╤П ╨╛╨▒╨╜╨╛╨▓╨╗╤П╨╡╨╝ ╨╛╤В╨╛╨▒╤А╨░╨╢╨╡╨╜╨╕╨╡
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
    // ╨г╤Б╤В╨░╨╜╨░╨▓╨╗╨╕╨▓╨░╨╡╨╝ ╨║╤Г╤А╤Б╨╛╤А "╤А╤Г╨║╨░" ╨┐╤А╨╕ ╨▓╤Е╨╛╨┤╨╡ ╨▓ ╨▓╨╕╨┤╨╢╨╡╤В
    if (!isRightMousePanning) {
        setCursor(Qt::OpenHandCursor);
    }
}

void WaveformView::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    // ╨Т╨╛╨╖╨▓╤А╨░╤Й╨░╨╡╨╝ ╨╛╨▒╤Л╤З╨╜╤Л╨╣ ╨║╤Г╤А╤Б╨╛╤А ╨┐╤А╨╕ ╨▓╤Л╤Е╨╛╨┤╨╡ ╨╕╨╖ ╨▓╨╕╨┤╨╢╨╡╤В╨░
    if (!isRightMousePanning) {
        setCursor(Qt::ArrowCursor);
    }
}

void WaveformView::drawLoopMarkers(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty() || (loopStartPosition <= 0 && loopEndPosition <= 0)) {
        return;
    }
    
    // ╨Ш╤Б╨┐╨╛╨╗╤М╨╖╤Г╨╡╨╝ ╤В╤Г ╨╢╨╡ ╨╗╨╛╨│╨╕╨║╤Г, ╤З╤В╨╛ ╨╕ ╨▓ drawWaveform
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    
    // ╨Э╨░╤Б╤В╤А╨░╨╕╨▓╨░╨╡╨╝ ╨┐╨╡╤А╨╛ ╨┤╨╗╤П ╨╝╨░╤А╨║╨╡╤А╨╛╨▓ ╤Ж╨╕╨║╨╗╨░
    QPen loopPen;
    loopPen.setWidth(2);
    loopPen.setStyle(Qt::DashLine);

    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨░╤А╨║╨╡╤А ╨╜╨░╤З╨░╨╗╨░ ╤Ж╨╕╨║╨╗╨░
    if (loopStartPosition > 0) {
        qint64 startSamplePos = (loopStartPosition * sampleRate) / 1000;
        
        // ╨Я╤А╨╛╨▓╨╡╤А╤П╨╡╨╝, ╨┐╨╛╨┐╨░╨┤╨░╨╡╤В ╨╗╨╕ ╨╝╨░╤А╨║╨╡╤А ╨▓ ╨▓╨╕╨┤╨╕╨╝╤Г╤О ╨╛╨▒╨╗╨░╤Б╤В╤М
        if (startSamplePos >= startSample && startSamplePos < startSample + visibleSamples) {
            loopPen.setColor(QColor(0, 255, 0, 180)); // ╨Ч╨╡╨╗╤С╨╜╤Л╨╣ ╨┤╨╗╤П ╨╜╨░╤З╨░╨╗╨░
            painter.setPen(loopPen);
            
            // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨╝╨░╤А╨║╨╡╤А╨░ ╨▓ ╨┐╨╕╨║╤Б╨╡╨╗╤П╤Е
            float x = (startSamplePos - startSample) / samplesPerPixel;
            
            if (x >= 0 && x < rect.width()) {
                painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));
                
                // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨╡╤В╨║╤Г "A"
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(QPointF(rect.x() + x + 5, rect.top() + 20), "A");
            }
        }
    }

    // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨░╤А╨║╨╡╤А ╨║╨╛╨╜╤Ж╨░ ╤Ж╨╕╨║╨╗╨░
    if (loopEndPosition > 0) {
        qint64 endSamplePos = (loopEndPosition * sampleRate) / 1000;
        
        // ╨Я╤А╨╛╨▓╨╡╤А╤П╨╡╨╝, ╨┐╨╛╨┐╨░╨┤╨░╨╡╤В ╨╗╨╕ ╨╝╨░╤А╨║╨╡╤А ╨▓ ╨▓╨╕╨┤╨╕╨╝╤Г╤О ╨╛╨▒╨╗╨░╤Б╤В╤М
        if (endSamplePos >= startSample && endSamplePos < startSample + visibleSamples) {
            loopPen.setColor(QColor(255, 0, 0, 180)); // ╨Ъ╤А╨░╤Б╨╜╤Л╨╣ ╨┤╨╗╤П ╨║╨╛╨╜╤Ж╨░
            painter.setPen(loopPen);
            
            // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╤О ╨╝╨░╤А╨║╨╡╤А╨░ ╨▓ ╨┐╨╕╨║╤Б╨╡╨╗╤П╤Е
            float x = (endSamplePos - startSample) / samplesPerPixel;
            
            if (x >= 0 && x < rect.width()) {
                painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));
                
                // ╨а╨╕╤Б╤Г╨╡╨╝ ╨╝╨╡╤В╨║╤Г "B"
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(QPointF(rect.x() + x + 5, rect.top() + 20), "B");
            }
        }
    }

    // ╨Х╤Б╨╗╨╕ ╤Г╤Б╤В╨░╨╜╨╛╨▓╨╗╨╡╨╜╤Л ╨╛╨▒╨░ ╨╝╨░╤А╨║╨╡╤А╨░, ╨╖╨░╨╗╨╕╨▓╨░╨╡╨╝ ╨╛╨▒╨╗╨░╤Б╤В╤М ╨╝╨╡╨╢╨┤╤Г ╨╜╨╕╨╝╨╕
    if (loopStartPosition > 0 && loopEndPosition > 0) {
        qint64 startSamplePos = (loopStartPosition * sampleRate) / 1000;
        qint64 endSamplePos = (loopEndPosition * sampleRate) / 1000;
        
        // ╨Я╤А╨╛╨▓╨╡╤А╤П╨╡╨╝, ╨┐╨╛╨┐╨░╨┤╨░╨╡╤В ╨╗╨╕ ╤Е╨╛╤В╤П ╨▒╤Л ╤З╨░╤Б╤В╤М ╨╛╨▒╨╗╨░╤Б╤В╨╕ ╤Ж╨╕╨║╨╗╨░ ╨▓ ╨▓╨╕╨┤╨╕╨╝╤Г╤О ╨╛╨▒╨╗╨░╤Б╤В╤М
        if (endSamplePos >= startSample && startSamplePos < startSample + visibleSamples) {
            // ╨Т╤Л╤З╨╕╤Б╨╗╤П╨╡╨╝ ╨┐╨╛╨╖╨╕╤Ж╨╕╨╕ ╨▓ ╨┐╨╕╨║╤Б╨╡╨╗╤П╤Е
            float x1 = qMax(0.0f, (startSamplePos - startSample) / samplesPerPixel);
            float x2 = qMin(float(rect.width()), (endSamplePos - startSample) / samplesPerPixel);
            
            if (x2 > x1) {
                QColor fillColor(128, 128, 255, 40); // ╨Я╨╛╨╗╤Г╨┐╤А╨╛╨╖╤А╨░╤З╨╜╤Л╨╣ ╤Б╨╕╨╜╨╕╨╣
                painter.fillRect(QRectF(rect.x() + x1, rect.top(), x2 - x1, rect.height()), fillColor);
            }
        }
    }
} 
