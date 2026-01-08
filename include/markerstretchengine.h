#ifndef MARKERSTRETCHENGINE_H
#define MARKERSTRETCHENGINE_H

#include <QtCore/QVector>
#include <QUndoCommand>
#include <QString>

// Нам нужен Marker из WaveformView
#include "waveformview.h"

/**
 * @brief Результат применения сжатия/растяжения по меткам
 */
struct MarkerStretchResult
{
    QVector<QVector<float>> audioData; ///< Обработанные аудиоданные
    QVector<Marker> newMarkers;        ///< Обновлённые метки под новую длину
};

/**
 * @brief Применяет сжатие/растяжение ко всему аудио на основе меток
 *
 * Логика полностью вынесена из WaveformView::applyTimeStretch, чтобы
 * отделить обработку аудио от визуализации.
 *
 * @param audioData   Исходные аудиоданные (каналы x сэмплы)
 * @param markers     Текущие метки (position/originalPosition)
 * @param sampleRate  Частота дискретизации
 * @return MarkerStretchResult с новыми аудиоданными и метками
 */
MarkerStretchResult applyTimeStretchToMarkers(
    const QVector<QVector<float>>& audioData,
    const QVector<Marker>& markers,
    int sampleRate);

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

#endif // MARKERSTRETCHENGINE_H


