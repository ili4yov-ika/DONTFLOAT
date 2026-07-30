#include "../include/pitchshiftsettingsdialog.h"
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

PitchShiftSettingsDialog::PitchShiftSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Pitch shift settings"));
    setModal(false);
    buildUi();
}

void PitchShiftSettingsDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    enableCheck = new QCheckBox(tr("Apply pitch shift after time stretch"), this);
    // По умолчанию питч-шифтер активен после растяжения
    enableCheck->setChecked(true);
    mainLayout->addWidget(enableCheck);

    auto* group = new QGroupBox(tr("Parameters"), this);
    auto* form  = new QFormLayout(group);

    // Питч (полутоны)
    pitchSlider = new QSlider(Qt::Horizontal, this);
    pitchSlider->setRange(-24, 24);
    pitchSlider->setSingleStep(1);
    pitchSlider->setValue(0);
    pitchSlider->setToolTip(tr("Pitch in semitones (−24..+24)."));
    pitchLabel = new QLabel(tr("0 st"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(pitchSlider);
        row->addWidget(pitchLabel);
        form->addRow(tr("Pitch (st):"), row);
    }

    // Частота гран
    grainHzSlider = new QSlider(Qt::Horizontal, this);
    grainHzSlider->setRange(20, 400); // 2.0..40.0 Гц × 10
    grainHzSlider->setSingleStep(5);
    grainHzSlider->setValue(80); // 8.0 Гц по умолчанию
    grainHzSlider->setToolTip(tr("Grain rate (Hz). Lower = larger grains; higher = smaller, cleaner."));
    grainHzLabel = new QLabel(tr("8.0 Hz"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(grainHzSlider);
        row->addWidget(grainHzLabel);
        form->addRow(tr("Grain rate (Hz):"), row);
    }

    // Форма огибающей
    shapeSlider = new QSlider(Qt::Horizontal, this);
    shapeSlider->setRange(0, 100);
    shapeSlider->setValue(50);
    shapeSlider->setToolTip(tr("Grain envelope. 0 = equal-gain; 100 = equal-power."));
    shapeLabel = new QLabel(tr("50%"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(shapeSlider);
        row->addWidget(shapeLabel);
        form->addRow(tr("Envelope:"), row);
    }

    // Джиттер питча
    jitterSlider = new QSlider(Qt::Horizontal, this);
    jitterSlider->setRange(0, 100);
    jitterSlider->setValue(0);
    jitterSlider->setToolTip(tr("Pitch spread (0..1 octave). Chorus-like."));
    jitterLabel = new QLabel(tr("0%"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(jitterSlider);
        row->addWidget(jitterLabel);
        form->addRow(tr("Pitch spread:"), row);
    }

    // Wet mix
    wetSlider = new QSlider(Qt::Horizontal, this);
    wetSlider->setRange(0, 100);
    wetSlider->setValue(100);
    wetSlider->setToolTip(tr("Wet/dry level."));
    wetLabel = new QLabel(tr("100%"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(wetSlider);
        row->addWidget(wetLabel);
        form->addRow(tr("Wet level:"), row);
    }

    // Предфильтр
    prefilterCheck = new QCheckBox(tr("Prefilter (reduces aliasing when shifting up)"), this);
    prefilterCheck->setChecked(true);
    form->addRow(prefilterCheck);

    mainLayout->addWidget(group);

    auto* note = new QLabel(
        tr("<small><i>Pitch shift applies on Ctrl+T.</i></small>"),
        this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    mainLayout->addStretch();

    closeBtn = new QPushButton(tr("Close"), this);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    setFixedWidth(400);

    // Соединения
    connect(enableCheck, &QCheckBox::toggled, this, [this](bool checked) {
        pitchSlider->setEnabled(checked);
        grainHzSlider->setEnabled(checked);
        shapeSlider->setEnabled(checked);
        jitterSlider->setEnabled(checked);
        wetSlider->setEnabled(checked);
        prefilterCheck->setEnabled(checked);
        onAnyChange();
    });
    connect(pitchSlider, &QSlider::valueChanged, this, [this](int v) {
        pitchLabel->setText(tr("%1 st").arg(v > 0 ? QString("+%1").arg(v) : QString::number(v)));
        onAnyChange();
    });
    connect(grainHzSlider, &QSlider::valueChanged, this, [this](int v) {
        grainHzLabel->setText(tr("%1 Hz").arg(v / 10.0, 0, 'f', 1));
        onAnyChange();
    });
    connect(shapeSlider, &QSlider::valueChanged, this, [this](int v) {
        shapeLabel->setText(tr("%1%").arg(v));
        onAnyChange();
    });
    connect(jitterSlider, &QSlider::valueChanged, this, [this](int v) {
        jitterLabel->setText(tr("%1%").arg(v));
        onAnyChange();
    });
    connect(wetSlider, &QSlider::valueChanged, this, [this](int v) {
        wetLabel->setText(tr("%1%").arg(v));
        onAnyChange();
    });
    connect(prefilterCheck, &QCheckBox::toggled, this, &PitchShiftSettingsDialog::onAnyChange);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    // Начальное состояние
    pitchSlider->setEnabled(false);
    grainHzSlider->setEnabled(false);
    shapeSlider->setEnabled(false);
    jitterSlider->setEnabled(false);
    wetSlider->setEnabled(false);
    prefilterCheck->setEnabled(false);
}

void PitchShiftSettingsDialog::onAnyChange()
{
    emit paramsChanged(getParams());
}

void PitchShiftSettingsDialog::setParams(const GranularEngine::Params& p)
{
    QSignalBlocker b1(enableCheck);  QSignalBlocker b2(pitchSlider);
    QSignalBlocker b3(grainHzSlider); QSignalBlocker b4(shapeSlider);
    QSignalBlocker b5(jitterSlider);  QSignalBlocker b6(wetSlider);
    QSignalBlocker b7(prefilterCheck);

    enableCheck->setChecked(p.enabled);
    pitchSlider->setValue(qRound(p.pitchSemitones));
    pitchLabel->setText(tr("%1 st").arg(pitchSlider->value() > 0
        ? QString("+%1").arg(pitchSlider->value())
        : QString::number(pitchSlider->value())));

    const int grainHzInt = qRound(p.grainHz * 10.f);
    grainHzSlider->setValue(grainHzInt);
    grainHzLabel->setText(tr("%1 Hz").arg(p.grainHz, 0, 'f', 1));

    shapeSlider->setValue(qRound(p.shape * 100.f));
    shapeLabel->setText(tr("%1%").arg(shapeSlider->value()));

    jitterSlider->setValue(qRound(p.jitter * 100.f));
    jitterLabel->setText(tr("%1%").arg(jitterSlider->value()));

    wetSlider->setValue(qRound(p.wet * 100.f));
    wetLabel->setText(tr("%1%").arg(wetSlider->value()));

    prefilterCheck->setChecked(p.prefilter);

    const bool en = p.enabled;
    pitchSlider->setEnabled(en);
    grainHzSlider->setEnabled(en);
    shapeSlider->setEnabled(en);
    jitterSlider->setEnabled(en);
    wetSlider->setEnabled(en);
    prefilterCheck->setEnabled(en);
}

GranularEngine::Params PitchShiftSettingsDialog::getParams() const
{
    GranularEngine::Params p;
    p.enabled        = enableCheck->isChecked();
    p.pitchSemitones = float(pitchSlider->value());
    p.grainHz        = float(grainHzSlider->value()) / 10.f;
    p.shape          = shapeSlider->value() / 100.f;
    p.jitter         = jitterSlider->value() / 100.f;
    p.wet            = wetSlider->value() / 100.f;
    p.prefilter      = prefilterCheck->isChecked();
    return p;
}
