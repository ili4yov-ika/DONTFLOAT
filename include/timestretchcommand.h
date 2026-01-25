#ifndef TIMESTRETCHCOMMAND_H
#define TIMESTRETCHCOMMAND_H

#include <QUndoCommand>
#include <QVector>
#include <QString>
#include "waveformview.h"
#include "markerengine.h"

/**
 * @brief Команда для undo/redo применения time stretch по меткам
 *
 * Сохраняет состояние аудио и меток до и после применения растяжения/сжатия.
 */
class TimeStretchCommand : public QUndoCommand
{
public:
    /**
     * @brief Конструктор команды
     * @param view          Указатель на WaveformView
     * @param oldData       Аудиоданные до применения эффекта
     * @param newData       Аудиоданные после применения эффекта
     * @param oldMarkers    Метки до применения эффекта
     * @param newMarkers    Метки после применения эффекта
     * @param text          Текст команды для UI (например, "Применить сжатие-растяжение")
     * @param parent        Родительская команда (для группировки)
     */
    TimeStretchCommand(WaveformView* view,
                      const QVector<QVector<float>>& oldData,
                      const QVector<QVector<float>>& newData,
                      const QVector<Marker>& oldMarkers,
                      const QVector<Marker>& newMarkers,
                      const QString& text,
                      QUndoCommand* parent = nullptr);

    /**
     * @brief Отменить команду (вернуть старое состояние)
     */
    void undo() override;

    /**
     * @brief Повторить команду (применить новое состояние)
     */
    void redo() override;

private:
    WaveformView* waveformView;
    QVector<QVector<float>> oldAudioData;
    QVector<QVector<float>> newAudioData;
    QVector<Marker> oldMarkerData;
    QVector<Marker> newMarkerData;
};

#endif // TIMESTRETCHCOMMAND_H
