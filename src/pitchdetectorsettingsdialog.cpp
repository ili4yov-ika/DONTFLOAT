#include "../include/pitchdetectorsettingsdialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

#include <cmath>

namespace {

/// Индекс стандарта, совпадающего с частотой (в пределах 0.05 Гц), иначе -1.
int standardIndexFor(float hz)
{
    int count = 0;
    const PitchDetector::TuningStandard* std_ = PitchDetector::tuningStandards(count);
    for (int i = 0; i < count; ++i) {
        if (std::abs(std_[i].hz - hz) < 0.05f) {
            return i;
        }
    }
    return -1;
}

} // namespace

PitchDetectorSettingsDialog::PitchDetectorSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Pitch detector settings"));
    setModal(false);
    buildUi();
    setOptions(PitchDetector::Options());
}

void PitchDetectorSettingsDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ─── Строй ───────────────────────────────────────────────────────────────
    auto* tuningBox = new QGroupBox(tr("Tuning"), this);
    auto* tuningForm = new QFormLayout(tuningBox);

    standardCombo = new QComboBox(this);
    int count = 0;
    const PitchDetector::TuningStandard* standards = PitchDetector::tuningStandards(count);
    for (int i = 0; i < count; ++i) {
        standardCombo->addItem(QString::fromLatin1(standards[i].name), standards[i].hz);
    }
    standardCombo->addItem(tr("Custom..."), -1.0f);
    standardCombo->setToolTip(tr("Common tuning standards; pick Custom to enter any value."));
    tuningForm->addRow(tr("Standard:"), standardCombo);

    referenceSpin = new QDoubleSpinBox(this);
    referenceSpin->setRange(380.0, 500.0);
    referenceSpin->setDecimals(2);
    referenceSpin->setSingleStep(0.5);
    referenceSpin->setSuffix(tr(" Hz"));
    referenceSpin->setToolTip(tr("Frequency of A4. Shifts the whole pitch scale."));
    tuningForm->addRow(tr("A4 reference:"), referenceSpin);

    referenceHintLabel = new QLabel(this);
    referenceHintLabel->setWordWrap(true);
    tuningForm->addRow(referenceHintLabel);

    mainLayout->addWidget(tuningBox);

    // ─── Диапазон поиска ─────────────────────────────────────────────────────
    auto* rangeBox = new QGroupBox(tr("Search range"), this);
    auto* rangeForm = new QFormLayout(rangeBox);

    minFreqSpin = new QDoubleSpinBox(this);
    minFreqSpin->setRange(8.0, 2000.0);
    minFreqSpin->setDecimals(1);
    minFreqSpin->setSingleStep(1.0);
    minFreqSpin->setSuffix(tr(" Hz"));
    minFreqSpin->setToolTip(
        tr("Lowest f0 to look for. Lowering it lengthens the analysis window "
           "and smears short notes."));
    rangeForm->addRow(tr("Lowest pitch:"), minFreqSpin);

    maxFreqSpin = new QDoubleSpinBox(this);
    maxFreqSpin->setRange(100.0, 8000.0);
    maxFreqSpin->setDecimals(0);
    maxFreqSpin->setSingleStep(50.0);
    maxFreqSpin->setSuffix(tr(" Hz"));
    maxFreqSpin->setToolTip(tr("Highest f0 to look for."));
    rangeForm->addRow(tr("Highest pitch:"), maxFreqSpin);

    rangeHintLabel = new QLabel(this);
    rangeHintLabel->setWordWrap(true);
    rangeForm->addRow(rangeHintLabel);

    mainLayout->addWidget(rangeBox);

    // ─── Сегментация ─────────────────────────────────────────────────────────
    auto* segBox = new QGroupBox(tr("Note segmentation"), this);
    auto* segForm = new QFormLayout(segBox);

    minDurationSpin = new QSpinBox(this);
    minDurationSpin->setRange(10, 1000);
    minDurationSpin->setSingleStep(10);
    minDurationSpin->setSuffix(tr(" ms"));
    minDurationSpin->setToolTip(tr("Shorter runs are discarded as noise."));
    segForm->addRow(tr("Minimum note:"), minDurationSpin);

    minRmsSpin = new QDoubleSpinBox(this);
    minRmsSpin->setRange(0.0, 0.5);
    minRmsSpin->setDecimals(3);
    minRmsSpin->setSingleStep(0.005);
    minRmsSpin->setToolTip(tr("Frames quieter than this are treated as silence."));
    segForm->addRow(tr("Silence threshold:"), minRmsSpin);

    minConfidenceSpin = new QDoubleSpinBox(this);
    minConfidenceSpin->setRange(0.0, 0.95);
    minConfidenceSpin->setDecimals(2);
    minConfidenceSpin->setSingleStep(0.05);
    minConfidenceSpin->setToolTip(
        tr("How periodic a frame must be to count as pitched. Raise it on noisy "
           "material, lower it if quiet notes go missing."));
    segForm->addRow(tr("Minimum confidence:"), minConfidenceSpin);

    mainLayout->addWidget(segBox);

    auto* note = new QLabel(
        tr("Settings apply to the next analysis; already detected notes are kept."), this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    auto* buttons = new QHBoxLayout;
    defaultsBtn = new QPushButton(tr("Restore defaults"), this);
    closeBtn = new QPushButton(tr("Close"), this);
    buttons->addWidget(defaultsBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);
    mainLayout->addLayout(buttons);

    connect(standardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PitchDetectorSettingsDialog::onStandardPicked);
    connect(referenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PitchDetectorSettingsDialog::onAnyChange);
    connect(minFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PitchDetectorSettingsDialog::onAnyChange);
    connect(maxFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PitchDetectorSettingsDialog::onAnyChange);
    connect(minDurationSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PitchDetectorSettingsDialog::onAnyChange);
    connect(minRmsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PitchDetectorSettingsDialog::onAnyChange);
    connect(minConfidenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PitchDetectorSettingsDialog::onAnyChange);
    connect(defaultsBtn, &QPushButton::clicked,
            this, &PitchDetectorSettingsDialog::onRestoreDefaults);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void PitchDetectorSettingsDialog::onStandardPicked(int index)
{
    if (updating || index < 0) {
        return;
    }
    const float hz = standardCombo->itemData(index).toFloat();
    if (hz > 0.0f) {
        updating = true;
        referenceSpin->setValue(double(hz));
        updating = false;
    }
    onAnyChange();
}

void PitchDetectorSettingsDialog::onRestoreDefaults()
{
    setOptions(PitchDetector::Options());
    emit optionsChanged(getOptions());
}

void PitchDetectorSettingsDialog::refreshDerivedLabels()
{
    const float ref = float(referenceSpin->value());
    const float cents = 1200.0f * std::log2(ref / 440.0f);
    referenceHintLabel->setText(
        std::abs(cents) < 0.05f
            ? tr("Standard concert pitch.")
            : tr("%1 cents relative to 440 Hz.").arg(cents, 0, 'f', 1));

    // Окно анализа — два периода нижней границы (см. PitchDetector::Options).
    const double minHz = minFreqSpin->value();
    const double windowMs = minHz > 0.0 ? 2000.0 / minHz : 0.0;
    rangeHintLabel->setText(
        tr("Analysis window ≈ %1 ms; notes shorter than that get smeared.")
            .arg(windowMs, 0, 'f', 0));
}

void PitchDetectorSettingsDialog::onAnyChange()
{
    if (updating) {
        return;
    }
    // Границы не должны схлопываться: верх всегда выше низа.
    if (maxFreqSpin->value() <= minFreqSpin->value() * 2.0) {
        updating = true;
        maxFreqSpin->setValue(minFreqSpin->value() * 2.0);
        updating = false;
    }

    updating = true;
    const int idx = standardIndexFor(float(referenceSpin->value()));
    standardCombo->setCurrentIndex(idx >= 0 ? idx : standardCombo->count() - 1);
    updating = false;

    refreshDerivedLabels();
    emit optionsChanged(getOptions());
}

void PitchDetectorSettingsDialog::setOptions(const PitchDetector::Options& o)
{
    updating = true;
    referenceSpin->setValue(double(o.referenceHz));
    minFreqSpin->setValue(double(o.minFrequencyHz));
    maxFreqSpin->setValue(double(o.maxFrequencyHz));
    minDurationSpin->setValue(o.minNoteDurationMs);
    minRmsSpin->setValue(double(o.minRms));
    minConfidenceSpin->setValue(double(o.minCorrelation));

    const int idx = standardIndexFor(o.referenceHz);
    standardCombo->setCurrentIndex(idx >= 0 ? idx : standardCombo->count() - 1);
    updating = false;

    refreshDerivedLabels();
}

PitchDetector::Options PitchDetectorSettingsDialog::getOptions() const
{
    PitchDetector::Options o;
    o.referenceHz = float(referenceSpin->value());
    o.minFrequencyHz = float(minFreqSpin->value());
    o.maxFrequencyHz = float(maxFreqSpin->value());
    o.minNoteDurationMs = minDurationSpin->value();
    o.minRms = float(minRmsSpin->value());
    o.minCorrelation = float(minConfidenceSpin->value());
    return o;
}
