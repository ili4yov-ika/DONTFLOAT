#ifndef METRONOMESETTINGSDIALOG_H
#define METRONOMESETTINGSDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QSoundEffect>

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
    QSpinBox *volumeSpinBox;
    QComboBox *soundComboBox;
    QPushButton *testButton;
    QPushButton *selectSoundButton; // Новая кнопка
    QSettings settings;
    QSoundEffect *metronomeSound; // Объект для тестового воспроизведения
};

#endif // METRONOMESETTINGSDIALOG_H