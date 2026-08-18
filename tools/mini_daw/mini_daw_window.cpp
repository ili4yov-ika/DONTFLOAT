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
constexpr int kBpmFieldWidth = 69;
constexpr int kBeatsFieldWidth = 25;
constexpr int kBlockSize = 512;
constexpr float kDefaultBpm = 120.0f;
constexpr int kDefaultBeatsPerBar = 4;

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

QRect PlaybackBar::trackRect() const
{
    return QRect(0, kCaretHeight, width(), kTrackHeight);
}

void PlaybackBar::setTrack(const QVector<float>& left, const QVector<float>& right, int sampleRate)
{
    channels_.clear();
    if (!left.isEmpty()) {
        channels_.append(left);
        if (!right.isEmpty()) {
            channels_.append(right);
        }
    }
    totalFrames_ = channels_.isEmpty() ? 0 : channels_.first().size();
    sampleRate_ = sampleRate > 0 ? sampleRate : 44100;
    position_ = std::min(position_, totalFrames_);
    rebuildEnvelope();
    update();
}

void PlaybackBar::clearTrack()
{
    channels_.clear();
    envelopeUpper_.clear();
    envelopeLower_.clear();
    totalFrames_ = 0;
    position_ = 0;
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

void PlaybackBar::setTrackName(const QString& name)
{
    trackName_ = name;
    update();
}

void PlaybackBar::rebuildEnvelope()
{
    envelopeUpper_.clear();
    envelopeLower_.clear();
    const QRect area = trackRect();
    if (channels_.isEmpty() || area.width() <= 0 || area.height() <= 2) {
        return;
    }
    // Огибающая считается тем же движком, что рисует волну в приложении
    const auto envelope =
        PianoRollEngine::buildWaveformEnvelope(channels_, area.width(), area.height());
    envelopeUpper_ = envelope.upper;
    envelopeLower_ = envelope.lower;
}

void PlaybackBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildEnvelope();
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
    const QRect track = trackRect();
    if (totalFrames_ <= 0 || track.width() <= 0) {
        return 0;
    }
    const double ratio = std::clamp(double(x - track.left()) / double(track.width()), 0.0, 1.0);
    return qint64(ratio * double(totalFrames_));
}

void PlaybackBar::setClipBoundaries(const QVector<qint64>& boundaries)
{
    clipBoundaries_ = boundaries;
    update();
}

void PlaybackBar::drawWaveform(QPainter& painter, const QRect& area) const
{
    if (envelopeUpper_.isEmpty()) {
        return;
    }
    painter.setPen(QPen(QColor(0x3A, 0x3A, 0x3A), 1.0));
    const int columns = std::min(int(envelopeUpper_.size()), area.width());
    for (int x = 0; x < columns; ++x) {
        const int top = area.top() + envelopeUpper_[x];
        const int bottom = area.top() + envelopeLower_[x];
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
    const QRect track = trackRect();

    // Дорожка: белое поле, волна, тактовая сетка, проигранная часть красным
    painter.fillRect(track, Qt::white);
    drawBeatGrid(painter, track);

    // Дорожка уже собрана из клипов: рисуем её целиком, границы клипов —
    // отдельными линиями (после реза их видно, как в DAW)
    const auto xForFrame = [&](qint64 frame) {
        return totalFrames_ > 0 ? double(frame) / double(totalFrames_) * track.width() : 0.0;
    };
    drawWaveform(painter, track);
    painter.setPen(QPen(QColor(0x20, 0x60, 0xC0), 1.0, Qt::DashLine));
    for (qint64 boundary : clipBoundaries_) {
        if (boundary <= 0 || boundary >= totalFrames_) {
            continue;
        }
        const double x = track.left() + xForFrame(boundary);
        painter.drawLine(QPointF(x, track.top()), QPointF(x, track.bottom()));
    }

    const double playedX = totalFrames_ > 0 ? xForFrame(position_) : 0.0;
    if (playedX > 0.0) {
        painter.fillRect(QRectF(track.left(), track.top(), playedX, track.height()),
                         QColor(0xFF, 0x00, 0x00, 128));
    }

    painter.setPen(QPen(QColor(0x60, 0x60, 0x60), 1.0));
    painter.drawRect(track.adjusted(0, 0, -1, -1));

    if (!trackName_.isEmpty()) {
        QFont nameFont = painter.font();
        nameFont.setPointSize(8);
        painter.setFont(nameFont);
        painter.setPen(QColor(0x20, 0x20, 0x20));
        painter.drawText(track.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, trackName_);
    }

    // Каретка: треугольник над дорожкой и линия по ней
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (totalFrames_ > 0) {
        QPolygonF caret;
        caret << QPointF(playedX, track.top())
              << QPointF(playedX - 3.5, track.top() - kCaretHeight + 2)
              << QPointF(playedX + 3.5, track.top() - kCaretHeight + 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xFF, 0x00, 0x00));
        painter.drawPolygon(caret);
        painter.setPen(QPen(QColor(0xFF, 0x00, 0x00), 1.0));
        painter.drawLine(QPointF(playedX, track.top()), QPointF(playedX, track.bottom()));
    }

    // Время — под дорожкой, слева прошло / справа всего
    QFont timeFont = painter.font();
    timeFont.setPointSize(7);
    painter.setFont(timeFont);
    painter.setPen(Qt::white);
    const QRect timeRow(0, track.bottom() + 1, width(), kTimeRowHeight);
    painter.drawText(timeRow, Qt::AlignVCenter | Qt::AlignLeft,
                     formatTime(position_, sampleRate_));
    painter.drawText(timeRow, Qt::AlignVCenter | Qt::AlignRight,
                     formatTime(totalFrames_, sampleRate_));
}

void PlaybackBar::mousePressEvent(QMouseEvent* event)
{
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
        const QRect track = trackRect();
        const int dx = int(event->position().x()) - dragStartX_;
        dragDeltaFrames_ =
            qint64(double(dx) / std::max(1, track.width()) * double(totalFrames_));
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
    resize(1100, 760);

    positionTimer_ = new QTimer(this);
    positionTimer_->setInterval(33); // ~30 fps
    connect(positionTimer_, &QTimer::timeout, this, &Window::tickPosition);

    // Транспорт уже остановлен самим плеером — здесь только обновление UI
    connect(&player_, &Player::finished, this, [this]() {
        playbackBar_->setPosition(player_.totalFrames());
        updateTransportUi();
    });

    QTimer::singleShot(0, this, &Window::reloadPlugin);
}

Window::~Window()
{
    player_.stop();
    if (host_) {
        host_->unload();
    }
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
    // Панель = кнопки + дорожка со временем (высоты из макета)
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

    formatCombo_ = new QComboBox(topBar);
    formatCombo_->setObjectName(QStringLiteral("miniDawFormatCombo"));
    formatCombo_->addItem(QStringLiteral("CLAP"), int(PluginFormat::Clap));
    formatCombo_->addItem(QStringLiteral("VST3"), int(PluginFormat::Vst3));
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

    pluginSurface_ = new QWidget(pluginFrame_);
    pluginSurface_->setObjectName(QStringLiteral("miniDawPluginSurface"));
    pluginSurface_->setAttribute(Qt::WA_StyledBackground, true);
    // Редактор плагина парентится в это нативное окно, как в настоящей DAW
    pluginSurface_->setAttribute(Qt::WA_NativeWindow, true);
    pluginSurface_->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    frameLayout->addWidget(pluginSurface_);

    pluginMessage_ = new QLabel(pluginSurface_);
    pluginMessage_->setObjectName(QStringLiteral("miniDawPluginMessage"));
    pluginMessage_->setAlignment(Qt::AlignCenter);
    pluginMessage_->setWordWrap(true);

    root->addWidget(pluginFrame_, 1);

    formatCombo_->installEventFilter(this);
    productCombo_->installEventFilter(this);
    formatCombo_->setFocusPolicy(Qt::StrongFocus);
    productCombo_->setFocusPolicy(Qt::StrongFocus);

    connect(openButton_, &QToolButton::clicked, this, &Window::onOpenClicked);
    connect(playButton_, &QToolButton::clicked, this, &Window::onPlayClicked);
    connect(stopButton_, &QToolButton::clicked, this, &Window::onStopClicked);
    connect(playbackBar_, &PlaybackBar::seekRequested, this, &Window::onSeekRequested);
    connect(playbackBar_, &PlaybackBar::clipMoveRequested, this, &Window::onClipMoveRequested);

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
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, &Window::onSelectionChanged);
    connect(productCombo_, &QComboBox::currentIndexChanged, this, &Window::onSelectionChanged);
    connect(bpmEdit_, &QLineEdit::editingFinished, this, &Window::onBeatGridChanged);
    connect(beatsCombo_, &QComboBox::currentIndexChanged, this, &Window::onBeatGridChanged);
    beatsCombo_->installEventFilter(this);
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

bool Window::eventFilter(QObject* watched, QEvent* event)
{
    // Прокрутка колесом над списком незаметно меняла формат/редакцию, а с ними
    // перезагружался плагин и сбрасывался транспорт
    if (event->type() == QEvent::Wheel
        && (watched == formatCombo_ || watched == productCombo_ || watched == beatsCombo_)) {
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
    // Темп и размер такта уходят плагину транспортом хоста
    if (host_) {
        host_->setTransport(effectiveBpm, effectiveBeats);
    }
}

void Window::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (pluginMessage_ && pluginSurface_) {
        pluginMessage_->setGeometry(pluginSurface_->rect());
    }
    if (host_ && pluginSurface_) {
        host_->resizeEditor(pluginSurface_->size());
    }
}

void Window::showPluginMessage(const QString& text, bool isError)
{
    if (!pluginMessage_) {
        return;
    }
    pluginMessage_->setText(text);
    pluginMessage_->setStyleSheet(isError ? QStringLiteral("color: #FF8080;")
                                          : QStringLiteral("color: #E0E0E0;"));
    pluginMessage_->setGeometry(pluginSurface_->rect());
    pluginMessage_->setVisible(!text.isEmpty());
    pluginMessage_->raise();
}

void Window::reloadPlugin()
{
    // Внутри загрузки редактор плагина крутит свой Qt-код (мультимедиа,
    // ресурсы), и вложенный цикл событий может снова позвать reloadPlugin —
    // тогда host_ удалялся бы прямо во время embedEditor (падение по feeefeee)
    if (reloadingPlugin_) {
        return;
    }
    reloadingPlugin_ = true;
    const QScopeGuard reloadDone([this]() { reloadingPlugin_ = false; });

    player_.stop();
    updateTransportUi();

    if (host_) {
        host_->unload();
        host_.reset();
    }

    const PluginFormat format = currentFormat();
    const PluginProduct product = currentProduct();
    const QString path = Dontfloat::PluginTester::resolvePluginPath(format, product);

    showPluginMessage(tr("Loading %1 / %2…")
                          .arg(Dontfloat::PluginTester::formatLabel(format), editionLabel(product)),
                      false);

    host_ = createPluginHost(format);
    if (!host_) {
        showPluginMessage(tr("Format %1 is not supported in this build")
                              .arg(Dontfloat::PluginTester::formatLabel(format)), true);
        return;
    }
    if (!QFileInfo::exists(path)) {
        showPluginMessage(tr("Plugin not found:\n%1").arg(QDir::toNativeSeparators(path)), true);
        host_.reset();
        return;
    }

    QString error;
    if (!host_->load(path, product, sampleRate_, kBlockSize, &error)) {
        showPluginMessage(tr("Failed to load the plugin:\n%1").arg(error), true);
        host_.reset();
        return;
    }

    // Транспорт хоста — до первого прогона аудио
    onBeatGridChanged();

    // Каретку двигают в плагине — переставляем каретку хоста
    host_->setSeekRequestHandler([this](qint64 frame) {
        QMetaObject::invokeMethod(this, [this, frame]() { onSeekRequested(frame); },
                                  Qt::QueuedConnection);
    });

    QSize editorSize;
    if (host_->embedEditor(pluginSurface_->winId(), &editorSize, &error)) {
        showPluginMessage(QString(), false);
        host_->resizeEditor(pluginSurface_->size());
    } else {
        // DSP жив — трек всё равно прогоняем и играем, просто без редактора
        showPluginMessage(tr("The plugin loaded, but its editor did not open:\n%1").arg(error), true);
    }
    updateWindowTitle();

    // Плагин уже живой — отдаём ему трек, если он загружен
    if (!sourceLeft_.isEmpty()) {
        runTrackThroughPlugin();
    }
}

void Window::runTrackThroughPlugin()
{
    if (timelineLeft_.isEmpty()) {
        renderTimeline();
    }
    if (timelineLeft_.isEmpty()) {
        return;
    }
    // Прогон дорожки через process(): плагин видит её так же, как DAW —
    // с позициями блоков на таймлайне, включая тишину между клипами
    QVector<float> left = timelineLeft_;
    QVector<float> right = timelineRight_;
    if (host_) {
        // В Debug прогон длинного трека занимает секунды — предупреждаем
        showPluginMessage(tr("Streaming the track through the plugin…"), false);
        QApplication::processEvents();
        const int frames = int(std::min(left.size(), right.size()));
        for (int pos = 0; pos < frames; pos += kBlockSize) {
            const int n = std::min(kBlockSize, frames - pos);
            // Позиция блока на дорожке: плагин раскладывает захват по таймлайну
            host_->process(left.data() + pos, right.data() + pos, n, pos);
        }
    }
    player_.setAudio(left, right, sampleRate_);
    // На дорожке рисуем то, что вернул плагин — как звучит транспорт
    playbackBar_->setTrack(left, right, sampleRate_);
    playbackBar_->setTrackName(QFileInfo(audioPath_).fileName());
    playbackBar_->setPosition(0);
    updateTransportUi();
    if (host_) {
        showPluginMessage(QString(), false);
    }
    if (autoPlay_ && !player_.isPlaying()) {
        onPlayClicked();
    }
}

bool Window::openAudio(const QString& path)
{
    // Декодирование синхронное: в Debug длинный файл занимает секунды, поэтому
    // сначала показываем плашку и даём ей отрисоваться
    showPluginMessage(tr("Loading %1…").arg(QFileInfo(path).fileName()), false);
    QApplication::processEvents();

    const auto decoded = AudioFileService::decode(path);
    if (!decoded.ok || decoded.channels.isEmpty()) {
        qWarning("[mini-daw] не удалось декодировать %s: %s",
                 qUtf8Printable(path), qUtf8Printable(decoded.error));
        showPluginMessage(tr("Failed to load audio:\n%1").arg(decoded.error), true);
        return false;
    }
    qInfo("[mini-daw] трек %s: %d Гц, %lld кадров",
          qUtf8Printable(QFileInfo(path).fileName()), decoded.sampleRate,
          static_cast<long long>(decoded.channels.first().size()));

    audioPath_ = path;
    sampleRate_ = decoded.sampleRate > 0 ? decoded.sampleRate : 44100;
    sourceLeft_ = decoded.channels.first();
    sourceRight_ = decoded.channels.size() > 1 ? decoded.channels[1] : decoded.channels.first();
    const int frames = int(std::min(sourceLeft_.size(), sourceRight_.size()));
    sourceLeft_.resize(frames);
    sourceRight_.resize(frames);

    // Дорожка начинается с одного клипа на весь файл (дальше его режут,
    // двигают, обрезают и растягивают — см. splitClipAtPlayhead и соседей)
    clips_.clear();
    Clip whole;
    whole.timelineStart = 0;
    whole.sourceStart = 0;
    whole.sourceLength = frames;
    clips_.append(whole);
    selectedClip_ = 0;
    renderTimeline();

    updateWindowTitle();
    // Плагин активируется на частоте трека — перезагружаем его под новый файл
    reloadPlugin();
    if (!host_) {
        // Плагин не поднялся — трек всё равно можно послушать
        player_.setAudio(timelineLeft_, timelineRight_, sampleRate_);
        playbackBar_->setTrack(timelineLeft_, timelineRight_, sampleRate_);
        playbackBar_->setTrackName(QFileInfo(audioPath_).fileName());
        playbackBar_->setPosition(0);
        updateTransportUi();
    }
    return true;
}

void Window::selectPlugin(PluginFormat format, PluginProduct product)
{
    const int formatIndex = formatCombo_->findData(int(format));
    const int productIndex = productCombo_->findData(int(product));
    // Меняем без сигналов: перезагрузку плагина делаем один раз в конце
    const QSignalBlocker formatBlocker(formatCombo_);
    const QSignalBlocker productBlocker(productCombo_);
    if (formatIndex >= 0) {
        formatCombo_->setCurrentIndex(formatIndex);
    }
    if (productIndex >= 0) {
        productCombo_->setCurrentIndex(productIndex);
    }
    updateWindowTitle();
}

void Window::onOpenClicked()
{
    const QString start = audioPath_.isEmpty() ? QDir::currentPath() : audioPath_;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open audio file"), start,
        tr("Audio files (*.wav *.mp3 *.flac);;All files (*.*)"));
    if (!path.isEmpty()) {
        openAudio(path);
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
    if (host_) {
        host_->setPlayhead(0, false);
    }
    updateTransportUi();
}

void Window::onSelectionChanged()
{
    reloadPlugin();
}

void Window::onSeekRequested(qint64 frame)
{
    // Правки идут по клипу под кареткой — как выделение клипа в DAW
    const int index = clipAt(frame);
    if (index >= 0) {
        selectedClip_ = index;
    }
    player_.seek(frame);
    playbackBar_->setPosition(frame);
    if (host_) {
        host_->setPlayhead(frame, player_.isPlaying());
    }
}

int Window::clipAt(qint64 frame) const
{
    for (int i = 0; i < clips_.size(); ++i) {
        if (frame >= clips_[i].timelineStart && frame < clips_[i].timelineEnd()) {
            return i;
        }
    }
    return -1;
}

void Window::renderTimeline()
{
    timelineLeft_.clear();
    timelineRight_.clear();
    if (clips_.isEmpty() || sourceLeft_.isEmpty()) {
        return;
    }

    qint64 total = 0;
    for (const Clip& clip : clips_) {
        total = std::max(total, clip.timelineEnd());
    }
    if (total <= 0) {
        return;
    }
    timelineLeft_.assign(int(total), 0.0f);
    timelineRight_.assign(int(total), 0.0f);

    const int sourceFrames = int(std::min(sourceLeft_.size(), sourceRight_.size()));
    for (const Clip& clip : clips_) {
        const qint64 length = clip.timelineLength();
        for (qint64 i = 0; i < length; ++i) {
            const qint64 outIndex = clip.timelineStart + i;
            if (outIndex < 0 || outIndex >= total) {
                continue;
            }
            // Растяжение — линейная интерполяция по исходному материалу
            const double sourcePos = double(clip.sourceStart) + double(i) / clip.stretch;
            const int base = int(sourcePos);
            if (base < 0 || base >= sourceFrames) {
                continue;
            }
            const int next = std::min(base + 1, sourceFrames - 1);
            const float t = float(sourcePos - double(base));
            timelineLeft_[int(outIndex)] = sourceLeft_[base] * (1.0f - t) + sourceLeft_[next] * t;
            timelineRight_[int(outIndex)] = sourceRight_[base] * (1.0f - t) + sourceRight_[next] * t;
        }
    }

    QVector<qint64> boundaries;
    for (const Clip& clip : clips_) {
        boundaries.append(clip.timelineStart);
        boundaries.append(clip.timelineEnd());
    }
    playbackBar_->setClipBoundaries(boundaries);
}

void Window::applyClipEdit(const QString& statusText)
{
    renderTimeline();
    if (!statusText.isEmpty()) {
        showPluginMessage(statusText, false);
        QApplication::processEvents();
    }
    // Прогоняем дорожку заново: плагин увидит новый материал по позициям
    // таймлайна и сам решит — сдвинуть разметку или пересчитать анализ
    runTrackThroughPlugin();
}

void Window::nudgeClip(int seconds)
{
    onClipMoveRequested(qint64(seconds) * sampleRate_);
}

void Window::onClipMoveRequested(qint64 deltaFrames)
{
    if (clips_.isEmpty() || deltaFrames == 0) {
        return;
    }
    selectedClip_ = std::clamp(selectedClip_, 0, int(clips_.size()) - 1);
    Clip& clip = clips_[selectedClip_];
    clip.timelineStart = std::max<qint64>(0, clip.timelineStart + deltaFrames);
    applyClipEdit(tr("Clip moved — re-streaming through the plugin..."));
}

void Window::splitClipAtPlayhead()
{
    if (clips_.isEmpty()) {
        return;
    }
    const qint64 position = playbackBar_->position();
    const int index = clipAt(position);
    if (index < 0) {
        showPluginMessage(tr("No clip under the cursor"), false);
        return;
    }

    constexpr qint64 kMinClipFrames = 256;
    Clip& clip = clips_[index];
    const qint64 offsetInClip = position - clip.timelineStart;
    if (offsetInClip < kMinClipFrames || clip.timelineLength() - offsetInClip < kMinClipFrames) {
        showPluginMessage(tr("The cut is too close to the clip edge"), false);
        return;
    }

    // Правая половина начинается там, где закончилась левая — и по дорожке,
    // и по исходному материалу (с учётом растяжения)
    const qint64 sourceOffset = qint64(double(offsetInClip) / clip.stretch);
    Clip tail = clip;
    tail.timelineStart = position;
    tail.sourceStart = clip.sourceStart + sourceOffset;
    tail.sourceLength = clip.sourceLength - sourceOffset;
    clip.sourceLength = sourceOffset;
    clips_.insert(index + 1, tail);
    selectedClip_ = index + 1;

    applyClipEdit(tr("Clip split — re-streaming through the plugin..."));
}

void Window::trimSelectedClip(bool startEdge, qint64 deltaFrames)
{
    if (clips_.isEmpty() || deltaFrames == 0) {
        return;
    }
    selectedClip_ = std::clamp(selectedClip_, 0, int(clips_.size()) - 1);
    Clip& clip = clips_[selectedClip_];
    constexpr qint64 kMinClipFrames = 256;
    const qint64 sourceFrames = qint64(std::min(sourceLeft_.size(), sourceRight_.size()));

    if (startEdge) {
        // Левый край: двигаем и позицию на дорожке, и точку в материале
        const qint64 sourceDelta = qint64(double(deltaFrames) / clip.stretch);
        const qint64 newSourceStart =
            std::clamp<qint64>(clip.sourceStart + sourceDelta, 0, sourceFrames - kMinClipFrames);
        const qint64 applied = newSourceStart - clip.sourceStart;
        if (applied == 0 || clip.sourceLength - applied < kMinClipFrames) {
            return;
        }
        clip.sourceStart = newSourceStart;
        clip.sourceLength -= applied;
        clip.timelineStart =
            std::max<qint64>(0, clip.timelineStart + qint64(double(applied) * clip.stretch));
    } else {
        // Правый край: меняем только длину куска материала
        const qint64 sourceDelta = qint64(double(deltaFrames) / clip.stretch);
        const qint64 maxLength = sourceFrames - clip.sourceStart;
        const qint64 newLength =
            std::clamp<qint64>(clip.sourceLength + sourceDelta, kMinClipFrames, maxLength);
        if (newLength == clip.sourceLength) {
            return;
        }
        clip.sourceLength = newLength;
    }

    applyClipEdit(startEdge ? tr("Clip start trimmed — re-streaming through the plugin...")
                            : tr("Clip end trimmed — re-streaming through the plugin..."));
}

void Window::stretchSelectedClip(double factor)
{
    if (clips_.isEmpty() || factor <= 0.0) {
        return;
    }
    selectedClip_ = std::clamp(selectedClip_, 0, int(clips_.size()) - 1);
    Clip& clip = clips_[selectedClip_];
    const double stretch = std::clamp(clip.stretch * factor, 0.25, 4.0);
    if (std::fabs(stretch - clip.stretch) < 1e-6) {
        return;
    }
    clip.stretch = stretch;
    applyClipEdit(tr("Clip stretched x%1 — re-streaming through the plugin...")
                      .arg(stretch, 0, 'f', 2));
}

void Window::tickPosition()
{
    const qint64 position = player_.positionFrames();
    playbackBar_->setPosition(position);
    // Каретка плагина идёт за кареткой транспорта (как в DAW)
    if (host_) {
        host_->setPlayhead(position, player_.isPlaying());
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
    QString title = QStringLiteral("DONTFLOAT mini-DAW — %1 / %2")
                        .arg(Dontfloat::PluginTester::formatLabel(currentFormat()),
                             editionLabel(currentProduct()));
    if (!audioPath_.isEmpty()) {
        title += QStringLiteral(" — %1").arg(QFileInfo(audioPath_).fileName());
    }
    setWindowTitle(title);
}

} // namespace MiniDaw
