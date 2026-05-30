#ifndef KEYSELECTIONMENU_H
#define KEYSELECTIONMENU_H

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QString>

class QWidget;
class QMenu;

// Контекстное меню выбора тональности: подменю «Мажорные»/«Минорные» (по 12 тональностей)
// и пункт «Не определена». Инкапсулирует построение меню; о выборе сообщает сигналом
// keySelected(). Используется для обоих полей тональности (основное и модуляция).
class KeySelectionMenu : public QObject
{
    Q_OBJECT
public:
    explicit KeySelectionMenu(QWidget* parent = nullptr);

    // Показать меню рядом с anchor в позиции pos (в координатах anchor).
    void popup(QWidget* anchor, const QPoint& pos);

signals:
    void keySelected(const QString& key); // пустая строка = тональность не определена

private:
    QMenu* m_menu = nullptr;
};

#endif // KEYSELECTIONMENU_H
