#include "../include/beatfixcommand.h"
#include <QtWidgets> // Для доступа к QUndoCommand и QList

BeatFixCommand::BeatFixCommand(WaveformView* view,
                             const QList<QList<float>>& originalData,
                             const QList<QList<float>>& fixedData,
                             float bpm,
                             const QList<BPMAnalyzer::BeatInfo>& beats,
                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , waveformView(view)
    , originalAudioData(originalData)
    , fixedAudioData(fixedData)
    , bpmValue(bpm)
    , beatInfo(beats)
{
    setText(QObject::tr("Выравнивание долей"));
}

void BeatFixCommand::undo()
{
    if (waveformView) {
        waveformView->setAudioData(originalAudioData);
        waveformView->setBeatInfo(beatInfo);
        waveformView->setBPM(bpmValue);
    }
}

void BeatFixCommand::redo()
{
    if (waveformView) {
        waveformView->setAudioData(fixedAudioData);
        waveformView->setBeatInfo(beatInfo);
        waveformView->setBPM(bpmValue);
    }
} 