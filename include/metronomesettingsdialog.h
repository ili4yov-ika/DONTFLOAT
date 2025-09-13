#ifndef METRONOMESETTINGSDIALOG_H
#define METRONOMESETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>
#include <QSoundEffect>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QComboBox;
class QPushButton;
QT_END_NAMESPACE

namespace Ui {
class MetronomeSettingsDialog;
}

class MetronomeSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetronomeSettingsDialog(QWidget *parent = nullptr);
    ~MetronomeSettingsDialog();

private slots:
    void saveSettings();
    void loadSettings();
    void onTestButtonClicked();
    void onSelectSoundButtonClicked();

private:
    Ui::MetronomeSettingsDialog *ui;
    QSettings settings;
    QSoundEffect *metronomeSound; // Объект для тестового воспроизведения
};

#endif // METRONOMESETTINGSDIALOG_H