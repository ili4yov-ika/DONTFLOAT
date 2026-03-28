#include "../include/spectrogramsettingsdialog.h"
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

SpectrogramSettingsDialog::SpectrogramSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Настройки спектрограммы"));
    setModal(false);
    buildUi();
}

void SpectrogramSettingsDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    // --- FFT окно ---
    windowSizeCombo = new QComboBox(this);
    windowSizeCombo->addItem("256",  256);
    windowSizeCombo->addItem("512",  512);
    windowSizeCombo->addItem("1024", 1024);
    windowSizeCombo->addItem("2048", 2048);
    windowSizeCombo->setCurrentIndex(2); // 1024 по умолчанию
    windowSizeCombo->setToolTip(tr("Размер FFT-окна. Больше — выше разрешение по частоте, ниже по времени."));
    form->addRow(tr("Размер FFT-окна:"), windowSizeCombo);

    // --- Оконная функция ---
    windowFuncCombo = new QComboBox(this);
    windowFuncCombo->addItem(tr("Прямоугольная"),   0);
    windowFuncCombo->addItem(tr("Blackman-Harris"), 1);
    windowFuncCombo->addItem(tr("Hamming"),         2);
    windowFuncCombo->addItem(tr("Hanning"),         3);
    windowFuncCombo->setCurrentIndex(1); // Blackman-Harris
    windowFuncCombo->setToolTip(tr("Оконная функция влияет на подавление боковых лепестков.\nBlackman-Harris — лучший выбор для спектрограмм."));
    form->addRow(tr("Оконная функция:"), windowFuncCombo);

    // --- Временное разрешение ---
    maxFramesSlider = new QSlider(Qt::Horizontal, this);
    maxFramesSlider->setRange(64, 1024);
    maxFramesSlider->setSingleStep(64);
    maxFramesSlider->setPageStep(128);
    maxFramesSlider->setValue(512);
    maxFramesSlider->setToolTip(tr("Количество временных кадров. Больше — детальнее по времени, медленнее генерация."));
    maxFramesLabel = new QLabel(tr("512 кадров"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(maxFramesSlider);
        row->addWidget(maxFramesLabel);
        form->addRow(tr("Временное разрешение:"), row);
    }

    // --- Частотные полосы ---
    freqBinsSlider = new QSlider(Qt::Horizontal, this);
    freqBinsSlider->setRange(32, 512);
    freqBinsSlider->setSingleStep(16);
    freqBinsSlider->setPageStep(32);
    freqBinsSlider->setValue(256);
    freqBinsSlider->setToolTip(tr("Число отображаемых частотных полос."));
    freqBinsLabel = new QLabel(tr("256 полос"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(freqBinsSlider);
        row->addWidget(freqBinsLabel);
        form->addRow(tr("Частотные полосы:"), row);
    }

    // --- Цветовая схема ---
    colorSchemeCombo = new QComboBox(this);
    colorSchemeCombo->addItem(tr("Тепловая карта"), 0);
    colorSchemeCombo->addItem(tr("Оттенки серого"), 1);
    colorSchemeCombo->addItem(tr("Холодная (cyan)"), 2);
    form->addRow(tr("Цветовая схема:"), colorSchemeCombo);

    mainLayout->addLayout(form);

    // --- Группа дополнительных параметров ---
    auto* advGroup = new QGroupBox(tr("Шкалы"), this);
    auto* advLayout = new QFormLayout(advGroup);

    logFreqCheck = new QCheckBox(tr("Логарифмическая шкала частот"), this);
    logFreqCheck->setChecked(true);
    logFreqCheck->setToolTip(tr("Логарифмическая шкала частот (рекомендуется для музыки).\nЛинейная показывает равные частотные интервалы."));
    advLayout->addRow(logFreqCheck);

    dbAmplCheck = new QCheckBox(tr("Амплитуда в дБ"), this);
    dbAmplCheck->setChecked(true);
    dbAmplCheck->setToolTip(tr("Отображение амплитуды в логарифмическом (дБ) масштабе.\nЛучше показывает слабые детали."));
    advLayout->addRow(dbAmplCheck);

    floorDbSpin = new QDoubleSpinBox(this);
    floorDbSpin->setRange(-120.0, -20.0);
    floorDbSpin->setSingleStep(5.0);
    floorDbSpin->setValue(-90.0);
    floorDbSpin->setSuffix(tr(" дБ"));
    floorDbSpin->setToolTip(tr("Нижняя граница шкалы дБ. Более отрицательное значение — больше деталей в тихих частях."));
    advLayout->addRow(tr("Нижний порог дБ:"), floorDbSpin);

    mainLayout->addWidget(advGroup);

    // --- Подсказка ---
    auto* note = new QLabel(
        tr("<small><i>Изменения применяются сразу, если включён режим «Спектрограмма».</i></small>"),
        this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    mainLayout->addStretch();

    closeBtn = new QPushButton(tr("Закрыть"), this);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    setFixedWidth(400);

    // Подключения
    connect(windowSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramSettingsDialog::onAnyChange);
    connect(windowFuncCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramSettingsDialog::onAnyChange);
    connect(maxFramesSlider, &QSlider::valueChanged, this, [this](int v) {
        maxFramesLabel->setText(tr("%1 кадров").arg(v));
        onAnyChange();
    });
    connect(freqBinsSlider, &QSlider::valueChanged, this, [this](int v) {
        freqBinsLabel->setText(tr("%1 полос").arg(v));
        onAnyChange();
    });
    connect(colorSchemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramSettingsDialog::onAnyChange);
    connect(logFreqCheck, &QCheckBox::toggled, this, &SpectrogramSettingsDialog::onAnyChange);
    connect(dbAmplCheck,  &QCheckBox::toggled, this, [this](bool checked) {
        floorDbSpin->setEnabled(checked);
        onAnyChange();
    });
    connect(floorDbSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onAnyChange(); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void SpectrogramSettingsDialog::onAnyChange()
{
    emit settingsChanged(getSettings());
}

void SpectrogramSettingsDialog::setSettings(const WaveformView::SpectrogramSettings& s)
{
    QSignalBlocker b1(windowSizeCombo);
    QSignalBlocker b2(windowFuncCombo);
    QSignalBlocker b3(maxFramesSlider);
    QSignalBlocker b4(freqBinsSlider);
    QSignalBlocker b5(colorSchemeCombo);
    QSignalBlocker b6(logFreqCheck);
    QSignalBlocker b7(dbAmplCheck);
    QSignalBlocker b8(floorDbSpin);

    int wIdx = windowSizeCombo->findData(s.windowSize);
    if (wIdx >= 0) windowSizeCombo->setCurrentIndex(wIdx);

    windowFuncCombo->setCurrentIndex(static_cast<int>(s.windowFunction));

    maxFramesSlider->setValue(s.maxFrames);
    maxFramesLabel->setText(tr("%1 кадров").arg(s.maxFrames));

    freqBinsSlider->setValue(s.freqBins);
    freqBinsLabel->setText(tr("%1 полос").arg(s.freqBins));

    colorSchemeCombo->setCurrentIndex(static_cast<int>(s.colorScheme));
    logFreqCheck->setChecked(s.logFreqScale);
    dbAmplCheck->setChecked(s.dbAmplitude);
    floorDbSpin->setValue(double(s.floorDb));
    floorDbSpin->setEnabled(s.dbAmplitude);
}

WaveformView::SpectrogramSettings SpectrogramSettingsDialog::getSettings() const
{
    WaveformView::SpectrogramSettings s;
    s.windowSize     = windowSizeCombo->currentData().toInt();
    s.windowFunction = static_cast<WaveformView::SpectrogramWindowFunction>(windowFuncCombo->currentIndex());
    s.maxFrames      = maxFramesSlider->value();
    s.freqBins       = freqBinsSlider->value();
    s.colorScheme    = static_cast<WaveformView::SpectrogramColorScheme>(colorSchemeCombo->currentIndex());
    s.logFreqScale   = logFreqCheck->isChecked();
    s.dbAmplitude    = dbAmplCheck->isChecked();
    s.floorDb        = float(floorDbSpin->value());
    s.zeroPadFactor  = 2;
    return s;
}
