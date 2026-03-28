#include "../include/beatfixcommand.h"
#include <QtWidgets>
#include <numeric>

BeatFixCommand::BeatFixCommand(WaveformView* view,
                             const QVector<QVector<float>>& originalData,
                             const QVector<QVector<float>>& fixedData,
                             float bpm,
                             const QVector<BPMAnalyzer::BeatInfo>& beats,
                             qint64 gridStartSample,
                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , waveformView(view)
    , originalAudioData(originalData)
    , fixedAudioData(fixedData)
    , bpmValue(bpm)
    , beatInfo(beats)
    , gridStartSampleValue(gridStartSample)
{
    setText(QObject::tr("Выравнивание долей"));
}

void BeatFixCommand::undo()
{
    if (waveformView) {
        waveformView->setAudioData(originalAudioData);
        waveformView->setBeatInfo(beatInfo);
        waveformView->setGridStartSample(gridStartSampleValue);
        waveformView->setBPM(bpmValue);
        waveformView->setBeatsAligned(false); // Отменяем выравнивание
    }
}

void BeatFixCommand::redo()
{
    if (waveformView) {
        waveformView->setAudioData(fixedAudioData);
        waveformView->setBeatInfo(beatInfo);
        waveformView->setGridStartSample(gridStartSampleValue);
        // Если доли были неровными, пересчитываем средний BPM по разметке
        if (beatInfo.size() > 2) {
            QVector<float> intervals;
            intervals.reserve(beatInfo.size() - 1);
            for (int i = 1; i < beatInfo.size(); ++i) {
                intervals.append(float(beatInfo[i].position - beatInfo[i - 1].position));
            }
            // Среднее арифметическое интервалов между долями
            double sum = 0.0;
            for (float v : intervals) sum += v;
            const double avgInterval = sum / qMax(1, intervals.size());
            const int sr = waveformView->getSampleRate();
            if (avgInterval > 0 && sr > 0) {
                bpmValue = float(60.0 * double(sr) / avgInterval);
            }
        }
        waveformView->setBPM(bpmValue);
        waveformView->setBeatsAligned(true); // Применяем выравнивание
    }
}
