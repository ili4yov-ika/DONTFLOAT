#ifndef BEATFIXCOMMAND_H
#define BEATFIXCOMMAND_H

#include <QUndoCommand>
#include <QList>
#include "waveformview.h"
#include "bpmanalyzer.h"

class BeatFixCommand : public QUndoCommand
{
public:
    BeatFixCommand(WaveformView* view, 
                  const QList<QList<float>>& originalData,
                  const QList<QList<float>>& fixedData,
                  float bpm,
                  const QList<BPMAnalyzer::BeatInfo>& beats,
                  QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    WaveformView* waveformView;
    QList<QList<float>> originalAudioData;
    QList<QList<float>> fixedAudioData;
    float bpmValue;
    QList<BPMAnalyzer::BeatInfo> beatInfo;
};

#endif // BEATFIXCOMMAND_H 