#include "dontfloat_pitch_editor.h"

#include "../../include/audiofileservice.h"
#include "../../include/keyanalyzer.h"
#include "../../include/keymodulationstrip.h"
#include "../core/dontfloat_diagnostics.h"
#if defined(DONTFLOAT_WITH_ARA)
#include "../ara/dontfloat_ara_document_controller.h"
#endif
#include "../../include/keyselectionmenu.h"
#include "../../include/notepreviewplayer.h"
#include "../../include/pitchcorrection.h"
#include "../../include/pitchnoteeditcommand.h"
#include "../../include/pitchnotesplitcommand.h"
#include "../../include/pitchnotemovecommand.h"
#include "../../include/pianoroll_engine.h"
#include "../../include/pianoroll_toolbar.h"
#include "../../include/pitchdetector.h"
#include "../../include/midiexporter.h"
#include "../../include/midiimporter.h"
#include "../../include/pitchgridwidget.h"
#include "../../include/uiconstants.h"

#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QUndoStack>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Dontfloat::Plugins::Ui {
namespace {

using Dontfloat::PluginCore::TrackAudioBuffer;
using Dontfloat::PluginCore::TrackPitchAnalysis;
using Dontfloat::PluginCore::TrackPitchNote;
using Dontfloat::PluginCore::TrackToolSession;

TrackPitchNote toCoreNote(const PitchDetector::PitchNote& note)
{
    TrackPitchNote out;
    out.startSample = note.startSample;
    out.endSample = note.endSample;
    out.midiPitch = note.midiPitch;
    out.detectedPitch = note.detectedPitch;
    out.confidence = note.confidence;
    // Без этого перенос ноты терялся бы на круге «редактор → сессия → редактор»
    out.sourceStartSample = note.sourceStartSample;
    out.sourceEndSample = note.sourceEndSample;
    return out;
}

PitchDetector::PitchNote fromCoreNote(const TrackPitchNote& note)
{
    PitchDetector::PitchNote out;
    out.startSample = note.startSample;
    out.endSample = note.endSample;
    out.midiPitch = note.midiPitch;
    out.detectedPitch = note.detectedPitch;
    out.confidence = note.confidence;
    out.sourceStartSample = note.sourceStartSample;
    out.sourceEndSample = note.sourceEndSample;
    return out;
}

QVector<PitchDetector::PitchNote> fromCoreNotes(const std::vector<TrackPitchNote>& notes)
{
    QVector<PitchDetector::PitchNote> out;
    out.reserve(static_cast<int>(notes.size()));
    for (const TrackPitchNote& note : notes) {
        out.append(fromCoreNote(note));
    }
    return out;
}

std::vector<TrackPitchNote> toCoreNotes(const QVector<PitchDetector::PitchNote>& notes)
{
    std::vector<TrackPitchNote> out;
    out.reserve(static_cast<std::size_t>(notes.size()));
    for (const PitchDetector::PitchNote& note : notes) {
        out.push_back(toCoreNote(note));
    }
    return out;
}

QString keyNameFromInfo(const KeyAnalyzer::KeyInfo& info)
{
    if (info.key == KeyAnalyzer::UNKNOWN_KEY || info.keyName.isEmpty()) {
        return QStringLiteral("C Major");
    }
    return info.keyName;
}

QVector<float> toQVector(const std::vector<float>& samples)
{
    QVector<float> out(static_cast<int>(samples.size()));
    if (!samples.empty()) {
        std::copy(samples.begin(), samples.end(), out.begin());
    }
    return out;
}

std::vector<float> toStdVector(const QVector<float>& samples)
{
    return std::vector<float>(samples.begin(), samples.end());
}

// QString::toStdString / fromStdString в отладочной сборке ходят в Qt6Core.dll,
// собранную с release-рантаймом, и возвращают мусор. Переводим через UTF-8 сами.
std::string toUtf8String(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString fromUtf8String(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

DontfloatPitchEditor::DontfloatPitchEditor(QWidget* parent, const QString& productName)
    : QWidget(parent)
    , productName_(productName)
    , analysisWatcher_(new QFutureWatcher<void>(this))
    , notePreviewPlayer_(new NotePreviewPlayer(this))
{
    setObjectName(QStringLiteral("dontfloatPitchEditor"));
    setMinimumSize(480, 220);
    setWindowTitle(productName_);

    // Разметка как в главном окне: поля тональностей над пианороллом, под ним
    // панель разреза (макет MARKDOWN/example_plugin_dontfloat.svg)
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(UiConstants::kTimelineHorizontalMarginPx, 4,
                             UiConstants::kTimelineHorizontalMarginPx, 0);
    root->setSpacing(4);

    auto* keyRow = new QHBoxLayout();
    keyRow->setSpacing(8);

    keyInput_ = new QLineEdit(this);
    keyInput_->setReadOnly(true);
    keyInput_->setPlaceholderText(tr("Undefined"));
    keyInput_->setMinimumWidth(120);
    keyInput_->setStyleSheet(
        "QLineEdit { background:#2b2b2b; border:1px solid #555; border-radius:2px;"
        "padding:2px 6px; color:white; font-size:11px; }"
        "QLineEdit:focus { border:1px solid #42a5f5; }");

    keyInput2_ = new QLineEdit(this);
    keyInput2_->setReadOnly(true);
    keyInput2_->setPlaceholderText(tr("Modulation"));
    keyInput2_->setMinimumWidth(120);
    keyInput2_->setVisible(false);
    keyInput2_->setStyleSheet(keyInput_->styleSheet());

    keyRow->addWidget(keyInput_);
    keyRow->addWidget(keyInput2_);
    keyRow->addStretch(1);
    root->addLayout(keyRow);

    // Панель тональностей референсного MIDI — под своей, появляется после
    // импорта. Поля стоят по тактам: модуляция видна там, где она звучит
    referenceKeyStrip_ = new KeyModulationStrip(this);
    referenceKeyStrip_->setReferenceAppearance(true);
    referenceKeyStrip_->hide();
    root->addWidget(referenceKeyStrip_);

    pitchGrid_ = new PitchGridWidget(this);
    // Ниже — только чтобы пианоролл не схлопывался: место ему даёт разметка
    pitchGrid_->setMinimumHeight(110);
    pitchGrid_->setPrimaryKey(QStringLiteral("C Major"));
    root->addWidget(pitchGrid_, 1);

    // Нижняя полоса — та же, что в главном окне: слева панель разреза, справа
    // «Экспорт MIDI» (макет MARKDOWN/example_plugin_dontfloat.svg).
    // Полоса тянется на всю ширину, поэтому экспорт стоит у правого края
    pianoRollToolbar_ = new PianoRollToolbar(this);

    // Аудио приходит с дорожки DAW, анализ идёт сам при каждом изменении
    // содержимого — кнопок «Анализировать» и «Экспорт WAV» больше нет.
    // «Применить коррекцию» живёт в той же полосе, слева от экспорта
    applyButton_ = new QPushButton(tr("Apply correction"), pianoRollToolbar_);
    applyButton_->setEnabled(false);
    applyButton_->setProperty("dontfloatSlim", true);
    applyButton_->setFixedHeight(PianoRollToolbar::kExportButtonHeightPx);
    pianoRollToolbar_->addTrailingWidget(applyButton_);
    root->addWidget(pianoRollToolbar_);

    setStatus(tr("load audio or play a track in the DAW to capture the signal."));

    analyzeOverlay_ = new QWidget(this);
    analyzeOverlay_->setStyleSheet(QStringLiteral(
        "background-color: rgba(40, 40, 40, 180); border-radius: 4px;"));
    analyzeOverlay_->hide();

    // Плашка теперь только про прогресс: анализ стартует сам, как только DAW
    // отдала аудио дорожки — нажимать «Анализировать» не нужно
    auto* overlayLayout = new QVBoxLayout(analyzeOverlay_);
    overlayLayout->setContentsMargins(16, 12, 16, 12);
    analyzeProgress_ = new QProgressBar(analyzeOverlay_);
    analyzeProgress_->setRange(0, 100);
    analyzeProgress_->setTextVisible(true);
    analyzeProgress_->setFormat(tr("Analyzing the track… %p%"));
    overlayLayout->addStretch(1);
    overlayLayout->addWidget(analyzeProgress_);
    overlayLayout->addStretch(1);

    keyMenu_ = new KeySelectionMenu(this);
    keyMenu2_ = new KeySelectionMenu(this);
    keyInput_->installEventFilter(this);
    keyInput2_->installEventFilter(this);
    keyInput_->setContextMenuPolicy(Qt::CustomContextMenu);
    keyInput2_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(keyInput_, &QLineEdit::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (keyMenu_) {
            keyMenu_->popup(keyInput_, pos);
        }
    });
    connect(keyInput2_, &QLineEdit::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (keyMenu2_) {
            keyMenu2_->popup(keyInput2_, pos);
        }
    });

    // Хост отдаёт аудио блоками; анализ запускаем, когда поток утих
    autoAnalysisTimer_ = new QTimer(this);
    autoAnalysisTimer_->setSingleShot(true);
    autoAnalysisTimer_->setInterval(kAutoAnalysisDelayMs);
    connect(autoAnalysisTimer_, &QTimer::timeout, this, &DontfloatPitchEditor::startAutoAnalysis);

    connect(keyMenu_, &KeySelectionMenu::keySelected, this, &DontfloatPitchEditor::onPrimaryKeySelected);
    connect(keyMenu2_, &KeySelectionMenu::keySelected, this, &DontfloatPitchEditor::onSecondaryKeySelected);
    connect(applyButton_, &QPushButton::clicked, this, &DontfloatPitchEditor::onApplyCorrectionClicked);
    connect(analysisWatcher_, &QFutureWatcher<void>::finished,
            this, &DontfloatPitchEditor::onPitchAnalysisFinished);

    // Масштаб и прокрутка общие с волной: обе половины окна показывают одну
    // дорожку, и разъехавшиеся таймлайны читать невозможно
    connect(pitchGrid_, &PitchGridWidget::horizontalOffsetChanged, this,
            [this](float offset) {
                if (!applyingTimelineView_) {
                    emit timelineOffsetChanged(offset);
                }
            });
    connect(pitchGrid_, &PitchGridWidget::timelineZoomRequested, this,
            [this](int angleDeltaY, float timelinePixelX) {
                // Масштаб задаёт волна, мы только просим: так он остаётся
                // в одних руках и не расходится
                emit timelineZoomRequested(angleDeltaY, timelinePixelX);
            });

    connect(pitchGrid_, &PitchGridWidget::notePitchEdited,
            this, &DontfloatPitchEditor::onNotePitchEdited);
    connect(pitchGrid_, &PitchGridWidget::notePreviewRequested,
            this, &DontfloatPitchEditor::onNotePreviewRequested);
    connect(pitchGrid_, &PitchGridWidget::notePreviewPitchChanged,
            this, &DontfloatPitchEditor::onNotePreviewPitchChanged);
    connect(pitchGrid_, &PitchGridWidget::notePreviewStopped,
            this, &DontfloatPitchEditor::onNotePreviewStopped);

    // Каретку двигают в плагине — просим DAW встать туда же. Раньше отсюда
    // уходил только сигнал наверх, а сам хост никто не двигал: щелчок по
    // пианороллу каретку DAW не трогал, и половины окна расходились
    connect(pitchGrid_, &PitchGridWidget::positionChanged, this, [this](qint64 positionMs) {
        if (applyingHostPlayhead_ || !session_) {
            return;
        }
        const int sampleRate = session_->audioBuffer().sampleRate;
        if (sampleRate > 0) {
            // Виджет по клику двигает только рисованную каретку, а свою
            // позицию оставляет прежней: в главном окне её ставит окно.
            // В плагине этого не делал никто — пианоролл щёлкали, а каретка
            // оставалась там, где была
            pitchGrid_->setPlaybackPosition(positionMs);
            const qint64 sourceSample = (positionMs * sampleRate) / 1000;
            requestHostSeek(sourceSample);
            emit seekRequested(sourceSample);
        }
    });

    // Разрез ноты: панель и клавиша S работают так же, как в главном окне
    connect(pitchGrid_, &PitchGridWidget::noteSplitRequested,
            this, &DontfloatPitchEditor::onNoteSplitRequested);
    connect(pitchGrid_, &PitchGridWidget::noteSplitRejected,
            this, &DontfloatPitchEditor::onNoteSplitRejected);
    connect(pitchGrid_, &PitchGridWidget::splitModeChanged,
            pianoRollToolbar_, &PianoRollToolbar::setSplitActive);
    connect(pianoRollToolbar_, &PianoRollToolbar::splitToggled,
            pitchGrid_, &PitchGridWidget::setSplitModeActive);
    connect(pianoRollToolbar_, &PianoRollToolbar::cutModeChanged,
            pitchGrid_, &PitchGridWidget::setCutMode);
    // Замки перемещения нот: горизонталь закрыта по умолчанию
    pitchGrid_->setHorizontalMoveLocked(pianoRollToolbar_->isHorizontalMoveLocked());
    pitchGrid_->setVerticalMoveLocked(pianoRollToolbar_->isVerticalMoveLocked());
    connect(pianoRollToolbar_, &PianoRollToolbar::horizontalMoveLockChanged,
            pitchGrid_, &PitchGridWidget::setHorizontalMoveLocked);
    connect(pianoRollToolbar_, &PianoRollToolbar::verticalMoveLockChanged,
            pitchGrid_, &PitchGridWidget::setVerticalMoveLocked);
    connect(pitchGrid_, &PitchGridWidget::noteTimeEdited, this,
        [this](int noteIndex, qint64 oldStartSample, qint64 newStartSample) {
            if (noteIndex < 0 || noteIndex >= baseNotes_.size()) {
                return;
            }
            // Сдвиг по времени — командой: Ctrl+Z вернёт ноту на место
            undoStack_->push(new PitchNoteMoveCommand(
                pitchGrid_, &baseNotes_, noteIndex, oldStartSample, newStartSample,
                tr("move note"), [this]() { applyNotesAfterUndo(); }));
        });
    connect(pianoRollToolbar_, &PianoRollToolbar::exportMidiRequested,
            this, &DontfloatPitchEditor::onExportMidiClicked);
    connect(pianoRollToolbar_, &PianoRollToolbar::importMidiRequested,
            this, &DontfloatPitchEditor::onImportMidiClicked);
    pianoRollToolbar_->setExportMidiEnabled(false);
    pitchGrid_->setCutMode(pianoRollToolbar_->cutMode());

    // Отмена/повтор внутри плагина: правки высот и разрезы нот. Клавиши ловим
    // только при фокусе в редакторе, чтобы не отбирать Ctrl+Z у DAW
    undoStack_ = new QUndoStack(this);
    // Рез ноты по S на всё окно плагина: сам пианоролл режет только когда
    // фокус на нём, а в плагине он легко уходит на кнопки панели. Двойного
    // реза не будет — виджет перехватывает клавишу через ShortcutOverride
    auto* splitShortcut = new QShortcut(QKeySequence(Qt::Key_S), this);
    splitShortcut->setContext(Qt::WindowShortcut);
    connect(splitShortcut, &QShortcut::activated, this, [this]() {
        if (pitchGrid_) {
            pitchGrid_->splitNoteAtPlaybackCursor();
        }
    });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        if (undoStack_->canUndo()) {
            setStatus(tr("undo: %1").arg(undoStack_->undoText()));
            undoStack_->undo();
        }
    });
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, this, [this]() {
        if (undoStack_->canRedo()) {
            setStatus(tr("redo: %1").arg(undoStack_->redoText()));
            undoStack_->redo();
        }
    });

    setPrimaryKey(QStringLiteral("C Major"));
    setSecondaryKey(QString());

    // Соседние экземпляры в этом же DAW: их ноты приходят к нам референсом.
    // Доска не умеет звать нас сама (ядро без Qt), поэтому опрашиваем её —
    // сравнивается только отметка, ноты копируются лишь когда они изменились
    instanceId_ = Dontfloat::PluginCore::SharedNoteBoard::registerInstance();
    sharedNotesTimer_ = new QTimer(this);
    sharedNotesTimer_->setInterval(kSharedNotesPollMs);
    connect(sharedNotesTimer_, &QTimer::timeout,
            this, &DontfloatPitchEditor::pullSharedReferenceNotes);
    sharedNotesTimer_->start();
}

DontfloatPitchEditor::~DontfloatPitchEditor()
{
    Dontfloat::PluginCore::SharedNoteBoard::unregisterInstance(instanceId_);
}

void DontfloatPitchEditor::setProductName(const QString& productName)
{
    productName_ = productName;
    setWindowTitle(productName_);
    refreshFromSession();
}

void DontfloatPitchEditor::setStatus(const QString& text)
{
    // Статусбар живёт в оболочке плагина — как в главном окне, один на весь редактор
    emit statusMessage(QStringLiteral("%1: %2").arg(productName_, text));
}

void DontfloatPitchEditor::onNoteSplitRequested(int noteIndex, qint64 splitSample)
{
    if (noteIndex < 0 || noteIndex >= baseNotes_.size()) {
        return;
    }
    // В плагине метки растяжения не двигают ноты, поэтому координата реза
    // совпадает с координатой модели — пересчёт по таймлайну не нужен
    constexpr qint64 minPart = PianoRollEngine::kMinNotePartSamples;
    const qint64 noteStart = baseNotes_[noteIndex].startSample;
    const qint64 noteEnd = baseNotes_[noteIndex].endSample;
    if (noteEnd - noteStart < 2 * minPart) {
        setStatus(tr("the note is too short to split"));
        return;
    }
    const qint64 cutSample = qBound(noteStart + minPart, splitSample, noteEnd - minPart);

    // Через команду: Ctrl+Z склеит ноту обратно
    undoStack_->push(new PitchNoteSplitCommand(
        &baseNotes_, noteIndex, cutSample, tr("split note"), [this]() {
            refreshPitchGrid();
            applyNotesAfterUndo();
        }));
    setStatus(tr("note split"));
}

void DontfloatPitchEditor::onNoteSplitRejected(PitchGridWidget::SplitRejection reason)
{
    setStatus(reason == PitchGridWidget::SplitRejection::NoNoteAtCursor
                  ? tr("no note at the cut position")
                  : tr("the cut point is outside the note — use “Free cut” or move the cursor"));
}

void DontfloatPitchEditor::bindSession(TrackToolSession* session)
{
    session_ = session;
    refreshFromSession();
}

void DontfloatPitchEditor::refreshFromSession()
{
    if (!session_) {
        return;
    }

    const TrackAudioBuffer& buffer = session_->audioBuffer();
    if (!buffer.empty()) {
        QVector<QVector<float>> channels;
        if (!buffer.left.empty()) {
            channels.append(toQVector(buffer.left));
            if (!buffer.right.empty()) {
                channels.append(toQVector(buffer.right));
            }
        } else if (!buffer.mono.empty()) {
            channels.append(toQVector(buffer.mono));
        }
        pitchGrid_->setAudioData(channels);
        pitchGrid_->setSampleRate(buffer.sampleRate);
        pitchGrid_->setTimelineSampleCount(buffer.frameCount());
        syncReferenceKeyStrip();
        setStatus(tr("audio: %1 samples, %2 Hz")
                      .arg(buffer.frameCount())
                      .arg(buffer.sampleRate));
        // Плашка показывается только на время самого анализа
        analyzeOverlay_->setVisible(analysisRunning_);
    } else {
        pitchGrid_->clearNotes();
        analyzeOverlay_->hide();
        setStatus(tr("load audio or play a track in the DAW to capture the signal."));
    }

    if (session_->pitchAnalysis().valid) {
        baseNotes_ = fromCoreNotes(session_->pitchAnalysis().notes);
        refreshPitchGrid();
        applyButton_->setEnabled(PitchCorrection::hasPendingEdits(baseNotes_));
        analyzeOverlay_->hide();
    }

    layoutAnalyzeOverlay();
}

void DontfloatPitchEditor::notifyHostAudioAppended()
{
    if (!session_ || session_->audioBuffer().empty()) {
        return;
    }
    // Хост зовёт это на каждый блок: полное обновление вида слишком дорого,
    // поэтому не чаще kHostRefreshIntervalMs (анализ всё равно ждёт таймера)
    if (!hostRefreshClock_.isValid()
        || hostRefreshClock_.elapsed() >= kHostRefreshIntervalMs) {
        hostRefreshClock_.restart();
        refreshFromSession();
    }
    // Хост шлёт аудио блоками: перезапускаем таймер, чтобы анализ стартовал
    // один раз — когда дорожка прогналась целиком и поток прекратился
    if (!analysisRunning_ && autoAnalysisTimer_) {
        autoAnalysisTimer_->start();
    }
}

void DontfloatPitchEditor::setHostPlayhead(qint64 samplePosition)
{
    if (!pitchGrid_ || !session_) {
        return;
    }
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (sampleRate <= 0) {
        return;
    }
    // Пианоролл живёт в миллисекундах дорожки: каретка встаёт туда же, где
    // каретка DAW (setPlaybackPosition сам пересчитает её в пиксели).
    // Флаг гасит обратную отправку в DAW — иначе позиция ходила бы по кругу.
    // Каретка приходит во времени проекта: клип может стоять не в начале и
    // быть растянут. Волна пересчитывает так же — иначе две каретки в одном
    // окне показывают разные места и при воспроизведении расходятся
    qint64 inSource = samplePosition;
#if defined(DONTFLOAT_WITH_ARA)
    if (araClipValid_) {
        const double projectSeconds = double(samplePosition) / double(sampleRate);
        const double sourceSeconds = araClipStartSourceSec_
            + (projectSeconds - araClipStartPlaybackSec_) / araClipStretch_;
        inSource = qint64(std::llround(sourceSeconds * double(sampleRate)));
    }
#endif

    const qint64 clamped = std::clamp<qint64>(
        inSource, 0, qint64(session_->audioBuffer().frameCount()));
    applyingHostPlayhead_ = true;
    pitchGrid_->setPlaybackPosition((clamped * 1000) / sampleRate);
    applyingHostPlayhead_ = false;
}

#if defined(DONTFLOAT_WITH_ARA)
Dontfloat::Ara::AraDocumentController* DontfloatPitchEditor::araDocumentController() const
{
    if (!araBinding_) {
        return nullptr;
    }
    const auto* extension = static_cast<const ARA::PlugIn::PlugInExtension*>(araBinding_);
    return extension->getDocumentController<Dontfloat::Ara::AraDocumentController>();
}

void DontfloatPitchEditor::publishEditedAudioToAra(const std::vector<float>& mono)
{
    auto* controller = araDocumentController();
    if (!controller || mono.empty()) {
        return;
    }
    const auto* extension = static_cast<const ARA::PlugIn::PlugInExtension*>(araBinding_);
    Dontfloat::Ara::AraAudioSource* source =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*extension);
    if (!source) {
        source = controller->onlyAudioSource();
    }
    controller->publishEditedAudio(source, mono);
}
#endif

void DontfloatPitchEditor::applySourcePlayhead(qint64 sourceSample)
{
    if (!pitchGrid_ || !session_) {
        return;
    }
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (sampleRate <= 0) {
        return;
    }
    const qint64 clamped = std::clamp<qint64>(
        sourceSample, 0, qint64(session_->audioBuffer().frameCount()));
    // Тот же флаг, что и для каретки хоста: иначе позиция ходила бы по кругу
    // между половинами окна
    applyingHostPlayhead_ = true;
    pitchGrid_->setPlaybackPosition((clamped * 1000) / sampleRate);
    applyingHostPlayhead_ = false;
}

bool DontfloatPitchEditor::requestHostTransport(bool start)
{
#if defined(DONTFLOAT_WITH_ARA)
    if (auto* controller = araDocumentController()) {
        return controller->requestHostPlayback(start);
    }
#else
    Q_UNUSED(start);
#endif
    return false;
}

bool DontfloatPitchEditor::requestHostSeek(qint64 sourceSample)
{
#if defined(DONTFLOAT_WITH_ARA)
    if (!araBinding_ || !session_) {
        return false;
    }
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (sampleRate <= 0) {
        return false;
    }
    auto* controller = araDocumentController();
    if (!controller) {
        return false;
    }
    // Сэмплы источника → секунды проекта тем же размещением клипа, каким
    // каретка хоста переводится к нам (см. setHostPlayhead)
    double projectSeconds = double(sourceSample) / double(sampleRate);
    if (araClipValid_) {
        projectSeconds = araClipStartPlaybackSec_
            + (projectSeconds - araClipStartSourceSec_) * araClipStretch_;
    }
    return controller->requestHostPlaybackPosition(projectSeconds);
#else
    Q_UNUSED(sourceSample);
    return false;
#endif
}

void DontfloatPitchEditor::startAutoAnalysis()
{
    if (!session_ || session_->audioBuffer().empty() || analysisRunning_) {
        return;
    }
    // Поток аудио утих — показываем дорожку целиком
    refreshFromSession();

    const auto print = Dontfloat::PluginCore::computeContentFingerprint(session_->audioBuffer());
    if (print.empty()) {
        return;
    }
    if (print.hash == analyzedContent_.hash && print.startFrame == analyzedContent_.startFrame
        && print.lengthFrames == analyzedContent_.lengthFrames) {
        return;  // содержимое не менялось
    }

    // Тот же материал на новой позиции — клип переехал в DAW: ноты едут с ним.
    // std::int64_t, а не qint64: на Linux (LP64) это разные типы, см. ядро
    std::int64_t shift = 0;
    if (Dontfloat::PluginCore::detectContentShift(analyzedContent_, print, &shift)) {
        shiftNotes(shift);
        analyzedContent_ = print;
        setStatus(tr("clip moved — notes followed"));
        return;
    }

    // Содержимое дорожки изменилось — считаем ноты заново
    analyzedContent_ = print;
    runPitchAnalysis();
}

void DontfloatPitchEditor::setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample)
{
    if (!pitchGrid_ || bpm <= 0.0) {
        return;
    }
    // Сетка пианоролла = сетка DAW: доли реза и снап считаются по ней
    hostBpm_ = bpm;
    hostBeatsPerBar_ = std::max(1, beatsPerBar);
    hostGridStartSample_ = std::max<qint64>(0, barStartSample);
    pitchGrid_->setBPM(float(hostBpm_));
    pitchGrid_->setBeatsPerBar(hostBeatsPerBar_);
    pitchGrid_->setGridStartSample(hostGridStartSample_);
}

void DontfloatPitchEditor::onImportMidiClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import reference MIDI"), QString(), tr("MIDI files (*.mid *.midi)"));
    if (path.isEmpty()) {
        return;
    }

    // Как раскладывать ноты по времени — спрашиваем сразу после выбора файла
    QMessageBox question(this);
    question.setWindowTitle(tr("Import reference MIDI"));
    question.setText(tr("How should the reference notes be placed on the timeline?"));
    question.setIcon(QMessageBox::Question);
    QPushButton* keepButton = question.addButton(tr("Keep as is"), QMessageBox::AcceptRole);
    QPushButton* fitButton = question.addButton(tr("Fit to BPM"), QMessageBox::AcceptRole);
    QPushButton* alignButton =
        question.addButton(tr("Align and fit to BPM"), QMessageBox::AcceptRole);
    question.addButton(QMessageBox::Cancel);
    question.exec();

    MidiImporter::Options options;
    if (question.clickedButton() == keepButton) {
        options.mode = MidiImporter::TimingMode::KeepAsIs;
    } else if (question.clickedButton() == fitButton) {
        options.mode = MidiImporter::TimingMode::FitToBpm;
    } else if (question.clickedButton() == alignButton) {
        options.mode = MidiImporter::TimingMode::AlignAndFitToBpm;
    } else {
        return;  // отмена
    }

    // Темп и сетка — от DAW (см. setHostBeatGrid), частота — от захваченной дорожки
    options.projectBpm = hostBpm_ > 0.0 ? float(hostBpm_) : 120.0f;
    options.sampleRate = session_ ? session_->audioBuffer().sampleRate : 44100;
    options.gridStartSample = hostGridStartSample_;

    const MidiImporter::Result result = MidiImporter::readFile(path, options);
    if (!result.ok) {
        setStatus(tr("MIDI import error: %1").arg(result.error));
        return;
    }

    // Импортированный вручную референс важнее нот соседней дорожки
    referenceFromImport_ = true;
    applyReferenceNotes(result.notes, options.sampleRate);

    const QString referenceKey = referenceKeys_.primaryKey.keyName;
    setStatus(tr("reference MIDI: %1 notes, key %2")
                  .arg(result.notes.size())
                  .arg(referenceKey.isEmpty() ? tr("undefined") : referenceKey));
}

void DontfloatPitchEditor::onExportMidiClicked()
{
    if (baseNotes_.isEmpty()) {
        setStatus(tr("there are no notes to export"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export MIDI"), QStringLiteral("dontfloat.mid"), tr("MIDI files (*.mid)"));
    if (path.isEmpty()) {
        return;
    }

    MidiExporter::Options options;
    options.sampleRate = session_ ? session_->audioBuffer().sampleRate : 44100;
    if (session_ && session_->analysisValid() && session_->analysis().bpm > 0.0f) {
        options.bpm = session_->analysis().bpm;
        options.startSample = session_->analysis().gridStartFrame;
    }

    QString error;
    if (!MidiExporter::writeFile(path, baseNotes_, options, &error)) {
        setStatus(tr("MIDI export error: %1").arg(error));
        return;
    }
    setStatus(tr("MIDI exported: %1 (notes: %2)")
                  .arg(QFileInfo(path).fileName())
                  .arg(baseNotes_.size()));
}

void DontfloatPitchEditor::shiftNotes(qint64 deltaSamples)
{
    if (deltaSamples == 0 || baseNotes_.isEmpty()) {
        return;
    }
    for (PitchDetector::PitchNote& note : baseNotes_) {
        note.startSample = std::max<qint64>(0, note.startSample + deltaSamples);
        note.endSample = std::max<qint64>(note.startSample + 1, note.endSample + deltaSamples);
        // Клип переехал целиком — вместе с ним едет и исходный отрезок ноты
        if (note.sourceStartSample >= 0) {
            note.sourceStartSample = std::max<qint64>(0, note.sourceStartSample + deltaSamples);
            note.sourceEndSample =
                std::max<qint64>(note.sourceStartSample + 1, note.sourceEndSample + deltaSamples);
        }
    }
    refreshPitchGrid();
    syncNotesToSession();
}

void DontfloatPitchEditor::setPrimaryKey(const QString& key)
{
    primaryKey_ = key.isEmpty() ? QStringLiteral("C Major") : key;
    keyInput_->setText(primaryKey_);
    pitchGrid_->setPrimaryKey(primaryKey_);
}

void DontfloatPitchEditor::setSecondaryKey(const QString& key)
{
    secondaryKey_ = key;
    keyInput2_->setVisible(!key.isEmpty());
    keyInput2_->setText(key);
    pitchGrid_->setSecondaryKey(key);
}

void DontfloatPitchEditor::setAnalysisRunning(bool running)
{
    analysisRunning_ = running;
    analyzeProgress_->setVisible(running);
    if (analyzeOverlay_) {
        analyzeOverlay_->setVisible(running);
        if (running) {
            analyzeOverlay_->raise();
        }
    }
    applyButton_->setEnabled(!running && PitchCorrection::hasPendingEdits(baseNotes_));
}

void DontfloatPitchEditor::layoutAnalyzeOverlay()
{
    if (!analyzeOverlay_ || !pitchGrid_) {
        return;
    }
    const QRect area = rect();
    const int top = keyInput_->geometry().bottom() + 8;
    analyzeOverlay_->setGeometry(8, top, area.width() - 16, area.height() - top - 48);
}

void DontfloatPitchEditor::refreshPitchGrid()
{
    pitchGrid_->setNotes(baseNotes_);
    fitPitchRangeToNotes();
    if (pianoRollToolbar_) {
        pianoRollToolbar_->setExportMidiEnabled(!baseNotes_.isEmpty());
    }
}

void DontfloatPitchEditor::fitPitchRangeToNotes()
{
    if (baseNotes_.isEmpty()) {
        return;
    }
    // Без подгонки найденные ноты могут оказаться ниже видимых строк, и после
    // авто-анализа пианоролл выглядит пустым
    float lowest = baseNotes_.first().midiPitch;
    float highest = lowest;
    for (const PitchDetector::PitchNote& note : baseNotes_) {
        lowest = std::min(lowest, note.midiPitch);
        highest = std::max(highest, note.midiPitch);
    }

    constexpr int kPadding = 3;
    constexpr int kMinSpan = 24;  // не меньше двух октав в кадре
    int minPitch = int(std::floor(lowest)) - kPadding;
    int maxPitch = int(std::ceil(highest)) + kPadding;
    if (maxPitch - minPitch < kMinSpan) {
        const int extra = (kMinSpan - (maxPitch - minPitch) + 1) / 2;
        minPitch -= extra;
        maxPitch += extra;
    }
    minPitch = std::clamp(minPitch, 0, 120);
    maxPitch = std::clamp(maxPitch, minPitch + 12, 127);
    pitchGrid_->setPitchRange(minPitch, maxPitch);
}

void DontfloatPitchEditor::syncNotesToSession()
{
    if (!session_) {
        return;
    }
    session_->pitchAnalysis().notes = toCoreNotes(baseNotes_);
    session_->pitchAnalysis().valid = !baseNotes_.isEmpty();
    publishNotesToBoard();
    emit pitchSessionChanged();
}

bool DontfloatPitchEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            if (watched == keyInput_ && keyMenu_) {
                keyMenu_->popup(keyInput_, mouse->pos());
                return true;
            }
            if (watched == keyInput2_ && keyMenu2_) {
                keyMenu2_->popup(keyInput2_, mouse->pos());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DontfloatPitchEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutAnalyzeOverlay();
    syncReferenceKeyStrip();
}

void DontfloatPitchEditor::applyReferenceNotes(const QVector<PitchDetector::PitchNote>& notes,
                                               int sampleRate)
{
    pitchGrid_->setReferenceNotes(notes);

    // Тональности референса — потактово по сетке DAW, как у полосы проекта
    KeyAnalyzer::BarGrid grid;
    grid.bpm = hostBpm_ > 0.0 ? float(hostBpm_) : 120.0f;
    grid.beatsPerBar = hostBeatsPerBar_;
    grid.gridStartSample = hostGridStartSample_;
    referenceKeys_ = MidiImporter::analyzeKeyPerBar(notes, grid, sampleRate);
    referenceKeyStrip_->setRegions(referenceKeys_.regions);
    referenceKeyStrip_->setVisible(!referenceKeys_.regions.isEmpty());
    syncReferenceKeyStrip();
}

void DontfloatPitchEditor::applyTimelineZoom(float zoom)
{
    if (!pitchGrid_ || zoom <= 0.0f) {
        return;
    }
    applyingTimelineView_ = true;
    pitchGrid_->setZoomLevel(zoom);
    applyingTimelineView_ = false;
}

void DontfloatPitchEditor::applyTimelineOffset(float offset)
{
    if (!pitchGrid_) {
        return;
    }
    applyingTimelineView_ = true;
    pitchGrid_->setHorizontalOffset(offset);
    applyingTimelineView_ = false;
}

void DontfloatPitchEditor::applyTimelineSampleCount(qint64 samples)
{
    if (pitchGrid_ && samples > 0) {
        // Длина таймлайна общая: иначе одна и та же секунда попадает на
        // разные пиксели в волне и в пианоролле
        pitchGrid_->setTimelineSampleCount(samples);
    }
}

void DontfloatPitchEditor::publishNotesToBoard()
{
    // Свои ноты — соседним экземплярам плагина в этом же DAW
    const int sampleRate = session_ ? session_->audioBuffer().sampleRate : 44100;
    Dontfloat::PluginCore::SharedNoteBoard::publish(
        instanceId_, toUtf8String(productName_), sampleRate, toCoreNotes(baseNotes_));
}

#if defined(DONTFLOAT_WITH_ARA)

void DontfloatPitchEditor::setAraBinding(const void* extension)
{
    araBinding_ = extension;
    appliedAraRevision_ = 0;
    if (araBinding_) {
        // В режиме ARA звук и разметка приходят из документа: захват блоками и
        // доска нот соседей больше не нужны
        setStatus(tr("ARA: bound to the host document"));
        pullFromAraModel();
    }
}

bool DontfloatPitchEditor::pullFromAraModel()
{
    if (!araBinding_) {
        return false;
    }
    const auto* extension =
        static_cast<const ARA::PlugIn::PlugInExtension*>(araBinding_);
    auto* controller = extension->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    if (!controller) {
        return false;
    }

    Dontfloat::Ara::AraAudioSource* ownSource =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*extension);
    if (!ownSource) {
        // Роли хост мог назначить позже привязки — на одной дорожке источник
        // всё равно один, берём его
        ownSource = controller->onlyAudioSource();
    }

    // Плашка прогресса: разбор идёт сразу после появления дорожки, и пианоролл
    // на это время прикрыт — как в главном окне
    const bool analysisRunning = ownSource && ownSource->analysisRunning();
    if (analysisRunning) {
        setAnalysisRunning(true);
        analyzeProgress_->setValue(ownSource->analysisProgress());
        analyzeOverlay_->setVisible(true);
        layoutAnalyzeOverlay();
    } else if (araAnalysisWasRunning_) {
        setAnalysisRunning(false);
        analyzeOverlay_->hide();
    }
    araAnalysisWasRunning_ = analysisRunning;

    const std::uint64_t revision = controller->modelRevision();
    if (revision == appliedAraRevision_ && araAudioApplied_) {
        return true;  // модель не менялась — дёргать вид незачем
    }
    appliedAraRevision_ = revision;

    // Звук дорожки приходит из ARA целиком: ни проигрывания, ни «записи»
    // блоков в плагин не нужно (так же работает Melodyne)
    if (ownSource && !ownSource->monoSamples().empty() && session_ && !araAudioApplied_) {
        const std::vector<float>& mono = ownSource->monoSamples();
        TrackAudioBuffer buffer;
        buffer.sampleRate = int(ownSource->getSampleRate() > 0.0 ? ownSource->getSampleRate()
                                                                : 44100.0);
        buffer.channelCount = 1;
        buffer.mono = mono;
        session_->setAudioBuffer(buffer);
        araAudioApplied_ = true;
        refreshFromSession();
    }

    // Тактовая сетка — из musical context хоста, причём в координатах дорожки:
    // клип может стоять не в начале проекта, и сетка обязана совпасть с DAW
    const Dontfloat::Ara::AraBeatGrid grid = controller->hostBeatGrid();
    if (grid.valid && pitchGrid_) {
        const int sampleRate = session_ && session_->audioBuffer().sampleRate > 0
            ? session_->audioBuffer().sampleRate
            : 44100;
        double gridStartInSource = grid.gridStartSeconds;
        // Клип берём **свой**, а не первый клип источника: после разреза их
        // несколько, и половины окна начинали считать в разных координатах
        Dontfloat::Ara::AraClipPlacement clip;
        if (Dontfloat::Ara::AraDocumentController::clipForInstance(*extension, &clip)) {
            // Время проекта → время источника: где начало такта 1 внутри файла
            gridStartInSource = clip.startInSourceSeconds
                + (grid.gridStartSeconds - clip.startInPlaybackSeconds) / clip.stretchFactor();
            // Тем же размещением переводится и каретка (см. setHostPlayhead)
            araClipStartPlaybackSec_ = clip.startInPlaybackSeconds;
            araClipStartSourceSec_ = clip.startInSourceSeconds;
            araClipStretch_ = clip.stretchFactor() > 0.0 ? clip.stretchFactor() : 1.0;
            araClipValid_ = true;
        }
        setHostBeatGrid(grid.tempoBpm, grid.beatsPerBar,
                        qint64(gridStartInSource * double(sampleRate)));
    }

    // Свои ноты: разбор сделал document controller по всему файлу дорожки.
    // Синими и правимыми они станут только здесь — жалоба «правятся ноты
    // соседней дорожки» означает, что экземпляр сел на чужой источник,
    // поэтому в дневник пишем, чей источник он посчитал своим
    if (ownSource && ownSource->hasNotes() && baseNotes_.isEmpty()) {
        const Dontfloat::Ara::AraNoteSet& own = ownSource->noteSet();
        baseNotes_ = fromCoreNotes(own.notes);
        if (Dontfloat::PluginCore::Diagnostics::enabled()) {
            char line[256];
            std::snprintf(line, sizeof(line), "ara.notes.own count=%d source=%s",
                          int(baseNotes_.size()), ownSource->getPersistentID().c_str());
            Dontfloat::PluginCore::Diagnostics::log(line);
        }
        refreshPitchGrid();
        applyNotesAfterUndo();
        setStatus(tr("ARA: %1 notes from the host document").arg(baseNotes_.size()));
    }

    // Ноты соседней дорожки — серым фоном (референс), если свой .mid не открыт
    if (!referenceFromImport_) {
        const Dontfloat::Ara::AraNoteSet neighbour =
            controller->referenceNotesExcluding(ownSource);
        if (neighbour.valid && !neighbour.notes.empty()) {
            const int sampleRate = int(neighbour.sampleRate > 0.0 ? neighbour.sampleRate : 44100.0);
            applyReferenceNotes(fromCoreNotes(neighbour.notes), sampleRate);
        } else if (pitchGrid_ && !pitchGrid_->referenceNotes().isEmpty()) {
            pitchGrid_->clearReferenceNotes();
            referenceKeys_ = KeyAnalyzer::PerBarKeyResult();
            if (referenceKeyStrip_) {
                referenceKeyStrip_->setRegions({});
                referenceKeyStrip_->hide();
            }
        }
    }
    return true;
}

#endif // DONTFLOAT_WITH_ARA

void DontfloatPitchEditor::pullSharedReferenceNotes()
{
#if defined(DONTFLOAT_WITH_ARA)
    // Под ARA всё берётся из документа: и свои ноты, и соседние дорожки
    if (pullFromAraModel()) {
        return;
    }
#endif
    if (referenceFromImport_) {
        return;  // импортированный вручную референс не перебиваем
    }

    const auto stamp = Dontfloat::PluginCore::SharedNoteBoard::latestStamp(instanceId_);
    if (stamp.revision == appliedSharedRevision_) {
        return;  // соседи ничего нового не выкладывали
    }
    appliedSharedRevision_ = stamp.revision;

    const auto shared = Dontfloat::PluginCore::SharedNoteBoard::latestFrom(instanceId_);
    if (shared.empty()) {
        // Сосед закрылся или остался без нот — референс убираем
        pitchGrid_->clearReferenceNotes();
        referenceKeys_ = KeyAnalyzer::PerBarKeyResult();
        referenceKeyStrip_->setRegions({});
        referenceKeyStrip_->hide();
        return;
    }

    const int sampleRate = session_ && session_->audioBuffer().sampleRate > 0
        ? session_->audioBuffer().sampleRate
        : shared.sampleRate;
    QVector<PitchDetector::PitchNote> notes = fromCoreNotes(shared.notes);
    if (shared.sampleRate > 0 && sampleRate != shared.sampleRate) {
        // У дорожек разная частота — позиции переводим в свои сэмплы
        const double scale = double(sampleRate) / double(shared.sampleRate);
        for (PitchDetector::PitchNote& note : notes) {
            note.startSample = qint64(std::llround(double(note.startSample) * scale));
            note.endSample = std::max(note.startSample + 1,
                                      qint64(std::llround(double(note.endSample) * scale)));
        }
    }

    applyReferenceNotes(notes, sampleRate);
    setStatus(tr("reference from another track (%1): %2 notes, key %3")
                  .arg(fromUtf8String(shared.publisherName))
                  .arg(notes.size())
                  .arg(referenceKeys_.primaryKey.keyName.isEmpty()
                           ? tr("undefined")
                           : referenceKeys_.primaryKey.keyName));
}

void DontfloatPitchEditor::syncReferenceKeyStrip()
{
    if (!referenceKeyStrip_ || !pitchGrid_) {
        return;
    }
    // У плагина таймлайн не масштабируется — достаточно длины и ширины вида
    referenceKeyStrip_->setTimelineSampleCount(pitchGrid_->timelineSamples());
    referenceKeyStrip_->setTimelineReferenceWidth(pitchGrid_->width());
}

void DontfloatPitchEditor::runPitchAnalysis()
{
    if (!session_ || session_->audioBuffer().empty() || analysisRunning_) {
        return;
    }

    const QVector<float> mono = toQVector(session_->audioBuffer().mono);
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (mono.isEmpty() || sampleRate <= 0) {
        setStatus(tr("no audio data for analysis"));
        return;
    }

    setAnalysisRunning(true);
    analyzeProgress_->setValue(0);
    analysisProgress_ = std::make_shared<std::atomic<int>>(0);

    // Результат не возвращаем через QFuture: после длинного анализа
    // QFutureWatcher::result() падает в QResultStore/QList::at (MSVC Debug) —
    // та же грабля, что описана в MainWindow. Отдаём через shared_ptr.
    pendingOutcome_ = std::make_shared<PitchAnalysisOutcome>();
    analysisWatcher_->setFuture(QtConcurrent::run(
        [mono, sampleRate, progress = analysisProgress_, outcome = pendingOutcome_]() {
            progress->store(2);
            const KeyAnalyzer::AnalysisResult keyResult = KeyAnalyzer::analyzeKey(mono, sampleRate);
            outcome->primaryKeyName = keyNameFromInfo(keyResult.primaryKey);
            if (keyResult.hasKeyChange
                && keyResult.secondaryKey.key != KeyAnalyzer::UNKNOWN_KEY
                && !keyResult.secondaryKey.keyName.isEmpty()) {
                outcome->secondaryKeyName = keyResult.secondaryKey.keyName;
            }
            progress->store(15);
            const QVector<PitchDetector::PitchNote> notes = PitchDetector::detectNotes(
                mono, sampleRate, PitchDetector::Options(),
                [progress](int pct) { progress->store(15 + pct * 85 / 100); });
            progress->store(100);

            outcome->pitch.valid = true;
            outcome->pitch.notes = toCoreNotes(notes);
            outcome->pitch.keys.hasKeyChange = keyResult.hasKeyChange;
        }));
}

void DontfloatPitchEditor::onPitchAnalysisFinished()
{
    setAnalysisRunning(false);
    analyzeProgress_->setValue(100);

    if (!pendingOutcome_) {
        return;
    }
    const PitchAnalysisOutcome outcome = *pendingOutcome_;
    pendingOutcome_.reset();
    setPrimaryKey(outcome.primaryKeyName);
    if (!outcome.secondaryKeyName.isEmpty()) {
        setSecondaryKey(outcome.secondaryKeyName);
    } else {
        setSecondaryKey(QString());
    }

    baseNotes_ = fromCoreNotes(outcome.pitch.notes);
    if (session_) {
        session_->pitchAnalysis() = outcome.pitch;
    }
    refreshPitchGrid();
    applyButton_->setEnabled(PitchCorrection::hasPendingEdits(baseNotes_));
    analyzeOverlay_->hide();

    const QString keysText = outcome.secondaryKeyName.isEmpty()
        ? outcome.primaryKeyName
        : outcome.primaryKeyName + QStringLiteral(" / ") + outcome.secondaryKeyName;
    setStatus(tr("analysis done: %1, notes found: %2")
                  .arg(keysText)
                  .arg(baseNotes_.size()));
    publishNotesToBoard();
    emit pitchSessionChanged();
}

void DontfloatPitchEditor::onApplyCorrectionClicked()
{
    if (!session_ || baseNotes_.isEmpty()) {
        return;
    }

    const TrackAudioBuffer& source = session_->audioBuffer();
    if (source.mono.empty()) {
        setStatus(tr("no source audio for correction"));
        return;
    }

    const QVector<float> mono = toQVector(source.mono);
    if (!PitchCorrection::hasPendingEdits(baseNotes_)) {
        setStatus(tr("no modified notes for correction"));
        return;
    }

    setStatus(tr("applying pitch correction…"));
    QVector<QVector<float>> channels;
    channels.append(mono);
    const QVector<QVector<float>> corrected = PitchCorrection::apply(
        channels, baseNotes_, source.sampleRate);
    if (corrected.isEmpty() || corrected[0].isEmpty()) {
        setStatus(tr("correction failed"));
        return;
    }

    TrackAudioBuffer buffer = source;
    buffer.mono = toStdVector(corrected[0]);
    buffer.left = buffer.mono;
    buffer.right.clear();
    buffer.channelCount = 1;
    session_->setAudioBuffer(buffer);
    session_->pitchAnalysis().notes = toCoreNotes(baseNotes_);
    for (TrackPitchNote& note : session_->pitchAnalysis().notes) {
        note.detectedPitch = note.midiPitch;
    }
    baseNotes_ = fromCoreNotes(session_->pitchAnalysis().notes);
    // Результат уходит в выход плагина: без этого DAW играла бы исходный звук
    session_->setRenderedOutput(buffer, 0);
#if defined(DONTFLOAT_WITH_ARA)
    // Под ARA выход плагина хосту не отдаётся вовсе: дорожку выдаёт наш
    // рендерер, и играть он будет ровно то, что положили сюда
    publishEditedAudioToAra(buffer.mono);
#endif
    refreshFromSession();
    applyButton_->setEnabled(false);
    setStatus(tr("correction applied — the DAW now plays the corrected audio"));
    emit renderedOutputChanged();
    emit pitchSessionChanged();
}

void DontfloatPitchEditor::onPrimaryKeySelected(const QString& key)
{
    setPrimaryKey(key.isEmpty() ? QStringLiteral("C Major") : key);
}

void DontfloatPitchEditor::onSecondaryKeySelected(const QString& key)
{
    setSecondaryKey(key);
}

void DontfloatPitchEditor::onNotePitchEdited(int noteIndex, float oldPitch, float newPitch)
{
    if (noteIndex < 0 || noteIndex >= baseNotes_.size()) {
        return;
    }
    // Через команду, а не напрямую: Ctrl+Z вернёт прежнюю высоту
    undoStack_->push(new PitchNoteEditCommand(
        pitchGrid_, &baseNotes_, noteIndex, oldPitch, newPitch,
        tr("note pitch"), [this]() { applyNotesAfterUndo(); }));
}

void DontfloatPitchEditor::applyNotesAfterUndo()
{
    syncNotesToSession();
    if (applyButton_) {
        applyButton_->setEnabled(PitchCorrection::hasPendingEdits(baseNotes_));
    }
}

void DontfloatPitchEditor::onNotePreviewRequested(int noteIndex)
{
    if (!session_ || noteIndex < 0 || noteIndex >= baseNotes_.size()) {
        return;
    }
    const TrackAudioBuffer& buffer = session_->audioBuffer();
    if (buffer.mono.empty()) {
        return;
    }
    const PitchDetector::PitchNote& note = baseNotes_[noteIndex];
    const QVector<float> mono = toQVector(buffer.mono);
    // Звук ноты — с её исходного места (после переноса там уже другое)
    const qint64 length = qMax<qint64>(1, note.sourceEnd() - note.sourceStart());
    const QVector<float> segment = mono.mid(int(note.sourceStart()), int(length));
    notePreviewPlayer_->start(segment, buffer.sampleRate, note.midiPitch - note.detectedPitch);
}

void DontfloatPitchEditor::onNotePreviewPitchChanged(int noteIndex, float midiPitch)
{
    if (noteIndex < 0 || noteIndex >= baseNotes_.size()) {
        return;
    }
    const PitchDetector::PitchNote& note = baseNotes_[noteIndex];
    notePreviewPlayer_->setSemitoneOffset(midiPitch - note.detectedPitch);
}

void DontfloatPitchEditor::onNotePreviewStopped()
{
    notePreviewPlayer_->stop();
}

} // namespace Dontfloat::Plugins::Ui
