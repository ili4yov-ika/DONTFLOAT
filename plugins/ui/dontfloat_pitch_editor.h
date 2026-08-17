#ifndef DONTFLOAT_PITCH_EDITOR_H
#define DONTFLOAT_PITCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../../include/pitchdetector.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QString>
#include <QWidget>
#include <QVector>
#include <atomic>
#include <memory>

class PitchGridWidget;
class KeySelectionMenu;
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

class DontfloatPitchEditor final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatPitchEditor(QWidget* parent = nullptr,
                                  const QString& productName = QStringLiteral("DONTFLOAT Pitcher"));
    ~DontfloatPitchEditor() override;

    void setProductName(const QString& productName);
    QString productName() const { return productName_; }

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void refreshFromSession();
    void notifyHostAudioAppended();
    /** Каретка DAW (сэмплы дорожки) — синхронизирует каретку пианоролла. */
    void setHostPlayhead(qint64 samplePosition);

signals:
    void pitchSessionChanged();

private slots:
    void onAnalyzeClicked();
    /** Анализ по приходу аудио от DAW — без нажатия «Анализировать». */
    void startAutoAnalysis();
    void onApplyCorrectionClicked();
    void onExportClicked();
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

    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    QString productName_;

    PitchGridWidget* pitchGrid_ = nullptr;
    QLineEdit* keyInput_ = nullptr;
    QLineEdit* keyInput2_ = nullptr;
    KeySelectionMenu* keyMenu_ = nullptr;
    KeySelectionMenu* keyMenu2_ = nullptr;
    QWidget* analyzeOverlay_ = nullptr;
    QPushButton* analyzeButton_ = nullptr;
    QProgressBar* analyzeProgress_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

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

    /** Пауза в потоке аудио от хоста, после которой стартует авто-анализ. */
    static constexpr int kAutoAnalysisDelayMs = 400;
    /** Минимальный интервал перерисовки вида при потоке блоков от хоста. */
    static constexpr int kHostRefreshIntervalMs = 200;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PITCH_EDITOR_H
