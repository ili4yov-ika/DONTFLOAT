#include "../include/timestretchcommand.h"
#include "../include/waveformview.h"
#include <utility>

TimeStretchCommand::TimeStretchCommand(WaveformView* view,
                                       const QVector<QVector<float>>& oldData,
                                       const QVector<QVector<float>>& newData,
                                       const QVector<Marker>& oldMarkers,
                                       const QVector<Marker>& newMarkers,
                                       const QString& text,
                                       ApplyCallback callback)
    : QUndoCommand(text)
    , waveformView(view)
    , oldAudioData(oldData)
    , newAudioData(newData)
    , oldMarkers(oldMarkers)
    , newMarkers(newMarkers)
    , applyCallback(std::move(callback))
{
}

void TimeStretchCommand::undo()
{
    applyState(oldAudioData, oldMarkers);
}

void TimeStretchCommand::redo()
{
    applyState(newAudioData, newMarkers);
}

void TimeStretchCommand::applyState(const QVector<QVector<float>>& data,
                                    const QVector<Marker>& markers)
{
    if (!waveformView) {
        return;
    }

    waveformView->setAudioData(data);
    waveformView->setMarkers(markers);

    if (applyCallback) {
        applyCallback(data, markers);
    }
}

