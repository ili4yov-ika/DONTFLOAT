#ifndef MIDIEXPORTER_H
#define MIDIEXPORTER_H

/**
 * Экспорт нот пианоролла в стандартный MIDI-файл (SMF, формат 0).
 *
 * Ноты приходят в сэмплах исходного аудио; в тики переводим по темпу и
 * частоте дискретизации, темп пишем мета-событием — так файл открывается в
 * любой DAW с той же сеткой, что в DONTFLOAT.
 */

#include <QtCore/QString>
#include <QtCore/QVector>

#include "pitchdetector.h"

namespace MidiExporter {

/** Разрешение файла: тиков на четверть (стандартное значение секвенсоров). */
constexpr int kTicksPerQuarter = 480;

struct Options {
    float bpm = 120.0f;
    int sampleRate = 44100;
    /** Смещение начала (сэмплы): позиция, которая станет нулём файла. */
    qint64 startSample = 0;
    int velocity = 96;
    /** Канал MIDI (0-15). */
    int channel = 0;
};

/**
 * Пишет ноты в файл \a path.
 * @return false и текст в \a error, если писать нечего или файл не открылся.
 */
bool writeFile(const QString& path,
               const QVector<PitchDetector::PitchNote>& notes,
               const Options& options,
               QString* error = nullptr);

/** То же, но в память — используется тестами и экспортом из плагина. */
QByteArray buildFile(const QVector<PitchDetector::PitchNote>& notes,
                     const Options& options);

} // namespace MidiExporter

#endif // MIDIEXPORTER_H
