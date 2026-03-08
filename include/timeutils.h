#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QtCore/QString>
#include <QtCore/qglobal.h>

/**
 * @brief Утилиты для работы со временем в аудио
 * 
 * Предоставляет методы для конвертации между сэмплами и временем,
 * а также форматирования времени для отображения.
 */
class TimeUtils
{
public:
    /**
     * @brief Конвертирует количество сэмплов в миллисекунды
     * @param samples Количество сэмплов
     * @param sampleRate Частота дискретизации (Hz)
     * @return Время в миллисекундах
     */
    static qint64 samplesToMs(qint64 samples, int sampleRate);
    
    /**
     * @brief Конвертирует количество сэмплов в секунды
     * @param samples Количество сэмплов
     * @param sampleRate Частота дискретизации (Hz)
     * @return Время в секундах (double)
     */
    static double samplesToSeconds(qint64 samples, int sampleRate);
    
    /**
     * @brief Конвертирует миллисекунды в количество сэмплов
     * @param ms Время в миллисекундах
     * @param sampleRate Частота дискретизации (Hz)
     * @return Количество сэмплов
     */
    static qint64 msToSamples(qint64 ms, int sampleRate);
    
    /**
     * @brief Конвертирует секунды в количество сэмплов
     * @param seconds Время в секундах
     * @param sampleRate Частота дискретизации (Hz)
     * @return Количество сэмплов
     */
    static qint64 secondsToSamples(double seconds, int sampleRate);
    
    /**
     * @brief Форматирует время в миллисекундах в строку MM:SS.mmm
     * @param ms Время в миллисекундах
     * @return Отформатированная строка (например, "01:23.456")
     */
    static QString formatTime(qint64 ms);
    
    /**
     * @brief Форматирует время в секундах в строку MM:SS.mmm
     * @param seconds Время в секундах
     * @return Отформатированная строка (например, "01:23.456")
     */
    static QString formatTime(double seconds);
    
    /**
     * @brief Форматирует время в миллисекундах в строку MM:SS.mmm с миллисекундами
     * @param ms Время в миллисекундах
     * @return Отформатированная строка (например, "01:23.456")
     */
    static QString formatTimeWithMs(qint64 ms);
    
    /**
     * @brief Форматирует время в миллисекундах в строку MM:SS (без миллисекунд)
     * @param ms Время в миллисекундах
     * @return Отформатированная строка (например, "01:23")
     */
    static QString formatTimeShort(qint64 ms);

    /**
     * @brief Форматирует время и позицию в тактах (MM:SS.mmm | bar.beat.sub)
     * @param msPosition Время в миллисекундах
     * @param bpm Темп (ударов в минуту)
     * @param beatsPerBar Долей в такте (4 для 4/4, 3 для 6/8)
     * @param sampleRate Частота дискретизации
     * @param gridStartSample Начальный сэмпл сетки (0 если от начала)
     * @return Строка вида "01:23.456 | 1.2.3" или "01:23.456 | --.--.--" при bpm <= 0
     */
    static QString formatTimeAndBars(qint64 msPosition, float bpm, int beatsPerBar,
                                     int sampleRate, qint64 gridStartSample);
};

#endif // TIMEUTILS_H

