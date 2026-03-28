#ifndef SHORTCUTSDIALOG_H
#define SHORTCUTSDIALOG_H

#include <QDialog>
#include <QKeySequence>
#include <QSettings>
#include <QMap>

QT_BEGIN_NAMESPACE
class QTableWidget;
class QKeySequenceEdit;
class QPushButton;
QT_END_NAMESPACE

/**
 * Диалог настройки горячих клавиш.
 * Позволяет просматривать и изменять сочетания клавиш, сохраняет их в QSettings.
 */
class ShortcutsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutsDialog(QWidget *parent = nullptr);
    ~ShortcutsDialog();

    /** Загрузить текущие значения из QSettings и заполнить таблицу */
    void loadFromSettings();
    /** Сохранить текущие значения из таблицы в QSettings */
    void saveToSettings();
    /** Вернуть карту id -> QKeySequence для применения в главном окне */
    QMap<QString, QKeySequence> currentShortcuts() const;

signals:
    /** Сигнал после принятия диалога — главное окно должно применить новые сочетания */
    void shortcutsChanged();

private slots:
    void onResetDefaults();
    void onCellDoubleClicked(int row, int column);

private:
    void buildTable();
    QKeySequence defaultShortcut(const QString& id) const;
    QString shortcutId(int row) const;
    void setShortcutAtRow(int row, const QKeySequence& seq);

    QTableWidget *table;
    QPushButton *resetButton;
    QSettings m_settings;
    struct Entry { QString id; QString description; QKeySequence defaultKey; };
    QVector<Entry> m_entries;
};

#endif // SHORTCUTSDIALOG_H
