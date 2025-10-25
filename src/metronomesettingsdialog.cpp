#include "../include/metronomesettingsdialog.h"
#include "ui_metronomesettingsdialog.h"
#include <QFileDialog>
#include <QUrl>

MetronomeSettingsDialog::MetronomeSettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::MetronomeSettingsDialog), settings("DONTFLOAT", "DONTFLOAT"), metronomeSound(new QSoundEffect(this))
{
    ui->setupUi(this);
    
    // Подключаем сигналы кнопок
    connect(ui->testButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onTestButtonClicked);
    connect(ui->selectSoundButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onSelectSoundButtonClicked);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &MetronomeSettingsDialog::saveSettings);
    
    // Подключаем сигналы ползунков
    connect(ui->strongBeatSlider, &QSlider::valueChanged, this, &MetronomeSettingsDialog::onStrongBeatVolumeChanged);
    connect(ui->weakBeatSlider, &QSlider::valueChanged, this, &MetronomeSettingsDialog::onWeakBeatVolumeChanged);

    // Загружаем сохраненные настройки
    loadSettings();
}

MetronomeSettingsDialog::~MetronomeSettingsDialog()
{
    delete ui;
    delete metronomeSound;
}

void MetronomeSettingsDialog::saveSettings()
{
    settings.setValue("Metronome/StrongBeatVolume", ui->strongBeatSlider->value());
    settings.setValue("Metronome/WeakBeatVolume", ui->weakBeatSlider->value());
    settings.setValue("Metronome/Sound", ui->soundComboBox->currentData());
}

void MetronomeSettingsDialog::loadSettings()
{
    ui->strongBeatSlider->setValue(settings.value("Metronome/StrongBeatVolume", 100).toInt());
    ui->weakBeatSlider->setValue(settings.value("Metronome/WeakBeatVolume", 90).toInt());
    QString sound = settings.value("Metronome/Sound", "click").toString();
    int index = ui->soundComboBox->findData(sound);
    if (index != -1)
    {
        ui->soundComboBox->setCurrentIndex(index);
    }
}

void MetronomeSettingsDialog::onTestButtonClicked()
{
    QString selectedSound = ui->soundComboBox->currentData().toString();
    int strongVolume = ui->strongBeatSlider->value();

    QString soundPath;

    if (selectedSound == "click")
    {
        // Используем системный звук или создаем простой звук
        metronomeSound->setSource(QUrl("qrc:/sounds/metronome.wav"));
    }
    else
    {
        // Пользовательский путь
        metronomeSound->setSource(QUrl::fromLocalFile(selectedSound));
    }
    metronomeSound->setVolume(strongVolume / 100.0f);
    
    // Проверяем, загрузился ли звук
    if (metronomeSound->status() != QSoundEffect::Ready) {
        qWarning() << "Failed to load metronome sound for testing:" << metronomeSound->status();
        return;
    }
    
    metronomeSound->play();
}

void MetronomeSettingsDialog::onStrongBeatVolumeChanged(int value)
{
    ui->strongBeatValueLabel->setText(QString::number(value) + "%");
}

void MetronomeSettingsDialog::onWeakBeatVolumeChanged(int value)
{
    ui->weakBeatValueLabel->setText(QString::number(value) + "%");
}

void MetronomeSettingsDialog::onSelectSoundButtonClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Выберите звук метронома"), "", tr("Аудиофайлы (*.wav *.mp3)"));
    if (!fileName.isEmpty())
    {
        ui->soundComboBox->addItem(tr("Пользовательский"), fileName);
        ui->soundComboBox->setCurrentIndex(ui->soundComboBox->count() - 1);
    }
}