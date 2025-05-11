#include "BPMAnalyzer.h"
#include <QDebug>
#include <cmath>

// Заглушка: имитация анализа
float BPMAnalyzer::estimateBPM(const AudioData &data, bool &isStable) {
    qDebug() << "Estimating BPM...";

    if (data.samples.empty()) {
        isStable = false;
        return 0.0f;
    }

    // Здесь будет логика анализа: пока просто фейк
    float simulatedBPM = 121.7f;

    // Простейшая симуляция нестабильности BPM (например, если слишком мал сигнал)
    isStable = (data.samples.size() > 44100 * 5); // >5 секунд
    return simulatedBPM;
}
