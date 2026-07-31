#include "dontfloat_pitch_editor.h"

#include "../../include/audiofileservice.h"
#include "../../include/keyanalyzer.h"
#include "../../include/keyselectionmenu.h"
#include "../../include/notepreviewplayer.h"
#include "../../include/pitchcorrection.h"
#include "../../include/pitchdetector.h"
#include "../../include/pitchgridwidget.h"
#include "../../include/wavwriter.h"

#include <QEvent>
#include <QFileDialog>
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
    , analysisWatcher_(new QFutureWatcher<PitchAnalysisOutcome>(this))
    , notePreviewPlayer_(new NotePreviewPlayer(this))
{
    setObjectName(QStringLiteral("dontfloatPitchEditor"));
    setMinimumSize(900, 560);
    setWindowTitle(productName_);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

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
    pitchGrid_->setMinimumHeight(320);
    pitchGrid_->setPrimaryKey(QStringLiteral("C Major"));
    root->addWidget(pitchGrid_, 1);

    auto* toolbar = new QHBoxLayout();
    importButton_ = new QPushButton(tr("Import WAV…"), this);
    analyzeButton_ = new QPushButton(tr("Analyze"), this);
    applyButton_ = new QPushButton(tr("Apply correction"), this);
    exportButton_ = new QPushButton(tr("Export WAV…"), this);
    applyButton_->setEnabled(false);
    toolbar->addWidget(importButton_);
    toolbar->addWidget(analyzeButton_);
    toolbar->addWidget(applyButton_);
    toolbar->addWidget(exportButton_);
    toolbar->addStretch(1);
    root->addLayout(toolbar);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(QStringLiteral("color:#aeb6c8;"));
    root->addWidget(statusLabel_);
    setStatus(tr("load audio or play a track in the DAW to capture the signal."));

    analyzeOverlay_ = new QWidget(this);
    analyzeOverlay_->setStyleSheet(QStringLiteral(
        "background-color: rgba(40, 40, 40, 180); border-radius: 4px;"));
    analyzeOverlay_->hide();

    auto* overlayLayout = new QVBoxLayout(analyzeOverlay_);
    overlayLayout->setContentsMargins(16, 12, 16, 12);
    auto* overlayAnalyzeButton = new QPushButton(tr("Analyze"), analyzeOverlay_);
    analyzeProgress_ = new QProgressBar(analyzeOverlay_);
    analyzeProgress_->setRange(0, 100);
    analyzeProgress_->setTextVisible(true);
    analyzeProgress_->hide();
    overlayLayout->addStretch(1);
    overlayLayout->addWidget(overlayAnalyzeButton, 0, Qt::AlignHCenter);
    overlayLayout->addWidget(analyzeProgress_);
    overlayLayout->addStretch(1);
    connect(overlayAnalyzeButton, &QPushButton::clicked, this, &DontfloatPitchEditor::onAnalyzeClicked);

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

    connect(keyMenu_, &KeySelectionMenu::keySelected, this, &DontfloatPitchEditor::onPrimaryKeySelected);
    connect(keyMenu2_, &KeySelectionMenu::keySelected, this, &DontfloatPitchEditor::onSecondaryKeySelected);
    connect(importButton_, &QPushButton::clicked, this, &DontfloatPitchEditor::onImportAudioClicked);
    connect(analyzeButton_, &QPushButton::clicked, this, &DontfloatPitchEditor::onAnalyzeClicked);
    connect(applyButton_, &QPushButton::clicked, this, &DontfloatPitchEditor::onApplyCorrectionClicked);
    connect(exportButton_, &QPushButton::clicked, this, &DontfloatPitchEditor::onExportClicked);
    connect(analysisWatcher_, &QFutureWatcher<PitchAnalysisOutcome>::finished,
            this, &DontfloatPitchEditor::onPitchAnalysisFinished);

    connect(pitchGrid_, &PitchGridWidget::notePitchEdited,
            this, &DontfloatPitchEditor::onNotePitchEdited);
    connect(pitchGrid_, &PitchGridWidget::notePreviewRequested,
            this, &DontfloatPitchEditor::onNotePreviewRequested);
    connect(pitchGrid_, &PitchGridWidget::notePreviewPitchChanged,
            this, &DontfloatPitchEditor::onNotePreviewPitchChanged);
    connect(pitchGrid_, &PitchGridWidget::notePreviewStopped,
            this, &DontfloatPitchEditor::onNotePreviewStopped);

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
    if (!statusLabel_) {
        return;
    }
    statusLabel_->setText(QStringLiteral("%1: %2").arg(productName_, text));
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
        analyzeOverlay_->setVisible(!session_->pitchAnalysis().valid && !analysisRunning_);
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
    refreshFromSession();
    if (!session_->pitchAnalysis().valid && !analysisRunning_) {
        analyzeOverlay_->show();
        analyzeOverlay_->raise();
    }
}

void DontfloatPitchEditor::setHostPlayhead(qint64 positionMs, bool playing)
{
    Q_UNUSED(playing);
    if (pitchGrid_) {
        pitchGrid_->setPlaybackPosition(positionMs);
    }
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
    importButton_->setEnabled(!running);
    analyzeButton_->setEnabled(!running);
    exportButton_->setEnabled(!running);
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

void DontfloatPitchEditor::onImportAudioClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("%1 — audio import").arg(productName_), QString(),
        tr("Audio Files (*.wav *.mp3 *.flac);;All Files (*)"));
    if (path.isEmpty() || !session_) {
        return;
    }

    const AudioFileService::DecodeResult decoded = AudioFileService::decode(path);
    if (!decoded.ok) {
        setStatus(tr("import error: %1").arg(decoded.error));
        return;
    }

    TrackAudioBuffer buffer;
    buffer.sampleRate = decoded.sampleRate;
    buffer.channelCount = decoded.channels.size();
    if (!decoded.channels.isEmpty()) {
        buffer.left = toStdVector(decoded.channels[0]);
    }
    if (decoded.channels.size() > 1) {
        buffer.right = toStdVector(decoded.channels[1]);
    }
    buffer.mono = toStdVector(AudioFileService::toMono(decoded.channels));

    session_->setAudioBuffer(buffer);
    session_->pitchAnalysis() = {};
    baseNotes_.clear();
    applyButton_->setEnabled(false);
    setSecondaryKey(QString());
    refreshFromSession();
    analyzeOverlay_->show();
    analyzeOverlay_->raise();
    emit pitchSessionChanged();
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

    analysisWatcher_->setFuture(QtConcurrent::run([mono, sampleRate, progress = analysisProgress_]() {
        PitchAnalysisOutcome outcome;
        progress->store(2);
        const KeyAnalyzer::AnalysisResult keyResult = KeyAnalyzer::analyzeKey(mono, sampleRate);
        outcome.primaryKeyName = keyNameFromInfo(keyResult.primaryKey);
        if (keyResult.hasKeyChange
            && keyResult.secondaryKey.key != KeyAnalyzer::UNKNOWN_KEY
            && !keyResult.secondaryKey.keyName.isEmpty()) {
            outcome.secondaryKeyName = keyResult.secondaryKey.keyName;
        }
        progress->store(15);
        const QVector<PitchDetector::PitchNote> notes = PitchDetector::detectNotes(
            mono, sampleRate, PitchDetector::Options(),
            [progress](int pct) { progress->store(15 + pct * 85 / 100); });
        progress->store(100);

        outcome.pitch.valid = true;
        outcome.pitch.notes = toCoreNotes(notes);
        outcome.pitch.keys.hasKeyChange = keyResult.hasKeyChange;
        return outcome;
    }));
}

void DontfloatPitchEditor::onAnalyzeClicked()
{
    runPitchAnalysis();
}

void DontfloatPitchEditor::onPitchAnalysisFinished()
{
    setAnalysisRunning(false);
    analyzeProgress_->setValue(100);

    const PitchAnalysisOutcome outcome = analysisWatcher_->result();
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
    refreshFromSession();
    applyButton_->setEnabled(false);
    setStatus(tr("correction applied — processed audio is in the plugin session"));
    emit pitchSessionChanged();
}

void DontfloatPitchEditor::onExportClicked()
{
    if (!session_ || session_->audioBuffer().empty()) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("%1 — WAV export").arg(productName_), QString(),
        tr("WAV (*.wav)"));
    if (path.isEmpty()) {
        return;
    }

    QVector<QVector<float>> channels;
    const TrackAudioBuffer& buffer = session_->audioBuffer();
    if (!buffer.left.empty()) {
        channels.append(toQVector(buffer.left));
        if (!buffer.right.empty()) {
            channels.append(toQVector(buffer.right));
        }
    } else if (!buffer.mono.empty()) {
        channels.append(toQVector(buffer.mono));
    }

    QString error;
    if (!WavWriter::writeFile(path, channels, buffer.sampleRate, &error)) {
        setStatus(tr("export error: %1").arg(error));
        return;
    }
    setStatus(tr("exported: %1").arg(path));
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
