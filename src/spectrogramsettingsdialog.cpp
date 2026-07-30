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
    setWindowTitle(tr("Spectrogram settings"));
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
    windowSizeCombo->setToolTip(tr("FFT window. Larger = better frequency resolution, lower time resolution."));
    form->addRow(tr("FFT window:"), windowSizeCombo);

    // --- Оконная функция ---
    windowFuncCombo = new QComboBox(this);
    windowFuncCombo->addItem(tr("Rectangular"),   0);
    windowFuncCombo->addItem(tr("Blackman-Harris"), 1);
    windowFuncCombo->addItem(tr("Hamming"),         2);
    windowFuncCombo->addItem(tr("Hanning"),         3);
    windowFuncCombo->setCurrentIndex(1); // Blackman-Harris
    windowFuncCombo->setToolTip(tr("Window function; Blackman-Harris is a good default."));
    form->addRow(tr("Window function:"), windowFuncCombo);

    // --- Временное разрешение ---
    maxFramesSlider = new QSlider(Qt::Horizontal, this);
    maxFramesSlider->setRange(64, 1024);
    maxFramesSlider->setSingleStep(64);
    maxFramesSlider->setPageStep(128);
    maxFramesSlider->setValue(512);
    maxFramesSlider->setToolTip(tr("Time frames; more = finer time resolution, slower render."));
    maxFramesLabel = new QLabel(tr("512 frames"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(maxFramesSlider);
        row->addWidget(maxFramesLabel);
        form->addRow(tr("Time resolution:"), row);
    }

    // --- Частотные полосы ---
    freqBinsSlider = new QSlider(Qt::Horizontal, this);
    freqBinsSlider->setRange(32, 512);
    freqBinsSlider->setSingleStep(16);
    freqBinsSlider->setPageStep(32);
    freqBinsSlider->setValue(256);
    freqBinsSlider->setToolTip(tr("Number of frequency bands."));
    freqBinsLabel = new QLabel(tr("256 bands"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(freqBinsSlider);
        row->addWidget(freqBinsLabel);
        form->addRow(tr("Frequency bands:"), row);
    }

    // --- Цветовая схема ---
    colorSchemeCombo = new QComboBox(this);
    colorSchemeCombo->addItem(tr("Heat map"), 0);
    colorSchemeCombo->addItem(tr("Grayscale"), 1);
    colorSchemeCombo->addItem(tr("Cool (cyan)"), 2);
    form->addRow(tr("Color scheme:"), colorSchemeCombo);

    mainLayout->addLayout(form);

    // --- Группа дополнительных параметров ---
    auto* advGroup = new QGroupBox(tr("Scales"), this);
    auto* advLayout = new QFormLayout(advGroup);

    logFreqCheck = new QCheckBox(tr("Log frequency scale"), this);
    logFreqCheck->setChecked(true);
    logFreqCheck->setToolTip(tr("Log scale (good for music). Linear = equal spacing."));
    advLayout->addRow(logFreqCheck);

    dbAmplCheck = new QCheckBox(tr("Amplitude (dB)"), this);
    dbAmplCheck->setChecked(true);
    dbAmplCheck->setToolTip(tr("dB amplitude scale; better for quiet detail."));
    advLayout->addRow(dbAmplCheck);

    floorDbSpin = new QDoubleSpinBox(this);
    floorDbSpin->setRange(-120.0, -20.0);
    floorDbSpin->setSingleStep(5.0);
    floorDbSpin->setValue(-90.0);
    floorDbSpin->setSuffix(tr(" dB"));
    floorDbSpin->setToolTip(tr("dB floor; more negative = more quiet detail."));
    advLayout->addRow(tr("Floor (dB):"), floorDbSpin);

    mainLayout->addWidget(advGroup);

    // --- Подсказка ---
    auto* note = new QLabel(
        tr("<small><i>Applies when Spectrogram mode is on.</i></small>"),
        this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    mainLayout->addStretch();

    closeBtn = new QPushButton(tr("Close"), this);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    setFixedWidth(400);

    // Подключения
    connect(windowSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramSettingsDialog::onAnyChange);
    connect(windowFuncCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramSettingsDialog::onAnyChange);
    connect(maxFramesSlider, &QSlider::valueChanged, this, [this](int v) {
        maxFramesLabel->setText(tr("%1 frames").arg(v));
        onAnyChange();
    });
    connect(freqBinsSlider, &QSlider::valueChanged, this, [this](int v) {
        freqBinsLabel->setText(tr("%1 bands").arg(v));
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
    maxFramesLabel->setText(tr("%1 frames").arg(s.maxFrames));

    freqBinsSlider->setValue(s.freqBins);
    freqBinsLabel->setText(tr("%1 bands").arg(s.freqBins));

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
