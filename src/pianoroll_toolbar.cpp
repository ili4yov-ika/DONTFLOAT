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

    // Импорт референсного MIDI — рядом с экспортом, тем же прямоугольником
    importMidiButton = new QToolButton(this);
    importMidiButton->setObjectName(QStringLiteral("pianoRollImportMidiButton"));
    importMidiButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    importMidiButton->setFixedHeight(kExportButtonHeightPx);
    importMidiButton->setCursor(Qt::PointingHandCursor);
    importMidiButton->setFocusPolicy(Qt::NoFocus);
    root->addWidget(importMidiButton, 0, Qt::AlignVCenter);
    connect(importMidiButton, &QToolButton::clicked,
            this, &PianoRollToolbar::importMidiRequested);

    // Экспорт MIDI — справа на полосе: ноты пианоролла уходят в файл .mid.
    // В макете это компактный прямоугольник ниже капсулы реза
    exportMidiButton = new QToolButton(this);
    exportMidiButton->setObjectName(QStringLiteral("pianoRollExportMidiButton"));
    exportMidiButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    exportMidiButton->setFixedHeight(kExportButtonHeightPx);
    exportMidiButton->setCursor(Qt::PointingHandCursor);
    exportMidiButton->setFocusPolicy(Qt::NoFocus);
    root->addWidget(exportMidiButton, 0, Qt::AlignVCenter);

    connect(exportMidiButton, &QToolButton::clicked,
            this, &PianoRollToolbar::exportMidiRequested);
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
    // Кнопка экспорта из макета: тёмная заливка и светлая рамка в тон панели
    const QString exportBg     = light ? QStringLiteral("#c4c4c4") : QStringLiteral("#3c3c3c");
    const QString exportBorder = light ? QStringLiteral("#8a8a8a") : QStringLiteral("#9a9a9a");
    const QString exportText   = light ? QStringLiteral("#202020") : QStringLiteral("#e8e8e8");

    // Радиусы: капсула и круглые переключатели — половина размера, подложка
    // ножниц — мягкий квадрат из макета. Радиус повторяется в каждом состоянии:
    // у QToolButton правила с псевдосостояниями не наследуют box-свойства.
    // Радиус строго не больше половины размера: иначе Qt считает его
    // некорректным и рисует прямоугольник без скруглений
    const QString capsuleRadius = QString::number(kCapsuleHeightPx / 2);
    const QString modeRadius = QString::number(kCutModeButtonSizePx / 2);
    // Слева капсула повторяет форму кнопки ножниц (в макете прямоугольник
    // кнопки закрашивает круглый конец капсулы): радиус кнопки + её отступ,
    // чтобы рамка вокруг кнопки была одинаковой ширины по углам
    const QString capsuleLeftRadius = QString::number(
        kSplitButtonRadiusPx + (kCapsuleHeightPx - kSplitButtonSizePx) / 2);
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
               background(groupBg)
                   + QStringLiteral(" border-top-left-radius: ") + capsuleLeftRadius
                   + QStringLiteral("px; border-bottom-left-radius: ") + capsuleLeftRadius
                   + QStringLiteral("px; border-top-right-radius: ") + capsuleRadius
                   + QStringLiteral("px; border-bottom-right-radius: ") + capsuleRadius
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
               modeBox + background(modeOnBg))
        // Кнопка экспорта — прямоугольник с тонкой рамкой (как в макете
        // MARKDOWN/example_plugin_dontfloat.svg), а не капсула
        + rule(QStringLiteral("QToolButton#pianoRollImportMidiButton"),
               background(exportBg)
                   + QStringLiteral(" color: ") + exportText
                   + QStringLiteral("; border: 1px solid ") + exportBorder
                   + QStringLiteral("; border-radius: 2px; padding: 1px 10px;"))
        + rule(QStringLiteral("QToolButton#pianoRollImportMidiButton:hover"),
               background(hoverBg) + QStringLiteral(" border: 1px solid ") + exportBorder
                   + QStringLiteral("; border-radius: 2px;"))
        + rule(QStringLiteral("QToolButton#pianoRollExportMidiButton"),
               background(exportBg)
                   + QStringLiteral(" color: ") + exportText
                   + QStringLiteral("; border: 1px solid ") + exportBorder
                   + QStringLiteral("; border-radius: 2px; padding: 1px 10px;"))
        + rule(QStringLiteral("QToolButton#pianoRollExportMidiButton:hover"),
               background(hoverBg) + QStringLiteral(" border: 1px solid ") + exportBorder
                   + QStringLiteral("; border-radius: 2px;"))
        + rule(QStringLiteral("QToolButton#pianoRollExportMidiButton:disabled"),
               QStringLiteral("color: rgba(255, 255, 255, 90); border: 1px solid ")
                   + exportBorder + QStringLiteral("; border-radius: 2px;")));
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

    importMidiButton->setText(tr("Import MIDI"));
    importMidiButton->setToolTip(QStringLiteral("%1 — %2")
        .arg(tr("Import MIDI"), tr("show notes from a .mid file as a grey reference")));
    importMidiButton->setAccessibleName(tr("Import MIDI"));

    exportMidiButton->setText(tr("Export MIDI"));
    exportMidiButton->setToolTip(QStringLiteral("%1 — %2")
        .arg(tr("Export MIDI"), tr("save the piano roll notes as a .mid file")));
    exportMidiButton->setAccessibleName(tr("Export MIDI"));
}

void PianoRollToolbar::setExportMidiEnabled(bool enabled)
{
    exportMidiButton->setEnabled(enabled);
}

void PianoRollToolbar::addTrailingWidget(QWidget* widget)
{
    auto* root = qobject_cast<QHBoxLayout*>(layout());
    if (!root || !widget) {
        return;
    }
    // Перед кнопкой экспорта: она остаётся крайней справа, как в макете
    const int exportIndex = root->indexOf(exportMidiButton);
    root->insertWidget(exportIndex >= 0 ? exportIndex : root->count(), widget, 0,
                       Qt::AlignVCenter);
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
