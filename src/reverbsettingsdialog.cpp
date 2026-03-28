#include "../include/reverbsettingsdialog.h"
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

ReverbSettingsDialog::ReverbSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Настройки реверберации"));
    setModal(false);
    buildUi();
}

void ReverbSettingsDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Включить/выключить ---
    enableCheck = new QCheckBox(tr("Применять реверберацию после растяжения"), this);
    // По умолчанию реверберация включена после растяжения
    enableCheck->setChecked(true);
    mainLayout->addWidget(enableCheck);

    // --- Параметры ---
    auto* group = new QGroupBox(tr("Параметры"), this);
    auto* form  = new QFormLayout(group);

    // feedback
    feedbackSlider = new QSlider(Qt::Horizontal, this);
    feedbackSlider->setRange(50, 99);  // 0.50 – 0.99
    feedbackSlider->setValue(92);      // 0.92 по умолчанию
    feedbackSlider->setToolTip(tr("Время реверберации (распад). Больше — дольше хвост."));
    feedbackLabel = new QLabel(tr("0.92"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(feedbackSlider);
        row->addWidget(feedbackLabel);
        form->addRow(tr("Затухание (feedback):"), row);
    }

    // lpfreq
    lpFreqSlider = new QSlider(Qt::Horizontal, this);
    lpFreqSlider->setRange(500, 20000);
    lpFreqSlider->setSingleStep(100);
    lpFreqSlider->setPageStep(500);
    lpFreqSlider->setValue(8000);
    lpFreqSlider->setToolTip(tr("Частота среза LP-фильтра в реверберации. Ниже — темнее, выше — ярче."));
    lpFreqLabel = new QLabel(tr("8000 Гц"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(lpFreqSlider);
        row->addWidget(lpFreqLabel);
        form->addRow(tr("LP-фильтр (Гц):"), row);
    }

    // wet mix
    wetSlider = new QSlider(Qt::Horizontal, this);
    wetSlider->setRange(0, 100);   // 0 – 100%
    wetSlider->setValue(25);       // 25% по умолчанию
    wetSlider->setToolTip(tr("Доля обработанного сигнала в финальном миксе (wet/dry)."));
    wetLabel = new QLabel(tr("25%"), this);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(wetSlider);
        row->addWidget(wetLabel);
        form->addRow(tr("Уровень (wet mix):"), row);
    }

    mainLayout->addWidget(group);

    // --- Подсказка ---
    auto* note = new QLabel(
        tr("<small><i>Реверберация применяется к аудио при нажатии Ctrl+T.</i></small>"),
        this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    mainLayout->addStretch();

    closeBtn = new QPushButton(tr("Закрыть"), this);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    setFixedWidth(380);

    // Соединения
    connect(enableCheck, &QCheckBox::toggled, this, [this](bool checked) {
        feedbackSlider->setEnabled(checked);
        lpFreqSlider->setEnabled(checked);
        wetSlider->setEnabled(checked);
        onAnyChange();
    });
    connect(feedbackSlider, &QSlider::valueChanged, this, [this](int v) {
        feedbackLabel->setText(QString::number(v / 100.0, 'f', 2));
        onAnyChange();
    });
    connect(lpFreqSlider, &QSlider::valueChanged, this, [this](int v) {
        lpFreqLabel->setText(tr("%1 Гц").arg(v));
        onAnyChange();
    });
    connect(wetSlider, &QSlider::valueChanged, this, [this](int v) {
        wetLabel->setText(tr("%1%").arg(v));
        onAnyChange();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    // Начальное состояние: параметры неактивны пока не включено
    feedbackSlider->setEnabled(false);
    lpFreqSlider->setEnabled(false);
    wetSlider->setEnabled(false);
}

void ReverbSettingsDialog::onAnyChange()
{
    emit paramsChanged(getParams());
}

void ReverbSettingsDialog::setParams(const ReverbEngine::ReverbParams& p)
{
    QSignalBlocker b1(enableCheck);
    QSignalBlocker b2(feedbackSlider);
    QSignalBlocker b3(lpFreqSlider);
    QSignalBlocker b4(wetSlider);

    enableCheck->setChecked(p.enabled);
    feedbackSlider->setValue(qRound(p.feedback * 100.f));
    feedbackLabel->setText(QString::number(p.feedback, 'f', 2));
    lpFreqSlider->setValue(qRound(p.lpFreq));
    lpFreqLabel->setText(tr("%1 Гц").arg(qRound(p.lpFreq)));
    wetSlider->setValue(qRound(p.wet * 100.f));
    wetLabel->setText(tr("%1%").arg(qRound(p.wet * 100.f)));

    feedbackSlider->setEnabled(p.enabled);
    lpFreqSlider->setEnabled(p.enabled);
    wetSlider->setEnabled(p.enabled);
}

ReverbEngine::ReverbParams ReverbSettingsDialog::getParams() const
{
    ReverbEngine::ReverbParams p;
    p.enabled  = enableCheck->isChecked();
    p.feedback = feedbackSlider->value() / 100.f;
    p.lpFreq   = float(lpFreqSlider->value());
    p.wet      = wetSlider->value() / 100.f;
    return p;
}
