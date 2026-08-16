#include "../include/pianoroll_toolbar.h"

#include <QtCore/QEvent>
#include <QtCore/QSignalBlocker>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>

namespace {

const QString kSplitIconPath = QStringLiteral(":/icons/resources/icons/trimmer.svg");
const QString kGridCutIconPath = QStringLiteral(":/icons/resources/icons/along_the_grid.svg");
const QString kFreeCutIconPath = QStringLiteral(":/icons/resources/icons/free_cut.svg");

} // namespace

PianoRollToolbar::PianoRollToolbar(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    applyStyle();
    refreshIcons();
    retranslateUi();
}

void PianoRollToolbar::buildUi()
{
    setObjectName(QStringLiteral("pianoRollToolbar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kPanelHeightPx);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(kPanelMarginPx, kPanelMarginPx, kPanelMarginPx, kPanelMarginPx);
    root->setSpacing(kPanelMarginPx);

    cutGroup = new QWidget(this);
    cutGroup->setObjectName(QStringLiteral("pianoRollCutGroup"));
    cutGroup->setAttribute(Qt::WA_StyledBackground, true);
    cutGroup->setFixedHeight(kCapsuleHeightPx);

    auto* groupLayout = new QHBoxLayout(cutGroup);
    // Ножницы меньше капсулы — центрируем их в оставшемся поле
    const int splitInset = (kCapsuleHeightPx - kSplitButtonSizePx) / 2;
    groupLayout->setContentsMargins(splitInset, 0, 0, 0);
    groupLayout->setSpacing(splitInset);
    groupLayout->setAlignment(Qt::AlignVCenter);

    splitButton = new QToolButton(cutGroup);
    splitButton->setObjectName(QStringLiteral("pianoRollSplitButton"));
    splitButton->setCheckable(true);
    splitButton->setFixedSize(kSplitButtonSizePx, kSplitButtonSizePx);
    splitButton->setIconSize(QSize(kSplitButtonSizePx, kSplitButtonSizePx));
    splitButton->setCursor(Qt::PointingHandCursor);
    splitButton->setFocusPolicy(Qt::NoFocus);
    groupLayout->addWidget(splitButton);

    cutModeGroup = new QWidget(cutGroup);
    cutModeGroup->setObjectName(QStringLiteral("pianoRollCutModeGroup"));
    cutModeGroup->setAttribute(Qt::WA_StyledBackground, true);
    cutModeGroup->setFixedHeight(kCapsuleHeightPx);

    auto* modeLayout = new QHBoxLayout(cutModeGroup);
    // Зазор капсулы вокруг круглых переключателей: 1 px макета × 1.5
    modeLayout->setContentsMargins(2, 2, 2, 2);
    modeLayout->setSpacing(2);
    modeLayout->setAlignment(Qt::AlignVCenter);

    auto makeModeButton = [this, modeLayout](const QString& objectName) {
        auto* button = new QToolButton(cutModeGroup);
        button->setObjectName(objectName);
        button->setCheckable(true);
        button->setAutoExclusive(true); // всегда активен ровно один режим
        button->setFixedSize(kCutModeButtonSizePx, kCutModeButtonSizePx);
        button->setIconSize(QSize(kCutModeButtonSizePx, kCutModeButtonSizePx));
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        modeLayout->addWidget(button);
        return button;
    };

    gridCutButton = makeModeButton(QStringLiteral("pianoRollGridCutButton"));
    freeCutButton = makeModeButton(QStringLiteral("pianoRollFreeCutButton"));
    gridCutButton->setChecked(true);

    groupLayout->addWidget(cutModeGroup);
    root->addWidget(cutGroup);
    root->addStretch(1);

    connect(splitButton, &QToolButton::toggled, this, &PianoRollToolbar::splitToggled);
    connect(gridCutButton, &QToolButton::toggled, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        currentCutMode = PitchGridWidget::CutMode::SnapToGrid;
        emit cutModeChanged(currentCutMode);
    });
    connect(freeCutButton, &QToolButton::toggled, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        currentCutMode = PitchGridWidget::CutMode::Free;
        emit cutModeChanged(currentCutMode);
    });
}

void PianoRollToolbar::applyStyle()
{
    const bool light = (colorScheme == QStringLiteral("light"));

    // Цвета макета; в светлой схеме светлеют только полоса и капсула —
    // круглые переключатели остаются тёмными «лунками», как в макете
    const QString panelBg   = light ? QStringLiteral("#d4d4d4") : QStringLiteral("#606060");
    const QString groupBg   = light ? QStringLiteral("#bdbdbd") : QStringLiteral("#454545");
    const QString modeBg    = light ? QStringLiteral("#ababab") : QStringLiteral("#3c3c3c");
    const QString hoverBg   = light ? QStringLiteral("rgba(0, 0, 0, 40)")
                                    : QStringLiteral("rgba(255, 255, 255, 40)");
    // Нажатое состояние всегда темнее отпущенного (как в макете)
    const QString splitCheckedBg = QStringLiteral("rgba(0, 0, 0, 128)");
    const QString modeOffBg = QStringLiteral("#585858");
    const QString modeOnBg  = QStringLiteral("#222222");

    // Радиусы: капсула и круглые переключатели — половина размера, подложка
    // ножниц — мягкий квадрат из макета. Радиус повторяется в каждом состоянии:
    // у QToolButton правила с псевдосостояниями не наследуют box-свойства.
    // Радиус строго не больше половины размера: иначе Qt считает его
    // некорректным и рисует прямоугольник без скруглений
    const QString capsuleRadius = QString::number(kCapsuleHeightPx / 2);
    const QString modeRadius = QString::number(kCutModeButtonSizePx / 2);
    const QString modeBox =
        QStringLiteral("border: 0px solid transparent; padding: 0px; border-radius: %1px;")
            .arg(modeRadius);
    const QString splitBox =
        QStringLiteral("border: 0px solid transparent; padding: 0px; border-radius: %1px;")
            .arg(kSplitButtonRadiusPx);

    auto rule = [](const QString& selector, const QString& body) {
        return selector + QStringLiteral(" { ") + body + QStringLiteral(" }");
    };
    auto background = [](const QString& color) {
        return QStringLiteral("background-color: ") + color + QStringLiteral(";");
    };

    const QString modeSelectors =
        QStringLiteral("QToolButton#pianoRollGridCutButton, QToolButton#pianoRollFreeCutButton");
    const QString splitSelector = QStringLiteral("QToolButton#pianoRollSplitButton");

    setStyleSheet(
        rule(QStringLiteral("QWidget#pianoRollToolbar"), background(panelBg))
        + rule(QStringLiteral("QWidget#pianoRollCutGroup"),
               background(groupBg) + QStringLiteral(" border-radius: ") + capsuleRadius
                   + QStringLiteral("px;"))
        + rule(QStringLiteral("QWidget#pianoRollCutModeGroup"),
               background(modeBg) + QStringLiteral(" border-radius: ") + capsuleRadius
                   + QStringLiteral("px;"))
        + rule(splitSelector, splitBox + background(QStringLiteral("transparent")))
        + rule(splitSelector + QStringLiteral(":hover"), splitBox + background(hoverBg))
        + rule(splitSelector + QStringLiteral(":checked"), splitBox + background(splitCheckedBg))
        + rule(modeSelectors, modeBox + background(modeOffBg))
        + rule(QStringLiteral("QToolButton#pianoRollGridCutButton:hover,"
                              " QToolButton#pianoRollFreeCutButton:hover"),
               modeBox + background(hoverBg))
        + rule(QStringLiteral("QToolButton#pianoRollGridCutButton:checked,"
                              " QToolButton#pianoRollFreeCutButton:checked"),
               modeBox + background(modeOnBg)));
}

void PianoRollToolbar::refreshIcons()
{
    splitButton->setIcon(loadIcon(kSplitIconPath, kSplitButtonSizePx, true));
    gridCutButton->setIcon(loadIcon(kGridCutIconPath, kCutModeButtonSizePx, false));
    freeCutButton->setIcon(loadIcon(kFreeCutIconPath, kCutModeButtonSizePx, false));
}

QIcon PianoRollToolbar::loadIcon(const QString& resourcePath, int sizePx,
                                 bool tintOnLightScheme) const
{
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmap = QIcon(resourcePath).pixmap(QSize(sizePx, sizePx), dpr);
    if (pixmap.isNull() || !tintOnLightScheme || colorScheme != QStringLiteral("light")) {
        return QIcon(pixmap);
    }

    // Графика макета белая: на светлой панели перекрашиваем её целиком в тёмную
    QPixmap tinted(pixmap.size());
    tinted.setDevicePixelRatio(pixmap.devicePixelRatio());
    tinted.fill(Qt::transparent);

    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), QColor(40, 40, 40));
    painter.end();
    return QIcon(tinted);
}

void PianoRollToolbar::retranslateUi()
{
    const QString splitHint = splitShortcutText.isEmpty()
        ? tr("Split note at the playback cursor or by clicking on a note")
        : tr("Split note at the playback cursor (%1) or by clicking on a note")
              .arg(splitShortcutText);

    splitButton->setText(tr("Split"));
    splitButton->setToolTip(QStringLiteral("%1 — %2").arg(tr("Split"), splitHint));
    splitButton->setAccessibleName(tr("Split"));

    gridCutButton->setText(tr("Along the grid"));
    gridCutButton->setToolTip(QStringLiteral("%1 — %2")
        .arg(tr("Along the grid"), tr("the cut snaps to the nearest beat grid division")));
    gridCutButton->setAccessibleName(tr("Along the grid"));

    freeCutButton->setText(tr("Free cut"));
    freeCutButton->setToolTip(QStringLiteral("%1 — %2")
        .arg(tr("Free cut"), tr("the cut lands exactly where the cursor is")));
    freeCutButton->setAccessibleName(tr("Free cut"));
}

void PianoRollToolbar::setSplitActive(bool active)
{
    if (splitButton->isChecked() == active) {
        return;
    }
    const QSignalBlocker blocker(splitButton);
    splitButton->setChecked(active);
}

bool PianoRollToolbar::isSplitActive() const
{
    return splitButton->isChecked();
}

void PianoRollToolbar::setCutMode(PitchGridWidget::CutMode mode)
{
    currentCutMode = mode;
    QToolButton* target = (mode == PitchGridWidget::CutMode::Free) ? freeCutButton : gridCutButton;
    if (target->isChecked()) {
        return;
    }
    const QSignalBlocker gridBlocker(gridCutButton);
    const QSignalBlocker freeBlocker(freeCutButton);
    gridCutButton->setChecked(mode == PitchGridWidget::CutMode::SnapToGrid);
    freeCutButton->setChecked(mode == PitchGridWidget::CutMode::Free);
}

void PianoRollToolbar::setColorScheme(const QString& scheme)
{
    if (colorScheme == scheme) {
        return;
    }
    colorScheme = scheme;
    applyStyle();
    refreshIcons();
    update();
}

void PianoRollToolbar::setSplitShortcutText(const QString& shortcutText)
{
    if (splitShortcutText == shortcutText) {
        return;
    }
    splitShortcutText = shortcutText;
    retranslateUi();
}

void PianoRollToolbar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}
