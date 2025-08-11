#ifndef MIXXXBPMANALYZER_H
#define MIXXXBPMANALYZER_H

#include <QVector>

struct MixxxBeatAnalysis {
    float bpm;                        // оцененный BPM (если доступен)
    QVector<int> beatPositionsFrames; // позиции битов в фреймах (frame = samples per channel)
    bool supportsBeatTracking;        // true если доступны beatPositionsFrames
};

class MixxxBpmAnalyzerFacade {
public:
    // samplesInterleaved: стерео interleaved float32 [-1..1], длина = frames * 2
    // sampleRate: Гц
    // Возвращает bpm и/или beat grid от QM-DSP плагина
    static MixxxBeatAnalysis analyzeStereoInterleaved(const QVector<float>& samplesInterleaved,
                                                      int sampleRate,
                                                      bool fastAnalysis = false);
};

#endif // MIXXXBPMANALYZER_H