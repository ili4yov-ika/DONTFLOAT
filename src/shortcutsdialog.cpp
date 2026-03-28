#include "../include/shortcutsdialog.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QDialogButtonBox>

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
    , table(nullptr)
    , resetButton(nullptr)
    , m_settings("DONTFLOAT", "DONTFLOAT")
{
    setWindowTitle(tr("Горячие клавиши"));
    setMinimumSize(520, 420);
    resize(560, 480);

    m_entries = {
        { "Open",           tr("Открыть файл"),           QKeySequence::Open },
        { "Save",           tr("Сохранить файл"),         QKeySequence::Save },
        { "Exit",           tr("Выход"),                 QKeySequence::Quit },
        { "Play",           tr("Воспроизведение/Пауза"), QKeySequence(Qt::Key_Space) },
        { "Stop",           tr("Стоп"),                  QKeySequence(Qt::Key_S) },
        { "Metronome",      tr("Метроном"),              QKeySequence(Qt::Key_T) },
        { "LoopStart",      tr("Установить начало цикла (A)"), QKeySequence(Qt::Key_A) },
        { "LoopEnd",        tr("Установить конец цикла (B)"),  QKeySequence(Qt::Key_B) },
        { "ClearLoopA",     tr("Удалить точку A"),       QKeySequence(Qt::SHIFT | Qt::Key_A) },
        { "ClearLoopB",     tr("Удалить точку B"),        QKeySequence(Qt::SHIFT | Qt::Key_B) },
        { "Undo",           tr("Отменить"),              QKeySequence::Undo },
        { "Redo",           tr("Повторить"),             QKeySequence::Redo },
        { "PitchGrid",      tr("Переключить питч-сетку"), QKeySequence(Qt::CTRL | Qt::Key_G) },
        { "AddMarker",      tr("Добавить метку"),        QKeySequence(Qt::Key_M) },
        { "TimeStretch",    tr("Применить сжатие-растяжение"), QKeySequence(Qt::CTRL | Qt::Key_T) },
    };

    auto *layout = new QVBoxLayout(this);

    table = new QTableWidget(this);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({ tr("Действие"), tr("Сочетание клавиш") });
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QTableWidget::SelectRows);
    table->setEditTriggers(QTableWidget::NoEditTriggers);
    table->setAlternatingRowColors(true);
    connect(table, &QTableWidget::cellDoubleClicked, this, &ShortcutsDialog::onCellDoubleClicked);
    layout->addWidget(table);

    auto *buttonLayout = new QHBoxLayout();
    resetButton = new QPushButton(tr("По умолчанию"), this);
    resetButton->setToolTip(tr("Восстановить все сочетания по умолчанию"));
    connect(resetButton, &QPushButton::clicked, this, &ShortcutsDialog::onResetDefaults);
    buttonLayout->addWidget(resetButton);
    buttonLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToSettings();
        emit shortcutsChanged();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonLayout->addWidget(buttonBox);
    layout->addLayout(buttonLayout);

    buildTable();
    loadFromSettings();
}

ShortcutsDialog::~ShortcutsDialog() = default;

void ShortcutsDialog::buildTable()
{
    table->setRowCount(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry& e = m_entries[row];
        table->setItem(row, 0, new QTableWidgetItem(e.description));
        auto *edit = new QKeySequenceEdit(this);
        edit->setClearButtonEnabled(true);
        edit->setProperty("shortcutId", e.id);
        table->setCellWidget(row, 1, edit);
    }
}

QKeySequence ShortcutsDialog::defaultShortcut(const QString& id) const
{
    for (const Entry& e : m_entries) {
        if (e.id == id)
            return e.defaultKey;
    }
    return QKeySequence();
}

QString ShortcutsDialog::shortcutId(int row) const
{
    if (row >= 0 && row < m_entries.size())
        return m_entries[row].id;
    return QString();
}

void ShortcutsDialog::setShortcutAtRow(int row, const QKeySequence& seq)
{
    auto *edit = qobject_cast<QKeySequenceEdit*>(table->cellWidget(row, 1));
    if (edit)
        edit->setKeySequence(seq);
}

void ShortcutsDialog::loadFromSettings()
{
    m_settings.beginGroup("Shortcuts");
    for (int row = 0; row < m_entries.size(); ++row) {
        const QString id = m_entries[row].id;
        QString stored = m_settings.value(id, QString()).toString();
        QKeySequence seq = stored.isEmpty()
                           ? m_entries[row].defaultKey
                           : QKeySequence::fromString(stored);
        setShortcutAtRow(row, seq);
    }
    m_settings.endGroup();
}

void ShortcutsDialog::saveToSettings()
{
    m_settings.beginGroup("Shortcuts");
    for (int row = 0; row < m_entries.size(); ++row) {
        auto *edit = qobject_cast<QKeySequenceEdit*>(table->cellWidget(row, 1));
        if (edit) {
            QKeySequence seq = edit->keySequence();
            m_settings.setValue(m_entries[row].id, seq.toString());
        }
    }
    m_settings.endGroup();
}

QMap<QString, QKeySequence> ShortcutsDialog::currentShortcuts() const
{
    QMap<QString, QKeySequence> out;
    for (int row = 0; row < m_entries.size(); ++row) {
        auto *edit = qobject_cast<QKeySequenceEdit*>(table->cellWidget(row, 1));
        if (edit)
            out.insert(m_entries[row].id, edit->keySequence());
    }
    return out;
}

void ShortcutsDialog::onResetDefaults()
{
    for (int row = 0; row < m_entries.size(); ++row)
        setShortcutAtRow(row, m_entries[row].defaultKey);
}

void ShortcutsDialog::onCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    auto *edit = qobject_cast<QKeySequenceEdit*>(table->cellWidget(row, 1));
    if (edit) {
        edit->setFocus();
        edit->clear();
    }
}
