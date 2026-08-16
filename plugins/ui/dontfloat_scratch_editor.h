#ifndef DONTFLOAT_SCRATCH_EDITOR_H
#define DONTFLOAT_SCRATCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../../include/bpmanalyzer.h"
#include "../../include/markerengine.h"

#include <QFutureWatcher>
#include <QString>
#include <QWidget>

class WaveformView;
class QLabel;
class QPushButton;

namespace Dontfloat::Plugins::Ui {

class DontfloatScratchEditor final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatScratchEditor(QWidget* parent = nullptr,
                                    const QString& productName = QStringLiteral("DONTFLOAT Scratch"));

    void setProductName(const QString& productName);
    QString productName() const { return productName_; }

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void refreshFromSession();
    void notifyHostAudioAppended();

private slots:
    void onImportAudioClicked();
    void onAnalyzeBpmClicked();
    void onAlignBeatsClicked();
    void onApplyStretchClicked();
    void onExportClicked();
    void onBpmAnalysisFinished();
    void onAlignFinished();
    void onMarkersChanged();

private:
    void refreshWaveform();
    void runBpmAnalysis();
    void runBeatAlign();
    void setStatus(const QString& text);
    void updateActionButtons();
    QVector<Marker> makeAlignedBeatMarkers(const QVector<BPMAnalyzer::BeatInfo>& beats,
                                           qint64 totalSamples,
                                           int sampleRate) const;
    void writeChannelsToSession(const QVector<QVector<float>>& channels, int sampleRate);

    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    QString productName_;
    WaveformView* waveform_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QPushButton* analyzeButton_ = nullptr;
    QPushButton* alignButton_ = nullptr;
    QPushButton* applyStretchButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QFutureWatcher<BPMAnalyzer::AnalysisResult>* bpmWatcher_ = nullptr;
    QFutureWatcher<QVector<QVector<float>>>* alignWatcher_ = nullptr;
    BPMAnalyzer::AnalysisResult lastAnalysis_;
    bool analysisRunning_ = false;
    bool alignRunning_ = false;
    int beatsPerBar_ = 4;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_SCRATCH_EDITOR_H
