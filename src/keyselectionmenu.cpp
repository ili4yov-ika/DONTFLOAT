#include "../include/keyselectionmenu.h"

#include <QtGui/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>

KeySelectionMenu::KeySelectionMenu(QWidget* parent)
    : QObject(parent)
    , m_menu(new QMenu(parent))
{
    static const QStringList majorKeys = {
        "C Major", "C# Major", "D Major", "D# Major", "E Major", "F Major",
        "F# Major", "G Major", "G# Major", "A Major", "A# Major", "B Major"
    };
    static const QStringList minorKeys = {
        "C Minor", "C# Minor", "D Minor", "D# Minor", "E Minor", "F Minor",
        "F# Minor", "G Minor", "G# Minor", "A Minor", "A# Minor", "B Minor"
    };

    auto addKeys = [this](QMenu* submenu, const QStringList& keys) {
        for (const QString& key : keys) {
            QAction* action = submenu->addAction(tr(key.toUtf8().constData()));
            action->setData(key);
            connect(action, &QAction::triggered, this, [this, key]() {
                emit keySelected(key);
            });
        }
    };

    addKeys(m_menu->addMenu(tr("Мажорные")), majorKeys);
    addKeys(m_menu->addMenu(tr("Минорные")), minorKeys);

    m_menu->addSeparator();
    QAction* unknownAction = m_menu->addAction(tr("Не определена"));
    connect(unknownAction, &QAction::triggered, this, [this]() {
        emit keySelected(QString());
    });
}

void KeySelectionMenu::popup(QWidget* anchor, const QPoint& pos)
{
    if (m_menu && anchor)
        m_menu->exec(anchor->mapToGlobal(pos));
}
