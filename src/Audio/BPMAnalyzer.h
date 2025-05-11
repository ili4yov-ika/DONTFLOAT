#pragma once

#include "AudioLoader.h"

class BPMAnalyzer {
public:
    // Анализ BPM: возвращает примерное значение и устанавливает флаг стабильности
    static float estimateBPM(const AudioData &data, bool &isStable);
};
