#ifndef PIANOROLL_TOOLBAR_H
#define PIANOROLL_TOOLBAR_H

/**
 * Панель кнопок под пианороллом: инструмент «Разделить» и переключатель
 * режима реза («Вдоль сетки» / «Свободный рез»).
 *
 * Разметка повторяет макет MARKDOWN/example_panel_buttons.svg, увеличенный в
 * полтора раза: полоса с капсулой, в ней квадратная кнопка ножниц и пара
 * круглых взаимоисключающих переключателей режима. Иконки — SVG из
 * `resources/icons/` (`trimmer`, `along_the_grid`, `free_cut`).
 */

#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtWidgets/QWidget>

#include "pitchgridwidget.h"

QT_BEGIN_NAMESPACE
class QToolButton;
QT_END_NAMESPACE

class PianoRollToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit PianoRollToolbar(QWidget *parent = nullptr);

    /** Состояние инструмента «Разделить» (без испускания сигнала). */
    void setSplitActive(bool active);
    bool isSplitActive() const;

    /** Режим реза (без испускания сигнала). */
    void setCutMode(PitchGridWidget::CutMode mode);
    PitchGridWidget::CutMode cutMode() const { return currentCutMode; }

    /** Цветовая схема: "dark" | "light" (прочее — тёмная). */
    void setColorScheme(const QString& scheme);

    /** Подсказка кнопки «Разделить» с актуальной горячей клавишей. */
    void setSplitShortcutText(const QString& shortcutText);

    /** Доступность кнопки «Экспорт MIDI» (нет нот — экспортировать нечего). */
    void setExportMidiEnabled(bool enabled);
    /** Ставит состояние замков (например, из настроек), без сигналов. */
    void setMoveLocks(bool horizontalLocked, bool verticalLocked);
    bool isHorizontalMoveLocked() const;
    bool isVerticalMoveLocked() const;

    /**
     * Ставит виджет на полосу слева от «Экспорт MIDI»: так действия редакции
     * плагина живут в той же строке, что и кнопки макета.
     */
    void addTrailingWidget(QWidget* widget);

    // Геометрия: макет (20 / 18 / 1 px) × 1.5; ножницы чуть меньше высоты
    // капсулы, как на макете, где иконка вписана внутрь кнопки
    static constexpr int kCapsuleHeightPx = 30;
    static constexpr int kSplitButtonSizePx = 26;
    /** Скругление подложки ножниц: rx=3 при 18 px в макете. */
    static constexpr int kSplitButtonRadiusPx = 4;
    static constexpr int kCutModeButtonSizePx = 27;
    /** Кнопка «Экспорт MIDI» из макета: невысокий прямоугольник с рамкой. */
    static constexpr int kExportButtonHeightPx = 22;
    static constexpr int kPanelMarginPx = 2;
    static constexpr int kPanelHeightPx = kCapsuleHeightPx + 2 * kPanelMarginPx;

signals:
    void splitToggled(bool active);
    void cutModeChanged(PitchGridWidget::CutMode mode);
    /** Нажата кнопка «Экспорт MIDI» справа на полосе. */
    void exportMidiRequested();
    /** Нажата кнопка «Импорт MIDI» — рядом с экспортом. */
    void importMidiRequested();
    /** Замок горизонтального перемещения нот (по умолчанию закрыт). */
    void horizontalMoveLockChanged(bool locked);
    /** Замок вертикального перемещения нот (по умолчанию открыт). */
    void verticalMoveLockChanged(bool locked);

protected:
    void changeEvent(QEvent *event) override;

private:
    void buildUi();
    void applyStyle();
    void refreshIcons();
    void retranslateUi();

    /**
     * SVG-иконка нужного размера. \a tintOnLightScheme перекрашивает белую
     * графику макета в тёмную для светлой схемы: это нужно ножницам, лежащим
     * прямо на капсуле, но не переключателям — их фон тёмный в любой схеме.
     */
    QIcon loadIcon(const QString& resourcePath, int sizePx, bool tintOnLightScheme) const;

    QWidget* cutGroup = nullptr;
    QWidget* cutModeGroup = nullptr;
    QToolButton* splitButton = nullptr;
    QToolButton* gridCutButton = nullptr;
    QToolButton* freeCutButton = nullptr;
    /** Замки перемещения нот: горизонталь и вертикаль. */
    QToolButton* horizontalLockButton = nullptr;
    QToolButton* verticalLockButton = nullptr;
    QToolButton* exportMidiButton = nullptr;
    QToolButton* importMidiButton = nullptr;

    PitchGridWidget::CutMode currentCutMode = PitchGridWidget::CutMode::SnapToGrid;
    QString colorScheme = QStringLiteral("dark");
    QString splitShortcutText;
};

#endif // PIANOROLL_TOOLBAR_H
