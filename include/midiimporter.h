#ifndef MIDIIMPORTER_H
#define MIDIIMPORTER_H

/**
 * Импорт референсного MIDI: ноты из .mid ложатся на пианоролл серым фоном,
 * чтобы сверять с ними разбор аудио.
 *
 * Тайминг задаётся при импорте (диалог с тремя кнопками):
 * оставить как есть, подогнать под BPM проекта или ещё и выровнять по сетке.
 */

#include <QtCore/QString>
#include <QtCore/QVector>

#include "keyanalyzer.h"
#include "pitchdetector.h"

namespace MidiImporter {

/** Как раскладывать ноты файла по времени проекта. */
enum class TimingMode {
    KeepAsIs,        ///< как в файле: по его собственному темпу
    FitToBpm,        ///< перетянуть под BPM проекта
    AlignAndFitToBpm ///< перетянуть под BPM и сдвинуть к началу тактовой сетки
};

struct Options {
    TimingMode mode = TimingMode::KeepAsIs;
    float projectBpm = 120.0f;
    int sampleRate = 44100;
    /** Начало тактовой сетки проекта — точка выравнивания. */
    qint64 gridStartSample = 0;
};

struct Result {
    QVector<PitchDetector::PitchNote> notes;  ///< в сэмплах проекта
    float sourceBpm = 0.0f;                   ///< темп из файла (мета-событие)
    int ticksPerQuarter = 480;
    bool ok = false;
    QString error;
};

/** Читает SMF и раскладывает ноты по времени согласно \a options. */
Result readFile(const QString& path, const Options& options);

/**
 * Тональность набора нот: хрома-вектор по длительностям звучания и тот же
 * разбор, что у аудио (`KeyAnalyzer::detectKeyFromChroma`).
 * @return например «C Major»; пустая строка, если нот нет.
 */
QString detectKey(const QVector<PitchDetector::PitchNote>& notes);

/**
 * Потактовая тональность референса: по каждому такту сетки \a grid строится
 * хрома из длительностей звучащих в нём нот, соседние такты с одной
 * тональностью сливаются в регионы — как у потактового анализа звука
 * (`KeyAnalyzer::analyzeKeyPerBar`), поэтому модуляции показываются той же
 * полосой полей над пианороллом.
 *
 * Такты без нот наследуют тональность предыдущего такта (пауза не модулирует);
 * такты до первой ноты и после последней не разбираются вовсе.
 */
KeyAnalyzer::PerBarKeyResult analyzeKeyPerBar(const QVector<PitchDetector::PitchNote>& notes,
                                              const KeyAnalyzer::BarGrid& grid,
                                              int sampleRate);

} // namespace MidiImporter

#endif // MIDIIMPORTER_H
