#ifndef MARKERENGINE_H
#define MARKERENGINE_H

#include <QtCore/QPoint>
#include <QtCore/QtGlobal>
#include "timeutils.h"

/**
 * @brief Базовая структура метки для обработки аудио
 *
 * Содержит только данные, необходимые для обработки аудио с растяжением/сжатием.
 * Не содержит UI-специфичных полей.
 */
struct MarkerData {
    qint64 position;           ///< Текущая позиция метки в сэмплах
    qint64 originalPosition;   ///< Исходная позиция метки (для вычисления коэффициента)
    qint64 timeMs;             ///< Текущая позиция во времени (миллисекунды)
    qint64 originalTimeMs;     ///< Исходная позиция во времени (миллисекунды)
    bool isFixed;              ///< Флаг неподвижности метки (нельзя перетаскивать)
    bool isEndMarker;          ///< Флаг конечной метки (особая метка в конце таймлайна)

    // Конструкторы
    MarkerData();
    MarkerData(qint64 pos, int sampleRate = 44100);
    MarkerData(qint64 pos, bool fixed, int sampleRate = 44100);
    MarkerData(qint64 pos, bool fixed, bool endMarker, int sampleRate = 44100);

    // Методы конвертации времени и сэмплов
    void updateTimeFromSamples(int sampleRate);
    void updateSamplesFromTime(int sampleRate);
};

/**
 * @brief Расширенная метка с UI-состоянием для WaveformView
 *
 * Наследуется от MarkerData и добавляет поля для UI-взаимодействия.
 */
struct Marker : public MarkerData {
    // UI-специфичные поля
    bool isDragging;           ///< Флаг перетаскивания
    QPoint dragStartPos;       ///< Начальная позиция перетаскивания (в пикселях)
    qint64 dragStartSample;    ///< Начальный сэмпл при перетаскивании

    // Конструкторы
    Marker();
    Marker(qint64 pos, int sampleRate = 44100);
    Marker(qint64 pos, bool fixed, int sampleRate = 44100);
    Marker(qint64 pos, bool fixed, bool endMarker, int sampleRate = 44100);
};

#endif // MARKERENGINE_H
