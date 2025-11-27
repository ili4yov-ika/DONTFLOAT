#include "../include/timestretchcommand.h"
#include "../include/waveformview.h"

TimeStretchCommand::TimeStretchCommand(WaveformView* view,
                                       const QVector<QVector<float>>& oldData,
                                       const QVector<QVector<float>>& newData,
                                       const QVector<Marker>& oldMarkers,
                                       const QVector<Marker>& newMarkers,
                                       const QString& text)
    : QUndoCommand(text)
    , waveformView(view)
    , oldAudioData(oldData)
    , newAudioData(newData)
    , oldMarkers(oldMarkers)
    , newMarkers(newMarkers)
{
}

void TimeStretchCommand::undo()
{
    if (waveformView) {
        waveformView->setAudioData(oldAudioData);
        waveformView->setMarkers(oldMarkers);
    }
}

void TimeStretchCommand::redo()
{
    if (waveformView) {
        waveformView->setAudioData(newAudioData);
        waveformView->setMarkers(newMarkers);
    }
}

