#pragma once

#include <QWidget>
#include "AudioLoader.h"

class WaveformRenderer : public QWidget {
    Q_OBJECT

public:
    explicit WaveformRenderer(QWidget *parent = nullptr);
    void setAudioData(const AudioData &data);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    AudioData audioData;
};
