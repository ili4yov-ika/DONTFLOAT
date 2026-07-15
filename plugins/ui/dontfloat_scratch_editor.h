#ifndef DONTFLOAT_SCRATCH_EDITOR_H
#define DONTFLOAT_SCRATCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"

#include <QFutureWatcher>
#include <QWidget>

class WaveformView;
class QLabel;
class QPushButton;

namespace Dontfloat::Plugins::Ui {

class DontfloatScratchEditor final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatScratchEditor(QWidget* parent = nullptr);

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void refreshFromSession();
    void notifyHostAudioAppended();

private slots:
    void onImportAudioClicked();
    void onAnalyzeBpmClicked();
    void onBpmAnalysisFinished();

private:
    void refreshWaveform();
    void runBpmAnalysis();

    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    WaveformView* waveform_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QPushButton* analyzeButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QFutureWatcher<float>* bpmWatcher_ = nullptr;
    bool analysisRunning_ = false;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_SCRATCH_EDITOR_H
