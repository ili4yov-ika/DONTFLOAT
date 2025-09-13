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
    settings.setValue("Metronome/Volume", ui->volumeSpinBox->value());
    settings.setValue("Metronome/Sound", ui->soundComboBox->currentData());
}

void MetronomeSettingsDialog::loadSettings()
{
    ui->volumeSpinBox->setValue(settings.value("Metronome/Volume", 50).toInt());
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
    int volume = ui->volumeSpinBox->value();

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
    metronomeSound->setVolume(volume / 100.0f);
    
    // Проверяем, загрузился ли звук
    if (metronomeSound->status() != QSoundEffect::Ready) {
        qWarning() << "Failed to load metronome sound for testing:" << metronomeSound->status();
        return;
    }
    
    metronomeSound->play();
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