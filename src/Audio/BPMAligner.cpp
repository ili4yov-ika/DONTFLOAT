#include "BPMAligner.h"
#include <QDebug>

// Заглушка: возвращает данные без изменений
AudioData BPMAligner::alignBPM(const AudioData &inputData, float targetBPM) {
    qDebug() << "Aligning to BPM:" << targetBPM;
    return inputData; // пока ничего не делает
}
