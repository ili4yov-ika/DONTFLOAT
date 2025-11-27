#ifndef TIMESTRETCHCOMMAND_H
#define TIMESTRETCHCOMMAND_H

#include <QUndoCommand>
#include <QVector>

// Forward declaration
class WaveformView;
struct Marker;

/**
 * @brief Команда для применения сжатия-растяжения аудио с тонкомпенсацией
 * 
 * Применяет изменения времени (time stretching) к сегментам аудио между метками
 * с сохранением высоты тона (pitch correction).
 */
class TimeStretchCommand : public QUndoCommand
{
public:
    /**
     * @brief Конструктор команды
     * @param view Указатель на WaveformView
     * @param oldData Исходные аудиоданные
     * @param newData Обработанные аудиоданные
     * @param oldMarkers Исходные метки
     * @param newMarkers Метки после применения изменений
     * @param text Описание команды
     */
    TimeStretchCommand(WaveformView* view,
                      const QVector<QVector<float>>& oldData,
                      const QVector<QVector<float>>& newData,
                      const QVector<Marker>& oldMarkers,
                      const QVector<Marker>& newMarkers,
                      const QString& text);

    void undo() override;
    void redo() override;

private:
    WaveformView* waveformView;
    QVector<QVector<float>> oldAudioData;
    QVector<QVector<float>> newAudioData;
    QVector<Marker> oldMarkers;
    QVector<Marker> newMarkers;
};

#endif // TIMESTRETCHCOMMAND_H

