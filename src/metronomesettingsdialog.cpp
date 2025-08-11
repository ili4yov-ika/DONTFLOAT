#include "metronomesettingsdialog.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QUrl>

MetronomeSettingsDialog::MetronomeSettingsDialog(QWidget *parent)
    : QDialog(parent), settings("DONTFLOAT", "DONTFLOAT"), metronomeSound(new QSoundEffect(this))
{
    setWindowTitle(tr("Настройки метронома"));
    setMinimumWidth(300);

    auto mainLayout = new QVBoxLayout(this);

    // Громкость
    auto volumeLayout = new QHBoxLayout;
    auto volumeLabel = new QLabel(tr("Громкость:"), this);
    volumeSpinBox = new QSpinBox(this);
    volumeSpinBox->setRange(0, 100);
    volumeSpinBox->setSuffix("%");
    volumeLayout->addWidget(volumeLabel);
    volumeLayout->addWidget(volumeSpinBox);
    mainLayout->addLayout(volumeLayout);

    // Выбор звука
    auto soundLayout = new QHBoxLayout;
    auto soundLabel = new QLabel(tr("Звук:"), this);
    soundComboBox = new QComboBox(this);
    soundComboBox->addItem(tr("Щелчок (стандартный)"), "click");
    soundLayout->addWidget(soundLabel);
    soundLayout->addWidget(soundComboBox);
    mainLayout->addLayout(soundLayout);

    // Кнопка выбора пользовательского звука
    selectSoundButton = new QPushButton(tr("Выбрать звук"), this);
    connect(selectSoundButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onSelectSoundButtonClicked);
    mainLayout->addWidget(selectSoundButton);

    // Кнопка тестирования
    testButton = new QPushButton(tr("Тест"), this);
    connect(testButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onTestButtonClicked);
    mainLayout->addWidget(testButton);

    // Стандартные кнопки
    auto buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &MetronomeSettingsDialog::saveSettings);
    mainLayout->addWidget(buttonBox);

    // Загружаем сохраненные настройки
    loadSettings();
}

MetronomeSettingsDialog::~MetronomeSettingsDialog()
{
    delete metronomeSound;
}

void MetronomeSettingsDialog::saveSettings()
{
    settings.setValue("Metronome/Volume", volumeSpinBox->value());
    settings.setValue("Metronome/Sound", soundComboBox->currentData());
}

void MetronomeSettingsDialog::loadSettings()
{
    volumeSpinBox->setValue(settings.value("Metronome/Volume", 50).toInt());
    QString sound = settings.value("Metronome/Sound", "click").toString();
    int index = soundComboBox->findData(sound);
    if (index != -1)
    {
        soundComboBox->setCurrentIndex(index);
    }
}

void MetronomeSettingsDialog::onTestButtonClicked()
{
    QString selectedSound = soundComboBox->currentData().toString();
    int volume = volumeSpinBox->value();

    QString soundPath;

    if (selectedSound == "click")
    {
        // Используем системный звук или создаем простой звук
        soundPath = "qrc:/sounds/metronome.wav";
    }
    else
    {
        soundPath = selectedSound; // Пользовательский путь
    }

    metronomeSound->setSource(QUrl::fromLocalFile(soundPath));
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
        soundComboBox->addItem(tr("Пользовательский"), fileName);
        soundComboBox->setCurrentIndex(soundComboBox->count() - 1);
    }
}