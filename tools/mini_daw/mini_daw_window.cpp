#include "mini_daw_window.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>
#include <QtGui/QDoubleValidator>
#include <QtGui/QMouseEvent>
#include <QtGui/QShortcut>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QPolygonF>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>

#include "audiofileservice.h"
#include "pianoroll_engine.h"
#include "plugin_product.h"

namespace MiniDaw {

namespace {

// Геометрия макета MARKDOWN/example_window_minidaw.svg
constexpr int kButtonSize = 20;
constexpr int kBarMargin = 4;
constexpr int kFormatComboWidth = 127;
constexpr int kTrackComboWidth = 34;  ///< «1/2» из макета
constexpr int kBpmFieldWidth = 69;
constexpr int kBeatsFieldWidth = 25;
constexpr int kBlockSize = 512;
constexpr float kDefaultBpm = 120.0f;
constexpr int kDefaultBeatsPerBar = 4;

/**
 * Приводит канал к частоте сессии линейной интерполяцией.
 * Нужно, когда во вторую дорожку кладут файл с другой частотой: смешивать в
 * мастер-шину и отдавать плагинам можно только материал одной частоты.
 */
QVector<float> resampleLinear(const QVector<float>& input, int fromRate, int toRate)
{
    if (input.isEmpty() || fromRate <= 0 || toRate <= 0 || fromRate == toRate) {
        return input;
    }
    const double ratio = double(toRate) / double(fromRate);
    const int outFrames = int(double(input.size()) * ratio);
    QVector<float> output(outFrames, 0.0f);
    for (int i = 0; i < outFrames; ++i) {
        const double sourcePos = double(i) / ratio;
        const int base = int(sourcePos);
        const int next = std::min(base + 1, int(input.size()) - 1);
        const float t = float(sourcePos - double(base));
        output[i] = input[base] * (1.0f - t) + input[next] * t;
    }
    return output;
}

QString editionLabel(PluginProduct product)
{
    return QString::fromUtf8(
        Dontfloat::PluginCore::productDescByIndex(static_cast<int>(product)).clapName);
}

/** Иконка «открыть файл» из макета: папка с корешком. */
QIcon buildOpenIcon()
{
    QPixmap pixmap(kButtonSize, kButtonSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(QRect(6, 6, 8, 8), QColor(0xD9, 0xD9, 0xD9));
    painter.fillRect(QRect(7, 10, 6, 3), Qt::black);
    painter.fillRect(QRect(8, 6, 4, 2), Qt::white);
    painter.end();
    return QIcon(pixmap);
}

/** Иконка «воспроизведение»: белый треугольник. */
QIcon buildPlayIcon()
{
    QPixmap pixmap(kButtonSize, kButtonSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPolygonF triangle;
    triangle << QPointF(8, 6.5) << QPointF(14, 10) << QPointF(8, 13.5);
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(triangle);
    painter.end();
    return QIcon(pixmap);
}

/** Иконка «стоп»: белый квадрат. */
QIcon buildStopIcon()
{
    QPixmap pixmap(kButtonSize, kButtonSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(QRect(6, 6, 8, 8), Qt::white);
    painter.end();
    return QIcon(pixmap);
}

} // namespace

// ------------------------------------------------------------ PlaybackBar --

PlaybackBar::PlaybackBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kBarHeight);
    setCursor(Qt::PointingHandCursor);
}

QRect PlaybackBar::tracksRect() const
{
    return QRect(0, kCaretHeight, width(), kTrackCount * kTrackHeight);
}

QRect PlaybackBar::trackRect(int index) const
{
    const int clamped = std::clamp(index, 0, kTrackCount - 1);
    return QRect(0, kCaretHeight + clamped * kTrackHeight, width(), kTrackHeight);
}

int PlaybackBar::trackAtY(int y) const
{
    if (y < kCaretHeight) {
        return -1;  // клик по каретке — дорожку не меняем
    }
    const int index = (y - kCaretHeight) / kTrackHeight;
    return index < kTrackCount ? index : -1;
}

qint64 PlaybackBar::trackFrames(int index) const
{
    return (index >= 0 && index < kTrackCount) ? lanes_[index].frames : 0;
}

void PlaybackBar::setTrack(int index, const QVector<float>& left, const QVector<float>& right,
                           int sampleRate)
{
    if (index < 0 || index >= kTrackCount) {
        return;
    }
    Lane& lane = lanes_[index];
    lane.channels.clear();
    if (!left.isEmpty()) {
        lane.channels.append(left);
        if (!right.isEmpty()) {
            lane.channels.append(right);
        }
    }
    lane.frames = lane.channels.isEmpty() ? 0 : lane.channels.first().size();
    sampleRate_ = sampleRate > 0 ? sampleRate : 44100;

    // Общая длина — по самой длинной дорожке: каретка одна на все
    totalFrames_ = 0;
    for (const Lane& other : lanes_) {
        totalFrames_ = std::max(totalFrames_, other.frames);
    }
    position_ = std::min(position_, totalFrames_);
    for (int i = 0; i < kTrackCount; ++i) {
        rebuildEnvelope(i);
    }
    update();
}

void PlaybackBar::clearTrack(int index)
{
    if (index < 0 || index >= kTrackCount) {
        return;
    }
    lanes_[index] = Lane {};
    totalFrames_ = 0;
    for (const Lane& other : lanes_) {
        totalFrames_ = std::max(totalFrames_, other.frames);
    }
    position_ = std::min(position_, totalFrames_);
    update();
}

void PlaybackBar::setActiveTrack(int index)
{
    activeTrack_ = std::clamp(index, 0, kTrackCount - 1);
    update();
}

void PlaybackBar::setPosition(qint64 frames)
{
    const qint64 clamped = std::clamp<qint64>(frames, 0, totalFrames_);
    if (clamped == position_) {
        return;
    }
    position_ = clamped;
    update();
}

void PlaybackBar::setBeatGrid(float bpm, int beatsPerBar)
{
    bpm_ = bpm > 0.0f ? bpm : 120.0f;
    beatsPerBar_ = std::max(1, beatsPerBar);
    update();
}

void PlaybackBar::setTrackName(int index, const QString& name)
{
    if (index < 0 || index >= kTrackCount) {
        return;
    }
    lanes_[index].name = name;
    update();
}

void PlaybackBar::rebuildEnvelope(int index)
{
    if (index < 0 || index >= kTrackCount) {
        return;
    }
    Lane& lane = lanes_[index];
    lane.envelopeUpper.clear();
    lane.envelopeLower.clear();
    const QRect area = trackRect(index);
    if (lane.channels.isEmpty() || area.width() <= 0 || area.height() <= 2) {
        return;
    }
    // Огибающая считается тем же движком, что рисует волну в приложении
    const auto envelope =
        PianoRollEngine::buildWaveformEnvelope(lane.channels, area.width(), area.height());
    lane.envelopeUpper = envelope.upper;
    lane.envelopeLower = envelope.lower;
}

void PlaybackBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    for (int i = 0; i < kTrackCount; ++i) {
        rebuildEnvelope(i);
    }
}

QString PlaybackBar::formatTime(qint64 frames, int sampleRate)
{
    const qint64 totalSeconds = sampleRate > 0 ? frames / sampleRate : 0;
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

qint64 PlaybackBar::frameAtX(int x) const
{
    const QRect area = tracksRect();
    if (totalFrames_ <= 0 || area.width() <= 0) {
        return 0;
    }
    const double ratio = std::clamp(double(x - area.left()) / double(area.width()), 0.0, 1.0);
    return qint64(ratio * double(totalFrames_));
}

void PlaybackBar::setClipBoundaries(int index, const QVector<qint64>& boundaries)
{
    if (index < 0 || index >= kTrackCount) {
        return;
    }
    lanes_[index].clipBoundaries = boundaries;
    update();
}

void PlaybackBar::drawWaveform(QPainter& painter, int index, const QRect& area) const
{
    const Lane& lane = lanes_[index];
    if (lane.envelopeUpper.isEmpty()) {
        return;
    }
    painter.setPen(QPen(QColor(0x3A, 0x3A, 0x3A), 1.0));
    const int columns = std::min(int(lane.envelopeUpper.size()), area.width());
    for (int x = 0; x < columns; ++x) {
        const int top = area.top() + lane.envelopeUpper[x];
        const int bottom = area.top() + lane.envelopeLower[x];
        painter.drawLine(area.left() + x, top, area.left() + x, bottom);
    }
}

void PlaybackBar::drawBeatGrid(QPainter& painter, const QRect& area) const
{
    if (totalFrames_ <= 0 || sampleRate_ <= 0 || bpm_ <= 0.0f) {
        return;
    }
    const auto metrics = PianoRollEngine::computeBeatGridMetrics(bpm_, beatsPerBar_, sampleRate_);
    if (metrics.samplesPerSubdivision <= 0.0f) {
        return;
    }

    const double pixelsPerSample = double(area.width()) / double(totalFrames_);
    const double step = double(metrics.samplesPerSubdivision) * pixelsPerSample;
    if (step < 2.0) {
        return;  // сетка гуще пикселя — не рисуем кашу
    }

    for (int index = 0;; ++index) {
        const double x = area.left() + index * step;
        if (x > area.right()) {
            break;
        }
        const bool isBarLine = (index % metrics.subdivisionsPerBar) == 0;
        painter.setPen(QPen(isBarLine ? QColor(0x70, 0x70, 0x70) : QColor(0xC0, 0xC0, 0xC0),
                            isBarLine ? 1.5 : 1.0));
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
}

void PlaybackBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const QRect allTracks = tracksRect();

    const auto xForFrame = [&](qint64 frame) {
        return totalFrames_ > 0 ? double(frame) / double(totalFrames_) * allTracks.width() : 0.0;
    };
    const double playedX = totalFrames_ > 0 ? xForFrame(position_) : 0.0;

    // Цвета проигранной части как на макете: первая дорожка красная, вторая оранжевая
    const QColor playedColors[kTrackCount] = { QColor(0xFF, 0x00, 0x00, 128),
                                               QColor(0xFF, 0x99, 0x33, 140) };

    for (int index = 0; index < kTrackCount; ++index) {
        const QRect area = trackRect(index);
        painter.fillRect(area, Qt::white);
        drawBeatGrid(painter, area);
        drawWaveform(painter, index, area);

        painter.setPen(QPen(QColor(0x20, 0x60, 0xC0), 1.0, Qt::DashLine));
        for (qint64 boundary : lanes_[index].clipBoundaries) {
            if (boundary <= 0 || boundary >= totalFrames_) {
                continue;
            }
            const double x = area.left() + xForFrame(boundary);
            painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        }

        if (playedX > 0.0 && lanes_[index].frames > 0) {
            painter.fillRect(QRectF(area.left(), area.top(), playedX, area.height()),
                             playedColors[index]);
        }

        // Выбранная дорожка обведена ярче: её редактор показан ниже
        const bool active = index == activeTrack_;
        painter.setPen(QPen(active ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x60, 0x60, 0x60),
                            active ? 2.0 : 1.0));
        painter.drawRect(area.adjusted(0, 0, -1, -1));

        if (!lanes_[index].name.isEmpty()) {
            QFont nameFont = painter.font();
            nameFont.setPointSize(8);
            painter.setFont(nameFont);
            painter.setPen(QColor(0x20, 0x20, 0x20));
            painter.drawText(area.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                             lanes_[index].name);
        }
    }

    // Каретка: треугольник над дорожками и линия через обе
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (totalFrames_ > 0) {
        QPolygonF caret;
        caret << QPointF(playedX, allTracks.top())
              << QPointF(playedX - 3.5, allTracks.top() - kCaretHeight + 2)
              << QPointF(playedX + 3.5, allTracks.top() - kCaretHeight + 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xFF, 0x00, 0x00));
        painter.drawPolygon(caret);
        painter.setPen(QPen(QColor(0xFF, 0x00, 0x00), 1.0));
        painter.drawLine(QPointF(playedX, allTracks.top()), QPointF(playedX, allTracks.bottom()));
    }

    // Время — под дорожками, слева прошло / справа всего
    QFont timeFont = painter.font();
    timeFont.setPointSize(7);
    painter.setFont(timeFont);
    painter.setPen(Qt::white);
    const QRect timeRow(0, allTracks.bottom() + 1, width(), kTimeRowHeight);
    painter.drawText(timeRow, Qt::AlignVCenter | Qt::AlignLeft,
                     formatTime(position_, sampleRate_));
    painter.drawText(timeRow, Qt::AlignVCenter | Qt::AlignRight,
                     formatTime(totalFrames_, sampleRate_));
}

void PlaybackBar::mousePressEvent(QMouseEvent* event)
{
    // Клик по дорожке и выбирает её (как в DAW), и переставляет каретку
    const int lane = trackAtY(int(event->position().y()));
    if (lane >= 0 && lane != activeTrack_) {
        emit trackClicked(lane);
    }
    if (event->button() == Qt::LeftButton && totalFrames_ > 0) {
        emit seekRequested(frameAtX(int(event->position().x())));
    } else if (event->button() == Qt::RightButton && totalFrames_ > 0) {
        // ПКМ тащит клип по дорожке — так проверяется перенос клипа в DAW
        draggingClip_ = true;
        dragStartX_ = int(event->position().x());
        dragDeltaFrames_ = 0;
        setCursor(Qt::ClosedHandCursor);
    }
}

void PlaybackBar::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && totalFrames_ > 0) {
        emit seekRequested(frameAtX(int(event->position().x())));
        return;
    }
    if (draggingClip_ && totalFrames_ > 0) {
        const QRect area = tracksRect();
        const int dx = int(event->position().x()) - dragStartX_;
        dragDeltaFrames_ =
            qint64(double(dx) / std::max(1, area.width()) * double(totalFrames_));
    }
}

void PlaybackBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && draggingClip_) {
        draggingClip_ = false;
        unsetCursor();
        if (dragDeltaFrames_ != 0) {
            emit clipMoveRequested(dragDeltaFrames_);
        }
        dragDeltaFrames_ = 0;
    }
}

// ----------------------------------------------------------------- Window --

Window::Window(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyStyle();
    updateWindowTitle();
    resize(1100, 860);

    positionTimer_ = new QTimer(this);
    positionTimer_->setInterval(33); // ~30 fps
    connect(positionTimer_, &QTimer::timeout, this, &Window::tickPosition);

    // Транспорт уже остановлен самим плеером — здесь только обновление UI
    connect(&player_, &Player::finished, this, [this]() {
        playbackBar_->setPosition(player_.totalFrames());
        updateTransportUi();
    });

    QTimer::singleShot(0, this, [this]() { reloadPlugin(0); });
}

Window::~Window()
{
    player_.stop();
#if defined(DONTFLOAT_WITH_ARA)
    // Порядок разрушения задан ARA: сначала клипы снимаются с ролей (экземпляры
    // ещё живы и могут их отпустить), потом уходят сами экземпляры, и только
    // затем — документ с его контроллером
    if (araPumpTimer_) {
        araPumpTimer_->stop();
    }
    for (int i = 0; i < kTrackCount; ++i) {
        releaseAraTrack(i);
    }
#endif
    for (TrackState& state : tracks_) {
        if (state.host) {
            state.host->unload();
        }
    }
#if defined(DONTFLOAT_WITH_ARA)
    araSessions_.clear();
#endif
}

void Window::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- верхняя панель ----
    auto* topBar = new QWidget(this);
    topBar->setObjectName(QStringLiteral("miniDawTopBar"));
    topBar->setAttribute(Qt::WA_StyledBackground, true);
    // Панель = кнопки + дорожки со временем (высоты из макета)
    topBar->setFixedHeight(kBarMargin + kButtonSize + kBarMargin + PlaybackBar::kBarHeight);

    auto* topLayout = new QVBoxLayout(topBar);
    topLayout->setContentsMargins(kBarMargin, kBarMargin, kBarMargin, 0);
    topLayout->setSpacing(0);

    auto* controls = new QHBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(kBarMargin);

    auto makeButton = [this, topBar](const QString& objectName, const QIcon& icon,
                                     const QString& tip) {
        auto* button = new QToolButton(topBar);
        button->setObjectName(objectName);
        button->setIcon(icon);
        button->setIconSize(QSize(kButtonSize, kButtonSize));
        button->setFixedSize(kButtonSize, kButtonSize);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(tip);
        return button;
    };

    openButton_ = makeButton(QStringLiteral("miniDawOpenButton"), buildOpenIcon(),
                             tr("Open audio file"));
    controls->addWidget(openButton_);

    // «1/2» из макета: какая дорожка активна — в неё грузится файл и её
    // редактор плагина показан в рамке ниже
    trackCombo_ = new QComboBox(topBar);
    trackCombo_->setObjectName(QStringLiteral("miniDawTrackCombo"));
    for (int i = 0; i < kTrackCount; ++i) {
        trackCombo_->addItem(QStringLiteral("%1/%2").arg(i + 1).arg(kTrackCount), i);
    }
    trackCombo_->setFixedSize(kTrackComboWidth, kButtonSize);
    trackCombo_->setToolTip(tr("Track (each track has its own plugin instance)"));
    controls->addWidget(trackCombo_);

    formatCombo_ = new QComboBox(topBar);
    formatCombo_->setObjectName(QStringLiteral("miniDawFormatCombo"));
    // VST3 первым и по умолчанию: ARA по-человечески поддерживают только
    // VST3-хосты, поэтому проверять плагин начинают с него
    formatCombo_->addItem(QStringLiteral("VST3"), int(PluginFormat::Vst3));
    formatCombo_->addItem(QStringLiteral("CLAP"), int(PluginFormat::Clap));
    formatCombo_->addItem(QStringLiteral("LV2"), int(PluginFormat::Lv2));
    formatCombo_->setFixedWidth(kFormatComboWidth);
    formatCombo_->setFixedHeight(kButtonSize);
    formatCombo_->setToolTip(tr("Plugin format"));
    controls->addWidget(formatCombo_);

    productCombo_ = new QComboBox(topBar);
    productCombo_->setObjectName(QStringLiteral("miniDawProductCombo"));
    for (int i = 0; i < Dontfloat::PluginCore::kPluginProductCount; ++i) {
        const auto product = static_cast<PluginProduct>(i);
        productCombo_->addItem(editionLabel(product), i);
    }
    productCombo_->setFixedHeight(kButtonSize);
    productCombo_->setToolTip(tr("Plugin edition"));
    controls->addWidget(productCombo_, 1);

    // Темп хоста: сетка на дорожке строится по нему
    bpmEdit_ = new QLineEdit(topBar);
    bpmEdit_->setObjectName(QStringLiteral("miniDawBpmEdit"));
    bpmEdit_->setFixedSize(kBpmFieldWidth, kButtonSize);
    bpmEdit_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bpmEdit_->setValidator(new QDoubleValidator(20.0, 400.0, 2, bpmEdit_));
    bpmEdit_->setText(QString::number(kDefaultBpm, 'f', 2));
    bpmEdit_->setToolTip(tr("Host tempo (BPM)"));
    controls->addWidget(bpmEdit_);

    // Размер такта: определяет, где на дорожке линии тактов
    beatsCombo_ = new QComboBox(topBar);
    beatsCombo_->setObjectName(QStringLiteral("miniDawBeatsCombo"));
    for (int beats : { 4, 3, 2, 6, 12 }) {
        beatsCombo_->addItem(QStringLiteral("%1/4").arg(beats), beats);
    }
    beatsCombo_->setFixedSize(kBeatsFieldWidth + 12, kButtonSize);
    beatsCombo_->setToolTip(tr("Time signature"));
    controls->addWidget(beatsCombo_);

    playButton_ = makeButton(QStringLiteral("miniDawPlayButton"), buildPlayIcon(), tr("Play"));
    stopButton_ = makeButton(QStringLiteral("miniDawStopButton"), buildStopIcon(), tr("Stop"));
    controls->addWidget(playButton_);
    controls->addWidget(stopButton_);
    topLayout->addLayout(controls);

    playbackBar_ = new PlaybackBar(topBar);
    topLayout->addWidget(playbackBar_);
    root->addWidget(topBar);

    // ---- область плагина в красной рамке ----
    pluginFrame_ = new QWidget(this);
    pluginFrame_->setObjectName(QStringLiteral("miniDawPluginFrame"));
    pluginFrame_->setAttribute(Qt::WA_StyledBackground, true);

    auto* frameLayout = new QVBoxLayout(pluginFrame_);
    frameLayout->setContentsMargins(1, 1, 1, 1);
    frameLayout->setSpacing(0);

    // По странице на дорожку: видна страница активной, остальные экземпляры
    // плагина живут дальше — так они и обмениваются нотами-референсами
    pluginStack_ = new QStackedWidget(pluginFrame_);
    frameLayout->addWidget(pluginStack_);

    for (int i = 0; i < kTrackCount; ++i) {
        auto* surface = new QWidget(pluginStack_);
        surface->setObjectName(QStringLiteral("miniDawPluginSurface"));
        surface->setAttribute(Qt::WA_StyledBackground, true);
        // Редактор плагина парентится в это нативное окно, как в настоящей DAW
        surface->setAttribute(Qt::WA_NativeWindow, true);
        surface->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

        auto* message = new QLabel(surface);
        message->setObjectName(QStringLiteral("miniDawPluginMessage"));
        message->setAlignment(Qt::AlignCenter);
        message->setWordWrap(true);

        tracks_[i].surface = surface;
        tracks_[i].message = message;
        pluginStack_->addWidget(surface);
    }

    root->addWidget(pluginFrame_, 1);

    trackCombo_->installEventFilter(this);
    formatCombo_->installEventFilter(this);
    productCombo_->installEventFilter(this);
    formatCombo_->setFocusPolicy(Qt::StrongFocus);
    productCombo_->setFocusPolicy(Qt::StrongFocus);

    connect(openButton_, &QToolButton::clicked, this, &Window::onOpenClicked);
    connect(playButton_, &QToolButton::clicked, this, &Window::onPlayClicked);
    connect(stopButton_, &QToolButton::clicked, this, &Window::onStopClicked);
    connect(playbackBar_, &PlaybackBar::seekRequested, this, &Window::onSeekRequested);
    connect(playbackBar_, &PlaybackBar::clipMoveRequested, this, &Window::onClipMoveRequested);
    connect(playbackBar_, &PlaybackBar::trackClicked, this, &Window::setActiveTrack);

    // Правка клипов с клавиатуры — то же, что делают мышью в DAW:
    // S — рез по каретке, Ctrl+←/→ — сдвиг, Alt/Shift+←/→ — обрезка краёв,
    // Ctrl+↑/↓ — растянуть/сжать во времени
    const auto addShortcut = [this](const QKeySequence& keys, auto slot) {
        auto* shortcut = new QShortcut(keys, this);
        // Клавиши правки клипа работают при любом фокусе внутри хоста: фокус
        // часто у встроенного редактора плагина (это отдельное нативное окно)
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    addShortcut(QKeySequence(Qt::Key_S), [this]() { splitClipAtPlayhead(); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), [this]() { nudgeClip(1); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), [this]() { nudgeClip(-1); });
    addShortcut(QKeySequence(Qt::ALT | Qt::Key_Right),
                [this]() { trimSelectedClip(true, sampleRate_ / 4); });
    addShortcut(QKeySequence(Qt::ALT | Qt::Key_Left),
                [this]() { trimSelectedClip(true, -(sampleRate_ / 4)); });
    addShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Right),
                [this]() { trimSelectedClip(false, sampleRate_ / 4); });
    addShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Left),
                [this]() { trimSelectedClip(false, -(sampleRate_ / 4)); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up), [this]() { stretchSelectedClip(1.05); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down),
                [this]() { stretchSelectedClip(1.0 / 1.05); });
    // Ctrl+1/Ctrl+2 — быстрый переход между дорожками
    for (int i = 0; i < kTrackCount; ++i) {
        addShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + i)), [this, i]() { setActiveTrack(i); });
    }

    connect(trackCombo_, &QComboBox::currentIndexChanged, this, &Window::onTrackChanged);
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, &Window::onSelectionChanged);
    connect(productCombo_, &QComboBox::currentIndexChanged, this, &Window::onSelectionChanged);
    connect(bpmEdit_, &QLineEdit::editingFinished, this, &Window::onBeatGridChanged);
    connect(beatsCombo_, &QComboBox::currentIndexChanged, this, &Window::onBeatGridChanged);
    beatsCombo_->installEventFilter(this);

    // Стартовый выбор списков — исходное состояние обеих дорожек
    for (TrackState& state : tracks_) {
        state.format = currentFormat();
        state.product = currentProduct();
    }
    for (int i = 0; i < kTrackCount; ++i) {
        playbackBar_->setTrackName(i, tr("Track %1").arg(i + 1));
    }
    playbackBar_->setActiveTrack(0);
    onBeatGridChanged();
}

void Window::applyStyle()
{
    // Цвета макета: фон #404040, панель — чёрный 50%, комбобоксы #9F9F9F
    setStyleSheet(QStringLiteral(
        "QWidget { background-color: #404040; color: #F0F0F0; }"
        "QWidget#miniDawTopBar { background-color: rgba(0, 0, 0, 128); }"
        "QWidget#miniDawPluginFrame { background-color: #FF0000; }"
        "QWidget#miniDawPluginSurface { background-color: #404040; }"
        "QLabel#miniDawPluginMessage { background: transparent; color: #E0E0E0;"
        " font-size: 12px; padding: 24px; }"
        "QToolButton { border: none; }"
        "QToolButton#miniDawOpenButton { background-color: rgba(0, 255, 234, 128); }"
        "QToolButton#miniDawPlayButton { background-color: rgba(128, 255, 0, 128); }"
        "QToolButton#miniDawStopButton { background-color: #FF0000; }"
        "QToolButton:hover { background-color: rgba(255, 255, 255, 60); }"
        "QToolButton#miniDawOpenButton:hover { background-color: rgba(0, 255, 234, 200); }"
        "QToolButton#miniDawPlayButton:hover { background-color: rgba(128, 255, 0, 200); }"
        "QToolButton#miniDawStopButton:hover { background-color: #FF4040; }"
        "QComboBox { background-color: #9F9F9F; color: #FFFFFF; border: none;"
        " padding-left: 6px; }"
        "QComboBox::drop-down { border: none; width: 14px; }"
        "QComboBox QAbstractItemView { background-color: #4A4A4A; color: #FFFFFF;"
        " selection-background-color: #6E6E6E; }"
        // Номер дорожки: узкий список без стрелки, как «1/2» на макете
        "QComboBox#miniDawTrackCombo { padding-left: 3px; }"
        "QComboBox#miniDawTrackCombo::drop-down { width: 0px; }"
        // Поле темпа: зелёная рамка, значение на зелёной подложке (макет)
        "QLineEdit#miniDawBpmEdit { background-color: rgba(58, 217, 0, 178);"
        " border: 1px solid #3AD900; color: #FFFFFF; padding-right: 4px; }"
        // Размер такта: жёлтая рамка
        "QComboBox#miniDawBeatsCombo { background-color: transparent;"
        " border: 1px solid #FFF714; color: #FFFFFF; padding-left: 3px; }"
        "QComboBox#miniDawBeatsCombo::drop-down { width: 10px; }"));
}

PluginFormat Window::currentFormat() const
{
    return static_cast<PluginFormat>(formatCombo_->currentData().toInt());
}

PluginProduct Window::currentProduct() const
{
    return static_cast<PluginProduct>(productCombo_->currentData().toInt());
}

void Window::syncPluginCombos()
{
    const QSignalBlocker formatBlocker(formatCombo_);
    const QSignalBlocker productBlocker(productCombo_);
    const int formatIndex = formatCombo_->findData(int(track().format));
    const int productIndex = productCombo_->findData(int(track().product));
    if (formatIndex >= 0) {
        formatCombo_->setCurrentIndex(formatIndex);
    }
    if (productIndex >= 0) {
        productCombo_->setCurrentIndex(productIndex);
    }
}

bool Window::eventFilter(QObject* watched, QEvent* event)
{
    // Прокрутка колесом над списком незаметно меняла формат/редакцию, а с ними
    // перезагружался плагин и сбрасывался транспорт
    if (event->type() == QEvent::Wheel
        && (watched == formatCombo_ || watched == productCombo_ || watched == beatsCombo_
            || watched == trackCombo_)) {
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void Window::onBeatGridChanged()
{
    const float bpm = bpmEdit_->text().toFloat();
    const int beatsPerBar = beatsCombo_->currentData().toInt();
    const float effectiveBpm = bpm > 0.0f ? bpm : kDefaultBpm;
    const int effectiveBeats = beatsPerBar > 0 ? beatsPerBar : kDefaultBeatsPerBar;

    playbackBar_->setBeatGrid(effectiveBpm, effectiveBeats);
    // Темп и размер такта уходят всем плагинам: сетка у дорожек общая
    for (TrackState& state : tracks_) {
        if (state.host) {
            state.host->setTransport(effectiveBpm, effectiveBeats);
        }
    }
}

void Window::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    for (TrackState& state : tracks_) {
        if (state.message && state.surface) {
            state.message->setGeometry(state.surface->rect());
        }
        if (state.host && state.surface) {
            state.host->resizeEditor(state.surface->size());
        }
    }
}

void Window::showPluginMessage(int trackIndex, const QString& text, bool isError)
{
    if (trackIndex < 0 || trackIndex >= kTrackCount) {
        return;
    }
    TrackState& state = tracks_[trackIndex];
    if (!state.message || !state.surface) {
        return;
    }
    state.message->setText(text);
    state.message->setStyleSheet(isError ? QStringLiteral("color: #FF8080;")
                                         : QStringLiteral("color: #E0E0E0;"));
    state.message->setGeometry(state.surface->rect());
    state.message->setVisible(!text.isEmpty());
    state.message->raise();
}

void Window::setActiveTrack(int trackIndex)
{
    const int clamped = std::clamp(trackIndex, 0, kTrackCount - 1);
    if (trackCombo_ && trackCombo_->currentIndex() != clamped) {
        const QSignalBlocker blocker(trackCombo_);
        trackCombo_->setCurrentIndex(clamped);
    }
    const bool changed = clamped != activeTrack_;
    activeTrack_ = clamped;
    playbackBar_->setActiveTrack(clamped);
    if (pluginStack_) {
        pluginStack_->setCurrentIndex(clamped);
        // Редактор скрытой страницы мог остаться с размером, который был у неё
        // на момент встраивания: показали — сразу растягиваем на всю рамку
        TrackState& shown = tracks_[clamped];
        if (shown.surface) {
            if (shown.message) {
                shown.message->setGeometry(shown.surface->rect());
            }
            if (shown.host) {
                shown.host->resizeEditor(shown.surface->size());
            }
        }
    }
    syncPluginCombos();
    updateWindowTitle();
    // Плагин дорожки поднимаем лениво: пока на неё не переключились, второй
    // экземпляр не нужен и не тратит время на загрузку
    if (changed && !track().host) {
        reloadPlugin(clamped);
    }
}

void Window::onTrackChanged()
{
    setActiveTrack(trackCombo_->currentData().toInt());
}

void Window::reloadPlugin(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= kTrackCount) {
        return;
    }
    // Внутри загрузки редактор плагина крутит свой Qt-код (мультимедиа,
    // ресурсы), и вложенный цикл событий может снова позвать reloadPlugin —
    // тогда host удалялся бы прямо во время embedEditor (падение по feeefeee)
    if (reloadingPlugin_) {
        return;
    }
    reloadingPlugin_ = true;
    const QScopeGuard reloadDone([this]() { reloadingPlugin_ = false; });

    player_.stop();
    updateTransportUi();

    TrackState& state = tracks_[trackIndex];
#if defined(DONTFLOAT_WITH_ARA)
    // Сначала снимаем дорожку с документа: её клип держит роли экземпляра,
    // который сейчас будет уничтожен
    releaseAraTrack(trackIndex);
#endif
    if (state.host) {
        state.host->unload();
        state.host.reset();
    }

    const PluginFormat format = state.format;
    const PluginProduct product = state.product;
    const QString path = Dontfloat::PluginTester::resolvePluginPath(format, product);

    showPluginMessage(trackIndex,
                      tr("Loading %1 / %2…")
                          .arg(Dontfloat::PluginTester::formatLabel(format), editionLabel(product)),
                      false);

    state.host = createPluginHost(format);
    if (!state.host) {
        showPluginMessage(trackIndex,
                          tr("Format %1 is not supported in this build")
                              .arg(Dontfloat::PluginTester::formatLabel(format)), true);
        return;
    }
    if (!QFileInfo::exists(path)) {
        showPluginMessage(trackIndex,
                          tr("Plugin not found:\n%1").arg(QDir::toNativeSeparators(path)), true);
        state.host.reset();
        return;
    }

    QString error;
    if (!state.host->load(path, product, sampleRate_, kBlockSize, &error)) {
        showPluginMessage(trackIndex, tr("Failed to load the plugin:\n%1").arg(error), true);
        state.host.reset();
        return;
    }

    // Транспорт хоста — до первого прогона аудио
    onBeatGridChanged();

    // Каретку двигают в плагине — переставляем каретку хоста
    state.host->setSeekRequestHandler([this](qint64 frame) {
        QMetaObject::invokeMethod(this, [this, frame]() { onSeekRequested(frame); },
                                  Qt::QueuedConnection);
    });
    // Плагин пересчитал звук — прогоняем дорожку заново, чтобы это стало слышно
    state.host->setRenderChangedHandler([this, trackIndex]() {
        QMetaObject::invokeMethod(this, [this, trackIndex]() {
            showPluginMessage(trackIndex, tr("The plugin re-rendered the audio — re-streaming…"),
                              false);
            runTrackThroughPlugin(trackIndex);
        }, Qt::QueuedConnection);
    });

    QSize editorSize;
    if (state.host->embedEditor(state.surface->winId(), &editorSize, &error)) {
        showPluginMessage(trackIndex, QString(), false);
        state.host->resizeEditor(state.surface->size());
    } else {
        // DSP жив — трек всё равно прогоняем и играем, просто без редактора
        showPluginMessage(trackIndex,
                          tr("The plugin loaded, but its editor did not open:\n%1").arg(error),
                          true);
    }
    updateWindowTitle();

    // Плагин уже живой — отдаём ему трек, если он загружен
    if (state.hasAudio()) {
#if defined(DONTFLOAT_WITH_ARA)
        // Через ARA плагин получает дорожку целиком и сразу: ни проигрывания,
        // ни прогона блоками для этого не нужно
        startAraSessionIfSupported(trackIndex);
#endif
        runTrackThroughPlugin(trackIndex);
    }
}

#if defined(DONTFLOAT_WITH_ARA)
Dontfloat::PluginTester::AraHostDocument* Window::araDocumentFor(const void* factory,
                                                                QString* error)
{
    if (!factory) {
        return nullptr;
    }
    // Дорожки с одной фабрикой садятся на один документ — как в настоящей DAW.
    // Разные форматы (CLAP на первой, VST3 на второй) дают разные фабрики,
    // и документов тогда два: чужие ноты в такой связке и не должны видеться
    for (AraSession& session : araSessions_) {
        if (session.factory == factory) {
            return session.document.get();
        }
    }
    auto document = std::make_unique<Dontfloat::PluginTester::AraHostDocument>();
    if (!document->open(static_cast<const ARA::ARAFactory*>(factory), error)) {
        return nullptr;
    }
    araSessions_.push_back(AraSession { factory, std::move(document) });
    return araSessions_.back().document.get();
}

void Window::startAraSessionIfSupported(int trackIndex)
{
    TrackState& state = tracks_[trackIndex];
    if (!state.host || !state.host->supportsAra() || !state.hasAudio()) {
        return;
    }
    Dontfloat::PluginTester::AraHostTrack araTrack;
    araTrack.left = state.sourceLeft;
    araTrack.right = state.sourceRight;
    araTrack.sampleRate = sampleRate_;
    // Темп и размер такта — те же, что показывает панель мини-DAW
    const float bpm = bpmEdit_ ? bpmEdit_->text().toFloat() : 0.0f;
    araTrack.tempoBpm = bpm > 0.0f ? double(bpm) : 120.0;
    araTrack.beatsPerBar = beatsCombo_ ? beatsCombo_->currentData().toInt() : 4;
    if (araTrack.beatsPerBar <= 0) {
        araTrack.beatsPerBar = 4;
    }
    araTrack.name = QFileInfo(state.audioPath).fileName();

    QString error;
    Dontfloat::PluginTester::AraHostDocument* document =
        araDocumentFor(state.host->araFactory(), &error);
    if (!document) {
        showPluginMessage(trackIndex, tr("ARA: %1").arg(error), true);
        return;
    }
    // Дорожку в документе заводим заново: прошлый клип этой дорожки больше
    // не нужен (файл или плагин сменились)
    if (state.araTrackIndex >= 0 && state.araDocument == document) {
        document->removeTrack(state.araTrackIndex);
    }
    state.araDocument = document;
    state.araTrackIndex = document->addTrack(araTrack);
    if (state.araTrackIndex < 0) {
        showPluginMessage(trackIndex, tr("ARA: the document did not accept the track"), true);
        return;
    }
    if (!state.host->bindAraDocument(*document, state.araTrackIndex, &error)) {
        showPluginMessage(trackIndex, tr("ARA: %1").arg(error), true);
        document->removeTrack(state.araTrackIndex);
        state.araTrackIndex = -1;
        return;
    }
    state.araActive = true;
    // Модель ARA обновляется в фоне (разбор нот) — прокачиваем её по таймеру.
    // Таймер один на окно: он качает все документы
    if (!araPumpTimer_) {
        araPumpTimer_ = new QTimer(this);
        araPumpTimer_->setInterval(100);
        connect(araPumpTimer_, &QTimer::timeout, this, [this]() {
            for (AraSession& session : araSessions_) {
                if (session.document) {
                    session.document->pumpModelUpdates();
                }
            }
        });
    }
    araPumpTimer_->start();
}

void Window::releaseAraTrack(int trackIndex)
{
    TrackState& state = tracks_[trackIndex];
    if (state.araDocument && state.araTrackIndex >= 0) {
        state.araDocument->removeTrack(state.araTrackIndex);
    }
    state.araDocument = nullptr;
    state.araTrackIndex = -1;
    state.araActive = false;
}
#endif

void Window::runTrackThroughPlugin(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= kTrackCount) {
        return;
    }
    TrackState& state = tracks_[trackIndex];
    if (state.timelineLeft.isEmpty()) {
        renderTimeline(trackIndex);
    }
    if (state.timelineLeft.isEmpty()) {
        return;
    }
    // Прогон дорожки через process(): плагин видит её так же, как DAW —
    // с позициями блоков на таймлайне, включая тишину между клипами
    QVector<float> left = state.timelineLeft;
    QVector<float> right = state.timelineRight;
    bool streamThroughPlugin = state.host != nullptr;
#if defined(DONTFLOAT_WITH_ARA)
    // Под ARA прогон не нужен вовсе: плагин уже прочитал дорожку целиком через
    // документ. Именно так работает Melodyne — без проигрывания и «записи»
    if (state.host && state.host->supportsAra() && state.araActive) {
        streamThroughPlugin = false;
    }
#endif
    if (streamThroughPlugin) {
        // В Debug прогон длинного трека занимает секунды — предупреждаем
        showPluginMessage(trackIndex, tr("Streaming the track through the plugin…"), false);
        QApplication::processEvents();
        const int frames = int(std::min(left.size(), right.size()));
        for (int pos = 0; pos < frames; pos += kBlockSize) {
            const int n = std::min(kBlockSize, frames - pos);
            // Позиция блока на дорожке: плагин раскладывает захват по таймлайну
            state.host->process(left.data() + pos, right.data() + pos, n, pos);
        }
    }
    state.renderedLeft = left;
    state.renderedRight = right;

    // На дорожке рисуем то, что вернул плагин — как звучит транспорт
    playbackBar_->setTrack(trackIndex, left, right, sampleRate_);
    playbackBar_->setTrackName(trackIndex, state.audioPath.isEmpty()
                                               ? tr("Track %1").arg(trackIndex + 1)
                                               : QStringLiteral("%1 · %2")
                                                     .arg(trackIndex + 1)
                                                     .arg(QFileInfo(state.audioPath).fileName()));
    updatePlayerMix();
    if (state.host) {
        showPluginMessage(trackIndex, QString(), false);
    }
    if (autoPlay_ && !player_.isPlaying()) {
        onPlayClicked();
    }
}

void Window::updatePlayerMix()
{
    // Транспорт играет сумму дорожек — как мастер-шина DAW
    int frames = 0;
    for (const TrackState& state : tracks_) {
        frames = std::max(frames, int(std::min(state.renderedLeft.size(),
                                               state.renderedRight.size())));
    }
    if (frames <= 0) {
        return;
    }
    QVector<float> mixLeft(frames, 0.0f);
    QVector<float> mixRight(frames, 0.0f);
    int activeTracks = 0;
    for (const TrackState& state : tracks_) {
        const int n = int(std::min(state.renderedLeft.size(), state.renderedRight.size()));
        if (n <= 0) {
            continue;
        }
        ++activeTracks;
        for (int i = 0; i < n; ++i) {
            mixLeft[i] += state.renderedLeft[i];
            mixRight[i] += state.renderedRight[i];
        }
    }
    if (activeTracks > 1) {
        // Простая защита от клиппинга при суммировании дорожек
        const float gain = 1.0f / float(activeTracks);
        for (int i = 0; i < frames; ++i) {
            mixLeft[i] *= gain;
            mixRight[i] *= gain;
        }
    }

    const qint64 keepPosition = std::min<qint64>(playbackBar_->position(), frames);
    player_.setAudio(mixLeft, mixRight, sampleRate_);
    player_.seek(keepPosition);
    playbackBar_->setPosition(keepPosition);
    updateTransportUi();
}

bool Window::openAudio(const QString& path)
{
    return openAudio(activeTrack_, path);
}

bool Window::openAudio(int trackIndex, const QString& path)
{
    if (trackIndex < 0 || trackIndex >= kTrackCount) {
        return false;
    }
    TrackState& state = tracks_[trackIndex];

    // Декодирование синхронное: в Debug длинный файл занимает секунды, поэтому
    // сначала показываем плашку и даём ей отрисоваться
    showPluginMessage(trackIndex, tr("Loading %1…").arg(QFileInfo(path).fileName()), false);
    QApplication::processEvents();

    const auto decoded = AudioFileService::decode(path);
    if (!decoded.ok || decoded.channels.isEmpty()) {
        qWarning("[mini-daw] не удалось декодировать %s: %s",
                 qUtf8Printable(path), qUtf8Printable(decoded.error));
        showPluginMessage(trackIndex, tr("Failed to load audio:\n%1").arg(decoded.error), true);
        return false;
    }
    qInfo("[mini-daw] дорожка %d, трек %s: %d Гц, %lld кадров", trackIndex + 1,
          qUtf8Printable(QFileInfo(path).fileName()), decoded.sampleRate,
          static_cast<long long>(decoded.channels.first().size()));

    const int fileRate = decoded.sampleRate > 0 ? decoded.sampleRate : 44100;
    // Частота сессии — от первой загруженной дорожки; остальные подгоняем под
    // неё, иначе смешивать их в мастер-шину нельзя
    bool sessionRateChanged = false;
    bool otherTrackHasAudio = false;
    for (int i = 0; i < kTrackCount; ++i) {
        if (i != trackIndex && tracks_[i].hasAudio()) {
            otherTrackHasAudio = true;
        }
    }
    if (!otherTrackHasAudio && fileRate != sampleRate_) {
        sampleRate_ = fileRate;
        sessionRateChanged = true;
    }

    state.audioPath = path;
    state.sampleRate = sampleRate_;
    state.sourceLeft = decoded.channels.first();
    state.sourceRight = decoded.channels.size() > 1 ? decoded.channels[1] : decoded.channels.first();
    if (fileRate != sampleRate_) {
        state.sourceLeft = resampleLinear(state.sourceLeft, fileRate, sampleRate_);
        state.sourceRight = resampleLinear(state.sourceRight, fileRate, sampleRate_);
    }
    const int frames = int(std::min(state.sourceLeft.size(), state.sourceRight.size()));
    state.sourceLeft.resize(frames);
    state.sourceRight.resize(frames);

    // Дорожка начинается с одного клипа на весь файл (дальше его режут,
    // двигают, обрезают и растягивают — см. splitClipAtPlayhead и соседей)
    state.clips.clear();
    Clip whole;
    whole.timelineStart = 0;
    whole.sourceStart = 0;
    whole.sourceLength = frames;
    state.clips.append(whole);
    state.selectedClip = 0;
    renderTimeline(trackIndex);

    updateWindowTitle();
    // Плагин активируется на частоте трека — перезагружаем его под новый файл.
    // Если частота сессии сменилась, это касается и соседней дорожки
    for (int i = 0; i < kTrackCount; ++i) {
        const bool needsReload = (i == trackIndex) || (sessionRateChanged && tracks_[i].host);
        if (needsReload) {
            reloadPlugin(i);
        }
    }
    if (!state.host) {
        // Плагин не поднялся — трек всё равно можно послушать
        state.renderedLeft = state.timelineLeft;
        state.renderedRight = state.timelineRight;
        playbackBar_->setTrack(trackIndex, state.timelineLeft, state.timelineRight, sampleRate_);
        playbackBar_->setTrackName(trackIndex, QStringLiteral("%1 · %2")
                                                   .arg(trackIndex + 1)
                                                   .arg(QFileInfo(path).fileName()));
        updatePlayerMix();
    }
    return true;
}

void Window::selectPlugin(PluginFormat format, PluginProduct product)
{
    // Ключи командной строки задают исходный плагин обеим дорожкам
    for (TrackState& state : tracks_) {
        state.format = format;
        state.product = product;
    }
    syncPluginCombos();
    updateWindowTitle();
}

void Window::onOpenClicked()
{
    const QString start = track().audioPath.isEmpty() ? QDir::currentPath() : track().audioPath;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open audio file for track %1").arg(activeTrack_ + 1), start,
        tr("Audio files (*.wav *.mp3 *.flac);;All files (*.*)"));
    if (!path.isEmpty()) {
        openAudio(activeTrack_, path);
    }
}

void Window::onPlayClicked()
{
    if (!player_.hasAudio()) {
        showPluginMessage(tr("Open an audio file first"), false);
        return;
    }
    if (player_.isPlaying()) {
        player_.stop();
    } else {
        player_.play();
        positionTimer_->start();
    }
    updateTransportUi();
}

void Window::onStopClicked()
{
    player_.stop();
    player_.seek(0);
    playbackBar_->setPosition(0);
    for (TrackState& state : tracks_) {
        if (state.host) {
            state.host->setPlayhead(0, false);
        }
    }
    updateTransportUi();
}

void Window::onSelectionChanged()
{
    // Списки формата и редакции относятся к активной дорожке
    track().format = currentFormat();
    track().product = currentProduct();
    reloadPlugin(activeTrack_);
}

void Window::onSeekRequested(qint64 frame)
{
    // Правки идут по клипу под кареткой — как выделение клипа в DAW
    const int index = clipAt(activeTrack_, frame);
    if (index >= 0) {
        track().selectedClip = index;
    }
    player_.seek(frame);
    playbackBar_->setPosition(frame);
    // Каретка общая для всех дорожек — как в DAW
    for (TrackState& state : tracks_) {
        if (state.host) {
            state.host->setPlayhead(frame, player_.isPlaying());
        }
    }
}

int Window::clipAt(int trackIndex, qint64 frame) const
{
    if (trackIndex < 0 || trackIndex >= kTrackCount) {
        return -1;
    }
    return MiniDaw::clipAt(tracks_[trackIndex].clips, frame);
}

void Window::renderTimeline(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= kTrackCount) {
        return;
    }
    TrackState& state = tracks_[trackIndex];
    MiniDaw::renderTimeline(state.clips, state.sourceLeft, state.sourceRight,
                            state.timelineLeft, state.timelineRight);
    playbackBar_->setClipBoundaries(trackIndex, MiniDaw::clipBoundaries(state.clips));
}

void Window::applyClipEdit(const QString& statusText)
{
    renderTimeline(activeTrack_);
    if (!statusText.isEmpty()) {
        showPluginMessage(statusText, false);
        QApplication::processEvents();
    }
    // Прогоняем дорожку заново: плагин увидит новый материал по позициям
    // таймлайна и сам решит — сдвинуть разметку или пересчитать анализ
    runTrackThroughPlugin(activeTrack_);
}

void Window::nudgeClip(int seconds)
{
    onClipMoveRequested(qint64(seconds) * sampleRate_);
}

void Window::onClipMoveRequested(qint64 deltaFrames)
{
    TrackState& state = track();
    if (state.clips.isEmpty()) {
        return;
    }
    state.selectedClip = std::clamp(state.selectedClip, 0, int(state.clips.size()) - 1);
    if (!MiniDaw::moveClip(state.clips, state.selectedClip, deltaFrames)) {
        return;
    }
    applyClipEdit(tr("Clip moved — re-streaming through the plugin..."));
}

void Window::splitClipAtPlayhead()
{
    TrackState& state = track();
    if (state.clips.isEmpty()) {
        return;
    }
    const qint64 position = playbackBar_->position();
    if (MiniDaw::clipAt(state.clips, position) < 0) {
        showPluginMessage(tr("No clip under the cursor"), false);
        return;
    }
    const int tail = MiniDaw::splitClipAt(state.clips, position);
    if (tail < 0) {
        showPluginMessage(tr("The cut is too close to the clip edge"), false);
        return;
    }
    state.selectedClip = tail;
    applyClipEdit(tr("Clip split — re-streaming through the plugin..."));
}

void Window::trimSelectedClip(bool startEdge, qint64 deltaFrames)
{
    TrackState& state = track();
    if (state.clips.isEmpty()) {
        return;
    }
    state.selectedClip = std::clamp(state.selectedClip, 0, int(state.clips.size()) - 1);
    const qint64 sourceFrames =
        qint64(std::min(state.sourceLeft.size(), state.sourceRight.size()));
    if (!MiniDaw::trimClip(state.clips, state.selectedClip, startEdge, deltaFrames,
                           sourceFrames)) {
        return;
    }
    applyClipEdit(startEdge ? tr("Clip start trimmed — re-streaming through the plugin...")
                            : tr("Clip end trimmed — re-streaming through the plugin..."));
}

void Window::stretchSelectedClip(double factor)
{
    TrackState& state = track();
    if (state.clips.isEmpty()) {
        return;
    }
    state.selectedClip = std::clamp(state.selectedClip, 0, int(state.clips.size()) - 1);
    if (!MiniDaw::stretchClip(state.clips, state.selectedClip, factor)) {
        return;
    }
    applyClipEdit(tr("Clip stretched x%1 — re-streaming through the plugin...")
                      .arg(state.clips[state.selectedClip].stretch, 0, 'f', 2));
}

void Window::tickPosition()
{
    const qint64 position = player_.positionFrames();
    playbackBar_->setPosition(position);
    // Каретка плагинов идёт за кареткой транспорта (как в DAW)
    for (TrackState& state : tracks_) {
        if (state.host) {
            state.host->setPlayhead(position, player_.isPlaying());
        }
    }
    if (!player_.isPlaying()) {
        positionTimer_->stop();
        updateTransportUi();
    }
}

void Window::updateTransportUi()
{
    if (player_.isPlaying()) {
        positionTimer_->start();
    } else {
        positionTimer_->stop();
    }
    playButton_->setToolTip(player_.isPlaying() ? tr("Pause") : tr("Play"));
}

void Window::updateWindowTitle()
{
    QString title = QStringLiteral("DONTFLOAT mini-DAW — %1/%2 — %3 / %4")
                        .arg(activeTrack_ + 1)
                        .arg(kTrackCount)
                        .arg(Dontfloat::PluginTester::formatLabel(track().format),
                             editionLabel(track().product));
    if (!track().audioPath.isEmpty()) {
        title += QStringLiteral(" — %1").arg(QFileInfo(track().audioPath).fileName());
    }
    setWindowTitle(title);
}

} // namespace MiniDaw
