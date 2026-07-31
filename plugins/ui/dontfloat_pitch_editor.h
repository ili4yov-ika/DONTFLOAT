#ifndef DONTFLOAT_PITCH_EDITOR_H
#define DONTFLOAT_PITCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../../include/pitchdetector.h"

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
    void setHostPlayhead(qint64 positionMs, bool playing);

signals:
    void pitchSessionChanged();

private slots:
    void onImportAudioClicked();
    void onAnalyzeClicked();
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
    QPushButton* importButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QString primaryKey_;
    QString secondaryKey_;
    QVector<PitchDetector::PitchNote> baseNotes_;

    QFutureWatcher<PitchAnalysisOutcome>* analysisWatcher_ = nullptr;
    std::shared_ptr<std::atomic<int>> analysisProgress_;
    NotePreviewPlayer* notePreviewPlayer_ = nullptr;
    bool analysisRunning_ = false;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PITCH_EDITOR_H
