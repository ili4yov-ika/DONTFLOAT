#include "dontfloat_plugin_editor_shell.h"

#include "../core/dontfloat_diagnostics.h"

#ifndef DONTFLOAT_PLUGIN_PRODUCT_INDEX
#error "DONTFLOAT_PLUGIN_PRODUCT_INDEX must be defined for plugin UI targets"
#endif

#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
#include "dontfloat_full_editor.h"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
#include "dontfloat_scratch_editor.h"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
#include "dontfloat_pitch_editor.h"
#else
#error "Invalid DONTFLOAT_PLUGIN_PRODUCT_INDEX"
#endif

#include "dontfloat_editor_content.h"
#include "dontfloat_plugin_theme.h"

#include "../../include/metronomecontroller.h"
#include "../../include/notepreviewplayer.h"

#include <QHBoxLayout>
#include <QIcon>

#include "../../include/svgiconloader.h"
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace Dontfloat::Plugins::Ui {
namespace {

// Размеры панели повторяют главное окно (ui/mainwindow.ui): кнопка 32×32,
// иконка 24×24, между группами — тот же зазор
constexpr int kToolButtonSizePx = 32;
constexpr int kToolIconSizePx = 24;
constexpr int kHeaderMarginPx = 6;
constexpr int kGroupSpacingPx = 6;
constexpr float kFallbackBpm = 120.0f;
/** Меньше этого окно плагина смысла не имеет: шапка + статусбар. */
constexpr int kShellMinWidthPx = 360;
constexpr int kShellMinHeightPx = 220;

} // namespace

DontfloatPluginEditorShell::DontfloatPluginEditorShell(
    Dontfloat::PluginCore::PluginProduct product, QWidget* parent)
    : QWidget(parent)
    , product_(product)
{
    const Dontfloat::PluginCore::PluginProductDesc& info =
        Dontfloat::PluginCore::productDesc(product_);
    const QString productName = QString::fromUtf8(info.clapName);

    setObjectName(QStringLiteral("dontfloatPluginEditorShell"));
    setWindowTitle(productName);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Разметка не запрещает окну быть маленьким — иначе хост не смог бы сжать
    // редактор, и он вылезал бы за свою рамку
    root->setSizeConstraint(QLayout::SetNoConstraint);
    setMinimumSize(kShellMinWidthPx, kShellMinHeightPx);

    root->addWidget(buildHeader());

#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
    using ContentEditor = DontfloatFullEditor;
    auto* editor = new ContentEditor(this);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
    using ContentEditor = DontfloatScratchEditor;
    auto* editor = new ContentEditor(this, productName);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
    using ContentEditor = DontfloatPitchEditor;
    auto* editor = new ContentEditor(this, productName);
#endif
    connect(editor, &ContentEditor::statusMessage,
            this, &DontfloatPluginEditorShell::showStatus);
    // Каретку двинули в плагине — просим хост встать туда же
    connect(editor, &ContentEditor::seekRequested, this, [this](qint64 samplePosition) {
        if (hostSeekHandler_) {
            hostSeekHandler_(samplePosition);
        }
    });
    // Плагин пересчитал звук — просим хост прогнать дорожку заново
    connect(editor, &ContentEditor::renderedOutputChanged, this, [this]() {
        if (hostRenderChangedHandler_) {
            hostRenderChangedHandler_();
        }
    });
    content_ = editor;
    contentWidget_ = editor;

    // Окно в DAW тянут как угодно, в том числе меньше, чем требует разметка.
    // Содержимое живёт в области прокрутки: пока места хватает — растягивается
    // как раньше, когда перестаёт — появляются полосы прокрутки, а не обрезанный
    // хвост интерфейса за рамкой окна хоста
    contentScroll_ = new QScrollArea(this);
    contentScroll_->setObjectName(QStringLiteral("dontfloatPluginContentScroll"));
    contentScroll_->setFrameShape(QFrame::NoFrame);
    contentScroll_->setWidgetResizable(true);
    contentScroll_->setWidget(contentWidget_);
    contentScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Своего фона у области прокрутки нет: иначе по краям окна светились
    // серые полосы вместо интерфейса
    contentScroll_->setStyleSheet(QStringLiteral(
        "QScrollArea#dontfloatPluginContentScroll { background: transparent; border: none; }"
        "QScrollArea#dontfloatPluginContentScroll > QWidget > QWidget { background: transparent; }"));
    contentScroll_->viewport()->setAutoFillBackground(false);
    contentScroll_->setMinimumSize(0, 0);
    root->addWidget(contentScroll_, 1);

    statusBar_ = new QLabel(this);
    statusBar_->setObjectName(QStringLiteral("dontfloatPluginStatus"));
    statusBar_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(statusBar_);

    // Инструменты волны есть не у всякой редакции (у Pitcher волны нет)
    const bool waveformTools = content_ && content_->hasWaveformTools();
    if (gridToolsGroup_) {
        gridToolsGroup_->setVisible(waveformTools);
    }
    if (loopToolsGroup_) {
        loopToolsGroup_->setVisible(waveformTools);
    }

    // NotePreviewPlayer и MetronomeController поднимают мультимедиа Qt, а это
    // при создании редактора может провернуть вложенный цикл событий у хоста.
    // Создаём их лениво — по первому нажатию (см. ensurePreviewPlayer).
    applyDontfloatWidgetTheme(this);
    showStatus(tr("Play the track in the DAW — the plugin will pick up the audio."));
}

QWidget* DontfloatPluginEditorShell::buildHeader()
{
    const Dontfloat::PluginCore::PluginProductDesc& info =
        Dontfloat::PluginCore::productDesc(product_);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("dontfloatPluginHeader"));
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(kHeaderMarginPx, kHeaderMarginPx, kHeaderMarginPx, kHeaderMarginPx);
    layout->setSpacing(kGroupSpacingPx);

    auto* title = new QLabel(QString::fromUtf8(info.clapName), header);
    title->setObjectName(QStringLiteral("productTitle"));
    title->setToolTip(QString::fromUtf8(info.clapDescription));
    layout->addWidget(title);

    // Группа сетки: OD / < / BG / > — сразу за названием (макет
    // MARKDOWN/example_plugin_dontfloat.svg), действия те же, что в главном окне
    gridToolsGroup_ = new QWidget(header);
    auto* gridLayout = new QHBoxLayout(gridToolsGroup_);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(kGroupSpacingPx);

    auto* onsetButton = makeToolButton(gridToolsGroup_, QStringLiteral("OD"), QString(),
                                       tr("Auto markers (onset detection)"));
    auto* gridBackButton = makeToolButton(gridToolsGroup_, QStringLiteral("<"), QString(),
                                          tr("Shift beat grid one beat back"));
    auto* gridSnapButton = makeToolButton(gridToolsGroup_, QStringLiteral("BG"), QString(),
                                          tr("Snap all markers to BPM grid"));
    auto* gridForwardButton = makeToolButton(gridToolsGroup_, QStringLiteral(">"), QString(),
                                             tr("Shift beat grid one beat forward"));
    gridLayout->addWidget(onsetButton);
    gridLayout->addWidget(gridBackButton);
    gridLayout->addWidget(gridSnapButton);
    gridLayout->addWidget(gridForwardButton);
    layout->addWidget(gridToolsGroup_);
    layout->addStretch(1);

    connect(onsetButton, &QPushButton::clicked, this, [this]() {
        if (content_) {
            content_->detectOnsetMarkers();
        }
    });
    connect(gridBackButton, &QPushButton::clicked, this, [this]() {
        if (content_) {
            content_->shiftBeatGrid(-1);
        }
    });
    connect(gridSnapButton, &QPushButton::clicked, this, [this]() {
        if (content_) {
            content_->snapMarkersToGrid();
        }
    });
    connect(gridForwardButton, &QPushButton::clicked, this, [this]() {
        if (content_) {
            content_->shiftBeatGrid(1);
        }
    });

    // Транспорт: кнопки повторяют транспорт DAW, своего проигрывателя нет
    playButton_ = makeToolButton(header, QString(),
                                 QStringLiteral(":/icons/resources/icons/play.svg"),
                                 tr("Start playback in the DAW"));
    stopButton_ = makeToolButton(header, QString(),
                                 QStringLiteral(":/icons/resources/icons/stop.svg"),
                                 tr("Stop playback in the DAW"));
    metronomeButton_ = makeToolButton(header, QString(),
                                      QStringLiteral(":/icons/resources/icons/metronome.svg"),
                                      tr("Metronome (during preview)"), true);
    layout->addWidget(playButton_);
    layout->addWidget(stopButton_);
    layout->addWidget(metronomeButton_);

    loopToolsGroup_ = new QWidget(header);
    auto* loopLayout = new QHBoxLayout(loopToolsGroup_);
    loopLayout->setContentsMargins(0, 0, 0, 0);
    loopLayout->setSpacing(kGroupSpacingPx);
    auto* loopStartButton = makeToolButton(loopToolsGroup_, QStringLiteral("A"), QString(),
                                           tr("Set loop start A at the cursor"));
    auto* loopEndButton = makeToolButton(loopToolsGroup_, QStringLiteral("B"), QString(),
                                         tr("Set loop end B at the cursor"));
    loopButton_ = makeToolButton(loopToolsGroup_, QString(),
                                 QStringLiteral(":/icons/resources/icons/loop.svg"),
                                 tr("Toggle loop"), true);
    loopLayout->addWidget(loopStartButton);
    loopLayout->addWidget(loopEndButton);
    loopLayout->addWidget(loopButton_);
    layout->addWidget(loopToolsGroup_);

    connect(loopStartButton, &QPushButton::clicked, this, [this]() {
        if (content_) {
            content_->setLoopBoundAtPlayhead(true);
        }
    });
    connect(loopEndButton, &QPushButton::clicked, this, [this]() {
        if (content_) {
            content_->setLoopBoundAtPlayhead(false);
        }
    });
    connect(loopButton_, &QPushButton::toggled, this, &DontfloatPluginEditorShell::onLoopToggled);
    connect(playButton_, &QPushButton::clicked,
            this, &DontfloatPluginEditorShell::onPreviewPlayClicked);
    connect(stopButton_, &QPushButton::clicked,
            this, &DontfloatPluginEditorShell::onPreviewStopClicked);
    connect(metronomeButton_, &QPushButton::toggled,
            this, &DontfloatPluginEditorShell::onMetronomeToggled);

    return header;
}

QPushButton* DontfloatPluginEditorShell::makeToolButton(QWidget* parent, const QString& text,
                                                        const QString& iconPath,
                                                        const QString& tooltip, bool checkable)
{
    auto* button = new QPushButton(text, parent);
    button->setFixedSize(kToolButtonSizePx, kToolButtonSizePx);
    button->setToolTip(tooltip);
    button->setCheckable(checkable);
    if (!iconPath.isEmpty()) {
        // SVG рисуем сами (QSvgRenderer): в DAW путь к плагинам Qt чужой, и
        // движок иконок не находится — транспорт оставался без значков
        button->setIcon(SvgIcons::load(iconPath, kToolIconSizePx, button->devicePixelRatioF()));
        button->setIconSize(QSize(kToolIconSizePx, kToolIconSizePx));
    }
    return button;
}

void DontfloatPluginEditorShell::setAraBinding(const void* extension)
{
    if (content_) {
        content_->setAraBinding(extension);
    }
}

void DontfloatPluginEditorShell::showStatus(const QString& text)
{
    if (statusBar_) {
        statusBar_->setText(text);
    }
}

QVector<float> DontfloatPluginEditorShell::sessionMonoMix(int* sampleRateOut) const
{
    if (sampleRateOut) {
        *sampleRateOut = 0;
    }
    if (!session_) {
        return {};
    }
    const Dontfloat::PluginCore::TrackAudioBuffer& buffer = session_->audioBuffer();
    if (buffer.mono.empty()) {
        return {};
    }
    if (sampleRateOut) {
        *sampleRateOut = buffer.sampleRate;
    }
    QVector<float> mono(static_cast<int>(buffer.mono.size()));
    std::copy(buffer.mono.begin(), buffer.mono.end(), mono.begin());
    return mono;
}

NotePreviewPlayer* DontfloatPluginEditorShell::ensurePreviewPlayer()
{
    if (!previewPlayer_) {
        previewPlayer_ = new NotePreviewPlayer(this);
    }
    return previewPlayer_;
}

MetronomeController* DontfloatPluginEditorShell::ensureMetronome()
{
    if (!metronome_) {
        metronome_ = new MetronomeController(this);
        metronome_->setBPM(kFallbackBpm);
    }
    return metronome_;
}

void DontfloatPluginEditorShell::setHostTransportPlaying(bool playing)
{
    if (hostPlaying_ == playing) {
        return;
    }
    hostPlaying_ = playing;
    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        Dontfloat::PluginCore::Diagnostics::log(playing ? "host.transport playing=1"
                                                        : "host.transport playing=0");
    }
    if (!playButton_) {
        return;
    }
    // Кнопка повторяет транспорт хоста: играет — показываем паузу
    playButton_->setIcon(SvgIcons::load(
        playing ? QStringLiteral(":/icons/resources/icons/pause.svg")
                : QStringLiteral(":/icons/resources/icons/play.svg"),
        kToolIconSizePx, playButton_->devicePixelRatioF()));
    playButton_->setToolTip(playing ? tr("Stop playback in the DAW")
                                    : tr("Start playback in the DAW"));
}

void DontfloatPluginEditorShell::onPreviewPlayClicked()
{
    // Кнопка — дубликат кнопки воспроизведения в DAW, и ничего больше:
    // играет — останавливаем, стоит — запускаем. Своего проигрывателя тут нет
    // намеренно. Раньше при недоступном транспорте кнопка слушала кусок сама и
    // в полной редакции выдавала звук референсного канала — не того, что видно
    if (content_ && content_->requestHostTransport(!hostPlaying_)) {
        return;
    }
    showStatus(tr("The DAW did not hand over transport control."));
}

void DontfloatPluginEditorShell::onPreviewStopClicked()
{
    // Останавливаем тем же путём, каким запускали
    if (content_ && content_->requestHostTransport(false)) {
        return;
    }
    showStatus(tr("The DAW did not hand over transport control."));
}

void DontfloatPluginEditorShell::onMetronomeToggled(bool enabled)
{
    if (!enabled && !metronome_) {
        return;  // выключили то, что ещё не поднимали
    }
    // Темп берём из анализа дорожки, пока его нет — 120
    float bpm = kFallbackBpm;
    if (session_ && session_->analysisValid() && session_->analysis().bpm > 0.0f) {
        bpm = session_->analysis().bpm;
    }
    MetronomeController* metronome = ensureMetronome();
    metronome->setBPM(bpm);
    metronome->setEnabled(enabled);
    metronome->setPlaying(enabled && previewPlayer_ && previewPlayer_->isActive());
}

void DontfloatPluginEditorShell::onLoopToggled(bool enabled)
{
    if (!content_) {
        return;
    }
    content_->setLoopEnabled(enabled);
    // Без точек A и B включать нечего — кнопка не должна оставаться нажатой
    if (enabled && !content_->loopRegionMs(nullptr, nullptr) && loopButton_) {
        QSignalBlocker blocker(loopButton_);
        loopButton_->setChecked(false);
    }
}

void DontfloatPluginEditorShell::bindSession(Dontfloat::PluginCore::TrackToolSession* session)
{
    session_ = session;
    if (content_) {
        content_->bindSession(session);
    }
}

void DontfloatPluginEditorShell::notifyHostAudioAppended()
{
    if (content_) {
        content_->notifyHostAudioAppended();
    }
}

void DontfloatPluginEditorShell::setHostPlayheadSeconds(double projectSeconds)
{
    if (content_) {
        content_->setHostPlayheadSeconds(projectSeconds);
    }
}

void DontfloatPluginEditorShell::setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample)
{
    if (content_) {
        content_->setHostBeatGrid(bpm, beatsPerBar, barStartSample);
    }
    // Метроном идёт в темпе DAW, если пользователь его включил
    if (metronome_ && bpm > 0.0) {
        metronome_->setBPM(float(bpm));
    }
}

} // namespace Dontfloat::Plugins::Ui
