#ifndef DONTFLOAT_PITCH_EDITOR_H
#define DONTFLOAT_PITCH_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "../core/dontfloat_shared_notes.h"
#include "../../include/keyanalyzer.h"
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

class KeyModulationStrip;
class QUndoStack;
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
    /**
     * Каретка из соседней половины окна (сэмплы **источника**).
     *
     * Клик по волне обязан двигать и каретку пианоролла: на стоящем
     * транспорте хост позицию обратно не присылает, и половины окна
     * оставались в разных местах.
     */
    void applySourcePlayhead(qint64 sourceSample);
#if defined(DONTFLOAT_WITH_ARA)
    /** Экземпляр привязан к документу ARA — работаем с моделью, а не с захватом. */
    void setAraBinding(const void* extension) override;
#endif
    // Общий вид таймлайна с волной к ARA отношения не имеет: половины окна
    // держат один масштаб и в сборке без SDK. Пока объявления прятались под
    // #if, такая сборка не собиралась вовсе
    /** Ставит масштаб таймлайна, не рассылая сигнал обратно. */
    void applyTimelineZoom(float zoom);
    /** Ставит прокрутку таймлайна, не рассылая сигнал обратно. */
    void applyTimelineOffset(float offset);
    /** Длина таймлайна в сэмплах — чтобы сетка пианоролла совпала с волной. */
    void applyTimelineSampleCount(qint64 samples);

signals:
    void pitchSessionChanged();
    /** Текст для статусбара оболочки плагина. */
    void statusMessage(const QString& text);
    /** Каретку двинули в плагине — DAW должна встать туда же. */
    void seekRequested(qint64 samplePosition);
    /** Плагин пересчитал звук — хосту стоит прогнать дорожку заново. */
    void renderedOutputChanged();
    /** Прокрутку таймлайна сменили здесь — волна должна повторить. */
    void timelineOffsetChanged(float offset);
    /** Колесо зума над пианороллом: масштаб задаёт волна, мы лишь просим. */
    void timelineZoomRequested(int angleDeltaY, float timelinePixelX);

private slots:
    /** Разрез ноты по каретке / клику — как в главном окне. */
    void onNoteSplitRequested(int noteIndex, qint64 splitSample);
    void onNoteSplitRejected(PitchGridWidget::SplitRejection reason);
    /** Анализ при каждом изменении содержимого дорожки — кнопок анализа нет. */
    void startAutoAnalysis();
    void onApplyCorrectionClicked();
    /** Экспорт нот пианоролла в .mid (кнопка справа на панели). */
    void onExportMidiClicked();
    /** Импорт референсного MIDI: серые ноты на фоне пианоролла. */
    void onImportMidiClicked();
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
    /** Полоса референса стоит на таймлайне пианоролла — держим их вместе. */
    void syncReferenceKeyStrip();
    /** Применяет ноты после отмены/повтора: вид, сессия и кнопка коррекции. */
    void applyNotesAfterUndo();
    /** Кладёт ноты фоном и пересчитывает потактовые тональности референса. */
    void applyReferenceNotes(const QVector<PitchDetector::PitchNote>& notes, int sampleRate);
    /** Выкладывает свои ноты соседним экземплярам плагина (общая доска). */
    void publishNotesToBoard();
    /** Забирает ноты соседней дорожки, если они изменились. */
    void pullSharedReferenceNotes();
    /**
     * Просит DAW встать в позицию (сэмплы источника).
     *
     * Каретку под ARA ведёт хост: пока он не переставлен, вторая половина
     * окна и волна в DAW остаются там, где были.
     */
    bool requestHostSeek(qint64 sourceSample);
#if defined(DONTFLOAT_WITH_ARA)
    /**
     * Тянет из модели ARA: свои ноты, тактовую сетку хоста и ноты соседней
     * дорожки референсом. Возвращает false, если экземпляр к ARA не привязан.
     */
    bool pullFromAraModel();
#endif

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
    /** Потактовая панель тональностей референса (под своей). */
    KeyModulationStrip* referenceKeyStrip_ = nullptr;
    KeyAnalyzer::PerBarKeyResult referenceKeys_;
    /** Референс пришёл из файла .mid — нотами соседней дорожки его не трогаем. */
    bool referenceFromImport_ = false;
    /** Место на общей доске нот: свои публикуем, чужие берём референсом. */
    std::uint64_t instanceId_ = 0;
    std::uint64_t appliedSharedRevision_ = 0;
    QTimer* sharedNotesTimer_ = nullptr;
#if defined(DONTFLOAT_WITH_ARA)
    /** ARA::PlugIn::PlugInExtension этого экземпляра (nullptr — режим без ARA). */
    const void* araBinding_ = nullptr;
    /** Ревизия модели ARA, на которой мы последний раз обновлялись. */
    std::uint64_t appliedAraRevision_ = 0;
    /** Звук из ARA уже отдан сессии — второй раз не копируем. */
    bool araAudioApplied_ = false;
    /** Шла ли в прошлый опрос разборка: по ней прячем плашку прогресса. */
    bool araAnalysisWasRunning_ = false;
#endif

    QString primaryKey_;
    QString secondaryKey_;
    QVector<PitchDetector::PitchNote> baseNotes_;

    /** Отмена/повтор правок нот внутри плагина (Ctrl+Z / Ctrl+Y). */
    QUndoStack* undoStack_ = nullptr;

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
    /** Идёт применение чужого масштаба — обратно его не рассылаем. */
    bool applyingTimelineView_ = false;
    /**
     * Размещение клипа на таймлайне DAW.
     *
     * Каретка приходит во времени проекта, а пианоролл живёт во времени
     * дорожки. Без пересчёта каретки волны и пианоролла стоят в разных
     * координатах и при воспроизведении расходятся.
     */
    bool araClipValid_ = false;
    double araClipStartPlaybackSec_ = 0.0;
    double araClipStartSourceSec_ = 0.0;
    double araClipStretch_ = 1.0;
    /** Минимальный интервал перерисовки вида при потоке блоков от хоста. */
    static constexpr int kHostRefreshIntervalMs = 200;
    /** Как часто спрашиваем доску, не появились ли ноты на соседней дорожке. */
    static constexpr int kSharedNotesPollMs = 700;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PITCH_EDITOR_H
