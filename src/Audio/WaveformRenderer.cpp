#include "WaveformRenderer.h"
#include <QPainter>

WaveformRenderer::WaveformRenderer(QWidget *parent)
    : QWidget(parent) {}

void WaveformRenderer::setAudioData(const AudioData &data) {
    audioData = data;
    update();
}

void WaveformRenderer::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (audioData.samples.empty()) return;

    painter.setPen(Qt::green);
    int midY = height() / 2;
    int len = audioData.samples.size();
    int step = len / width();

    for (int x = 0; x < width(); ++x) {
        int i = x * step * audioData.channels;
        if (i < len) {
            float sample = audioData.samples[i]; // только левый канал
            int y = static_cast<int>(sample * midY);
            painter.drawLine(x, midY - y, x, midY + y);
        }
    }
}
