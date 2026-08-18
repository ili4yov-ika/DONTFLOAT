#include "dontfloat_pitch_editor.h"

#include "../../include/audiofileservice.h"
#include "../../include/keyanalyzer.h"
#include "../../include/keyselectionmenu.h"
#include "../../include/notepreviewplayer.h"
#include "../../include/pitchcorrection.h"
#include "../../include/pianoroll_engine.h"
#include "../../include/pianoroll_toolbar.h"
#include "../../include/pitchdetector.h"
#include "../../include/midiexporter.h"
#include "../../include/pitchgridwidget.h"
#include "../../include/uiconstants.h"

#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
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

} // namespace

DontfloatPitchEditor::DontfloatPitchEditor(QWidget* parent, const QString& productName)
    : QWidget(parent)
    , productName_(productName)
    , analysisWatcher_(new QFutureWatcher<void>(this))
    , notePreviewPlayer_(new NotePreviewPlayer(this))
{
    setObjectName(QStringLiteral("dontfloatPitchEditor"));
    setMinimumSize(720, 280);
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

    pitchGrid_ = new PitchGridWidget(this);
    pitchGrid_->setMinimumHeight(180);
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

    connect(pitchGrid_, &PitchGridWidget::notePitchEdited,
            this, &DontfloatPitchEditor::onNotePitchEdited);
    connect(pitchGrid_, &PitchGridWidget::notePreviewRequested,
            this, &DontfloatPitchEditor::onNotePreviewRequested);
    connect(pitchGrid_, &PitchGridWidget::notePreviewPitchChanged,
            this, &DontfloatPitchEditor::onNotePreviewPitchChanged);
    connect(pitchGrid_, &PitchGridWidget::notePreviewStopped,
            this, &DontfloatPitchEditor::onNotePreviewStopped);

    // Каретку двигают в плагине — просим DAW встать туда же
    connect(pitchGrid_, &PitchGridWidget::positionChanged, this, [this](qint64 positionMs) {
        if (applyingHostPlayhead_ || !session_) {
            return;
        }
        const int sampleRate = session_->audioBuffer().sampleRate;
        if (sampleRate > 0) {
            emit seekRequested((positionMs * sampleRate) / 1000);
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
    connect(pianoRollToolbar_, &PianoRollToolbar::exportMidiRequested,
            this, &DontfloatPitchEditor::onExportMidiClicked);
    pianoRollToolbar_->setExportMidiEnabled(false);
    pitchGrid_->setCutMode(pianoRollToolbar_->cutMode());

    setPrimaryKey(QStringLiteral("C Major"));
    setSecondaryKey(QString());
}

DontfloatPitchEditor::~DontfloatPitchEditor() = default;

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
    PitchDetector::PitchNote& note = baseNotes_[noteIndex];
    if (note.endSample - note.startSample < 2 * minPart) {
        setStatus(tr("the note is too short to split"));
        return;
    }
    const qint64 cutSample =
        qBound(note.startSample + minPart, splitSample, note.endSample - minPart);

    PitchDetector::PitchNote tail = note;
    tail.startSample = cutSample;
    note.endSample = cutSample;
    baseNotes_.insert(noteIndex + 1, tail);

    refreshPitchGrid();
    syncNotesToSession();
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
    const qint64 clamped = std::clamp<qint64>(
        samplePosition, 0, qint64(session_->audioBuffer().frameCount()));
    applyingHostPlayhead_ = true;
    pitchGrid_->setPlaybackPosition((clamped * 1000) / sampleRate);
    applyingHostPlayhead_ = false;
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

    // Тот же материал на новой позиции — клип переехал в DAW: ноты едут с ним
    qint64 shift = 0;
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
    Q_UNUSED(oldPitch);
    if (noteIndex < 0 || noteIndex >= baseNotes_.size()) {
        return;
    }
    baseNotes_[noteIndex].midiPitch = newPitch;
    pitchGrid_->setNotePitch(noteIndex, newPitch);
    syncNotesToSession();
    applyButton_->setEnabled(PitchCorrection::hasPendingEdits(baseNotes_));
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
    const qint64 length = qMax<qint64>(1, note.endSample - note.startSample);
    const QVector<float> segment = mono.mid(int(note.startSample), int(length));
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
