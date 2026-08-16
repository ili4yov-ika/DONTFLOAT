#ifndef DONTFLOAT_SCRATCH_EDITOR_H
#define DONTFLOAT_SCRATCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../../include/bpmanalyzer.h"
#include "../../include/markerengine.h"

#include <QFutureWatcher>
#include <QString>
#include <QWidget>
#include <memory>

class WaveformView;
class QLabel;
class QPushButton;
class QTimer;

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
    void onAnalyzeBpmClicked();
    /** Анализ по приходу аудио от DAW — без нажатия кнопки. */
    void startAutoAnalysis();
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
    QPushButton* analyzeButton_ = nullptr;
    QPushButton* alignButton_ = nullptr;
    QPushButton* applyStretchButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QFutureWatcher<void>* bpmWatcher_ = nullptr;
    QFutureWatcher<void>* alignWatcher_ = nullptr;
    /** Результаты мимо QFuture::result() (см. runBpmAnalysis). */
    std::shared_ptr<BPMAnalyzer::AnalysisResult> pendingBpm_;
    std::shared_ptr<QVector<QVector<float>>> pendingAligned_;
    BPMAnalyzer::AnalysisResult lastAnalysis_;
    QTimer* autoAnalysisTimer_ = nullptr;
    bool analysisRunning_ = false;
    bool alignRunning_ = false;
    int beatsPerBar_ = 4;

    /** Пауза в потоке аудио от хоста, после которой стартует авто-анализ. */
    static constexpr int kAutoAnalysisDelayMs = 400;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_SCRATCH_EDITOR_H
