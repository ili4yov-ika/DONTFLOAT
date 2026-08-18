#ifndef DONTFLOAT_PITCH_EDITOR_H
#define DONTFLOAT_PITCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../../include/pitchdetector.h"
#include "../../include/pitchgridwidget.h"
#include "dontfloat_editor_content.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QString>
#include <QWidget>
#include <QVector>
#include <atomic>
#include <memory>

class KeySelectionMenu;
class PianoRollToolbar;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QLabel;
class QTimer;
class NotePreviewPlayer;

namespace Dontfloat::Plugins::Ui {

struct PitchAnalysisOutcome {
    Dontfloat::PluginCore::TrackPitchAnalysis pitch;
    QString primaryKeyName;
    QString secondaryKeyName;
};

class DontfloatPitchEditor final : public QWidget, public DontfloatEditorContent {
    Q_OBJECT

public:
    explicit DontfloatPitchEditor(QWidget* parent = nullptr,
                                  const QString& productName = QStringLiteral("DONTFLOAT Pitcher"));
    ~DontfloatPitchEditor() override;

    void setProductName(const QString& productName);
    QString productName() const { return productName_; }

    QWidget* widget() override { return this; }
    void bindSession(Dontfloat::PluginCore::TrackToolSession* session) override;
    void refreshFromSession();
    void notifyHostAudioAppended() override;
    /** Каретка DAW (сэмплы дорожки) — синхронизирует каретку пианоролла. */
    void setHostPlayhead(qint64 samplePosition) override;
    /** Тактовая сетка DAW: пианоролл рисует её же сетку. */
    void setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample) override;

signals:
    void pitchSessionChanged();
    /** Текст для статусбара оболочки плагина. */
    void statusMessage(const QString& text);
    /** Каретку двинули в плагине — DAW должна встать туда же. */
    void seekRequested(qint64 samplePosition);
    /** Плагин пересчитал звук — хосту стоит прогнать дорожку заново. */
    void renderedOutputChanged();

private slots:
    /** Разрез ноты по каретке / клику — как в главном окне. */
    void onNoteSplitRequested(int noteIndex, qint64 splitSample);
    void onNoteSplitRejected(PitchGridWidget::SplitRejection reason);
    /** Анализ при каждом изменении содержимого дорожки — кнопок анализа нет. */
    void startAutoAnalysis();
    void onApplyCorrectionClicked();
    /** Экспорт нот пианоролла в .mid (кнопка справа на панели). */
    void onExportMidiClicked();
    void onPitchAnalysisFinished();
    void onPrimaryKeySelected(const QString& key);
    void onSecondaryKeySelected(const QString& key);
    void onNotePitchEdited(int noteIndex, float oldPitch, float newPitch);
    void onNotePreviewRequested(int noteIndex);
    void onNotePreviewPitchChanged(int noteIndex, float midiPitch);
    void onNotePreviewStopped();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setPrimaryKey(const QString& key);
    void setSecondaryKey(const QString& key);
    void setAnalysisRunning(bool running);
    void layoutAnalyzeOverlay();
    void refreshPitchGrid();
    /** Подгоняет диапазон высот пианоролла под найденные ноты. */
    void fitPitchRangeToNotes();
    void runPitchAnalysis();
    void syncNotesToSession();
    void setStatus(const QString& text);
    /** Сдвиг нот вслед за переехавшим клипом (см. detectContentShift). */
    void shiftNotes(qint64 deltaSamples);

    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    QString productName_;

    PitchGridWidget* pitchGrid_ = nullptr;
    QLineEdit* keyInput_ = nullptr;
    QLineEdit* keyInput2_ = nullptr;
    KeySelectionMenu* keyMenu_ = nullptr;
    KeySelectionMenu* keyMenu2_ = nullptr;
    QWidget* analyzeOverlay_ = nullptr;
    QProgressBar* analyzeProgress_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    PianoRollToolbar* pianoRollToolbar_ = nullptr;

    QString primaryKey_;
    QString secondaryKey_;
    QVector<PitchDetector::PitchNote> baseNotes_;

    QFutureWatcher<void>* analysisWatcher_ = nullptr;
    /** Результат анализа: мимо QFuture::result() (см. runPitchAnalysis). */
    std::shared_ptr<PitchAnalysisOutcome> pendingOutcome_;
    std::shared_ptr<std::atomic<int>> analysisProgress_;
    NotePreviewPlayer* notePreviewPlayer_ = nullptr;
    QTimer* autoAnalysisTimer_ = nullptr;
    QElapsedTimer hostRefreshClock_;
    bool analysisRunning_ = false;
    /** Идёт применение каретки от DAW — обратно её не отправляем. */
    bool applyingHostPlayhead_ = false;
    /** Отпечаток содержимого, по которому считался последний анализ. */
    Dontfloat::PluginCore::TrackContentFingerprint analyzedContent_;
    /** Тактовая сетка, пришедшая от DAW (см. setHostBeatGrid). */
    double hostBpm_ = 0.0;
    int hostBeatsPerBar_ = 4;
    qint64 hostGridStartSample_ = 0;

    /** Пауза в потоке аудио от хоста, после которой стартует авто-анализ. */
    static constexpr int kAutoAnalysisDelayMs = 400;
    /** Минимальный интервал перерисовки вида при потоке блоков от хоста. */
    static constexpr int kHostRefreshIntervalMs = 200;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PITCH_EDITOR_H
