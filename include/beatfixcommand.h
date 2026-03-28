#ifndef BEATFIXCOMMAND_H
#define BEATFIXCOMMAND_H

#include <QUndoCommand>
#include <QVector>
#include "waveformview.h"
#include "bpmanalyzer.h"

class BeatFixCommand : public QUndoCommand
{
public:
    BeatFixCommand(WaveformView* view,
                  const QVector<QVector<float>>& originalData,
                  const QVector<QVector<float>>& fixedData,
                  float bpm,
                  const QVector<BPMAnalyzer::BeatInfo>& beats,
                  qint64 gridStartSample = 0,
                  QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    WaveformView* waveformView;
    QVector<QVector<float>> originalAudioData;
    QVector<QVector<float>> fixedAudioData;
    float bpmValue;
    QVector<BPMAnalyzer::BeatInfo> beatInfo;
    qint64 gridStartSampleValue;
};

#endif // BEATFIXCOMMAND_H 