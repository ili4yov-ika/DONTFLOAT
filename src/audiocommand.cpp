#include "../include/audiocommand.h"
#include "../include/waveformview.h"

AudioCommand::AudioCommand(WaveformView* view, const QVector<QVector<float>>& oldData,
                         const QVector<QVector<float>>& newData, const QString& text)
    : QUndoCommand(text)
    , waveformView(view)
    , oldAudioData(oldData)
    , newAudioData(newData)
{
}

void AudioCommand::undo()
{
    if (waveformView) {
        waveformView->setAudioData(oldAudioData);
    }
}

void AudioCommand::redo()
{
    if (waveformView) {
        waveformView->setAudioData(newAudioData);
    }
} 