#ifndef METRONOMESETTINGSDIALOG_H
#define METRONOMESETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>
#include <QSoundEffect>
#include <QMediaPlayer>
#include <QAudioOutput>

class MetronomeController; // Forward declaration

QT_BEGIN_NAMESPACE
class QSpinBox;
class QComboBox;
class QPushButton;
class QSlider;
QT_END_NAMESPACE

namespace Ui {
class MetronomeSettingsDialog;
}

class MetronomeSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetronomeSettingsDialog(QWidget *parent = nullptr, MetronomeController *controller = nullptr);
    ~MetronomeSettingsDialog();

private slots:
    void saveSettings();
    void loadSettings();
    void onTestButtonClicked();
    void onTestWeakButtonClicked();
    void onSelectSoundButtonClicked();
    void onStrongBeatVolumeChanged(int value);
    void onWeakBeatVolumeChanged(int value);

private:
    void createMetronomeSound();
    void playTestSound(int volume, bool isStrong);
    void updateSoundStatus();

    Ui::MetronomeSettingsDialog *ui;
    QSettings settings;
    MetronomeController *metronomeController; // Ссылка на контроллер метронома
    QSoundEffect *metronomeSoundEffect; // Объект для тестового воспроизведения
    QMediaPlayer *metronomePlayer; // Альтернативный способ воспроизведения
    QString metronomeSoundFile; // Путь к временному файлу звука
};

#endif // METRONOMESETTINGSDIALOG_H