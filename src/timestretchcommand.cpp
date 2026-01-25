#include "../include/timestretchcommand.h"

TimeStretchCommand::TimeStretchCommand(WaveformView* view,
                                     const QVector<QVector<float>>& oldData,
                                     const QVector<QVector<float>>& newData,
                                     const QVector<Marker>& oldMarkers,
                                     const QVector<Marker>& newMarkers,
                                     const QString& text,
                                     QUndoCommand* parent)
    : QUndoCommand(parent)
    , waveformView(view)
    , oldAudioData(oldData)
    , newAudioData(newData)
    , oldMarkerData(oldMarkers)
    , newMarkerData(newMarkers)
{
    setText(text);
}

void TimeStretchCommand::undo()
{
    if (!waveformView) {
        return;
    }

    // Восстанавливаем старые аудиоданные
    waveformView->setAudioData(oldAudioData);

    // Восстанавливаем старые метки
    waveformView->setMarkers(oldMarkerData);

    // Обновляем originalAudioData через updateOriginalData()
    waveformView->updateOriginalData(oldAudioData);

    // Обновляем визуализацию
    waveformView->update();
}

void TimeStretchCommand::redo()
{
    if (!waveformView) {
        return;
    }

    // Применяем новые аудиоданные
    waveformView->setAudioData(newAudioData);

    // Применяем новые метки
    waveformView->setMarkers(newMarkerData);

    // Обновляем originalAudioData через updateOriginalData()
    waveformView->updateOriginalData(newAudioData);

    // Обновляем визуализацию
    waveformView->update();
}
