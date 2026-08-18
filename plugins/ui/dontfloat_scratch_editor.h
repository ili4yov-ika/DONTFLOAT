#ifndef DONTFLOAT_SCRATCH_EDITOR_H
#define DONTFLOAT_SCRATCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../../include/bpmanalyzer.h"
#include "../../include/markerengine.h"
#include "dontfloat_editor_content.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QString>
#include <QWidget>
#include <memory>

class WaveformView;
class QLabel;
class QPushButton;
class QScrollBar;
class QTimer;

namespace Dontfloat::Plugins::Ui {

class DontfloatScratchEditor final : public QWidget, public DontfloatEditorContent {
    Q_OBJECT

public:
    explicit DontfloatScratchEditor(QWidget* parent = nullptr,
                                    const QString& productName = QStringLiteral("DONTFLOAT Scratch"));

    void setProductName(const QString& productName);
    QString productName() const { return productName_; }

    QWidget* widget() override { return this; }
    void bindSession(Dontfloat::PluginCore::TrackToolSession* session) override;
    void refreshFromSession();
    void notifyHostAudioAppended() override;
    /** Каретка DAW (сэмплы дорожки) — синхронизирует каретку волны. */
    void setHostPlayhead(qint64 samplePosition) override;
    /** Тактовая сетка DAW: волна рисует её же сетку. */
    void setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample) override;

    // Инструменты волны из шапки (те же действия, что в главном окне)
    bool hasWaveformTools() const override { return true; }
    void shiftBeatGrid(int beats) override;
    void snapMarkersToGrid() override;
    void detectOnsetMarkers() override;
    void setLoopBoundAtPlayhead(bool start) override;
    void setLoopEnabled(bool enabled) override;
    bool loopRegionMs(qint64* startMs, qint64* endMs) const override;

signals:
    /** Текст для статусбара оболочки плагина. */
    void statusMessage(const QString& text);
    /** Каретку двинули в плагине — DAW должна встать туда же. */
    void seekRequested(qint64 samplePosition);

private slots:
    /** Анализ при каждом изменении содержимого дорожки — кнопок анализа нет. */
    void startAutoAnalysis();
    void onAlignBeatsClicked();
    void onApplyStretchClicked();
    void onBpmAnalysisFinished();
    void onAlignFinished();
    void onMarkersChanged();

private:
    void refreshWaveform();
    void runBpmAnalysis();
    void runBeatAlign();
    void setStatus(const QString& text);
    void updateActionButtons();
    /** Сдвиг меток, сетки и точек цикла вслед за переехавшим клипом. */
    void shiftAnnotations(qint64 deltaSamples);
    qint64 samplesToMs(qint64 samples) const;
    QVector<Marker> makeAlignedBeatMarkers(const QVector<BPMAnalyzer::BeatInfo>& beats,
                                           qint64 totalSamples,
                                           int sampleRate) const;
    void writeChannelsToSession(const QVector<QVector<float>>& channels, int sampleRate);

    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    QString productName_;
    WaveformView* waveform_ = nullptr;
    QPushButton* alignButton_ = nullptr;
    QPushButton* applyStretchButton_ = nullptr;
    QScrollBar* horizontalScrollBar_ = nullptr;
    QFutureWatcher<void>* bpmWatcher_ = nullptr;
    QFutureWatcher<void>* alignWatcher_ = nullptr;
    /** Результаты мимо QFuture::result() (см. runBpmAnalysis). */
    std::shared_ptr<BPMAnalyzer::AnalysisResult> pendingBpm_;
    std::shared_ptr<QVector<QVector<float>>> pendingAligned_;
    BPMAnalyzer::AnalysisResult lastAnalysis_;
    QTimer* autoAnalysisTimer_ = nullptr;
    QElapsedTimer hostRefreshClock_;
    bool analysisRunning_ = false;
    bool alignRunning_ = false;
    /** Идёт применение каретки от DAW — обратно её не отправляем. */
    bool applyingHostPlayhead_ = false;
    int beatsPerBar_ = 4;
    /** Точки цикла (мс) и его состояние — как A/B в главном окне. */
    qint64 loopStartMs_ = -1;
    qint64 loopEndMs_ = -1;
    bool loopEnabled_ = false;
    /** Отпечаток содержимого, по которому считался последний анализ. */
    Dontfloat::PluginCore::TrackContentFingerprint analyzedContent_;

    /** Пауза в потоке аудио от хоста, после которой стартует авто-анализ. */
    static constexpr int kAutoAnalysisDelayMs = 400;
    /** Минимальный интервал перерисовки волны при потоке блоков от хоста. */
    static constexpr int kHostRefreshIntervalMs = 200;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_SCRATCH_EDITOR_H
