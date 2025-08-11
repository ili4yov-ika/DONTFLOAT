#ifndef AUDIOCOMMAND_H
#define AUDIOCOMMAND_H

#include <QUndoCommand>
#include <QVector>

class WaveformView;

class AudioCommand : public QUndoCommand
{
public:
    AudioCommand(WaveformView* view, const QVector<QVector<float>>& oldData, 
                const QVector<QVector<float>>& newData, const QString& text);
    
    void undo() override;
    void redo() override;

private:
    WaveformView* waveformView;
    QVector<QVector<float>> oldAudioData;
    QVector<QVector<float>> newAudioData;
};

#endif // AUDIOCOMMAND_H 