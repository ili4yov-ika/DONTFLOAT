#include "bpmfixdialog.h"
#include <QStyle>
#include <QApplication>
#include <QTimer>

BPMFixDialog::BPMFixDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult&)
    : QDialog(parent)
    , fix(false)
{
    setWindowTitle(QString::fromUtf8("Анализ и выравнивание долей"));
    setMinimumWidth(450);
    setMinimumHeight(300);

    auto layout = new QVBoxLayout(this);

    // Статус процесса
    statusLabel = new QLabel(QString::fromUtf8("Анализ аудио..."), this);
    layout->addWidget(statusLabel);

    // Прогресс бар
    progressBar = new QProgressBar(this);
    progressBar->setMinimum(0);
    progressBar->setMaximum(100);
    layout->addWidget(progressBar);

    // Информационное сообщение
    infoLabel = new QLabel(this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Информация о BPM
    preliminaryBPMLabel = new QLabel(this);
    preliminaryBPMLabel->setText(QString::fromUtf8("Предварительный BPM (на основе Mixxx): -- BPM"));
    layout->addWidget(preliminaryBPMLabel);
    
    computedBPMLabel = new QLabel(this);
    computedBPMLabel->setText(QString::fromUtf8("Вычисленный BPM (уверенность --%): -- BPM"));
    layout->addWidget(computedBPMLabel);
    
    deviationLabel = new QLabel(this);
    deviationLabel->setText(QString::fromUtf8("Среднее отклонение: --%"));
    layout->addWidget(deviationLabel);
    
    // Чекбокс для пометки неровных долей
    markIrregularCheckbox = new QCheckBox(QString::fromUtf8("Пометить неровные доли"), this);
    markIrregularCheckbox->setChecked(true); // По умолчанию включен
    layout->addWidget(markIrregularCheckbox);

    // Кнопки
    auto buttonLayout = new QHBoxLayout;
    
    fixButton = new QPushButton(QString::fromUtf8("Выровнять"), this);
    fixButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    fixButton->setEnabled(false);
    connect(fixButton, &QPushButton::clicked, this, &BPMFixDialog::onFixClicked);
    
    skipButton = new QPushButton(QString::fromUtf8("Пропустить"), this);
    skipButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    skipButton->setEnabled(false);
    connect(skipButton, &QPushButton::clicked, this, &BPMFixDialog::onSkipClicked);
    
    buttonLayout->addWidget(fixButton);
    buttonLayout->addWidget(skipButton);
    
    layout->addLayout(buttonLayout);

    // Начальное состояние
    updateProgress(QString::fromUtf8("Анализ аудио..."), 0);
}

void BPMFixDialog::updateProgress(const QString& status, int progress)
{
    statusLabel->setText(status);
    progressBar->setValue(progress);
    QApplication::processEvents();
}

void BPMFixDialog::showResult(const BPMAnalyzer::AnalysisResult& analysis)
{
    // Общее сообщение
    infoLabel->setText(QString::fromUtf8("Анализ завершён."));
    
    // Предварительный BPM (на основе Mixxx), если доступен
    if (analysis.hasPreliminaryBPM) {
        preliminaryBPMLabel->setText(QString::fromUtf8("Предварительный BPM (на основе Mixxx): %1 BPM")
            .arg(analysis.preliminaryBPM, 0, 'f', 3));
    } else {
        preliminaryBPMLabel->setText(QString::fromUtf8("Предварительный BPM (на основе Mixxx): -- BPM"));
    }
    
    // Вычисленный BPM (уверенность)
    computedBPMLabel->setText(QString::fromUtf8("Вычисленный BPM (уверенность %1%): %2 BPM")
        .arg(analysis.confidence * 100.0f, 0, 'f', 0)
        .arg(analysis.bpm, 0, 'f', 0));
    
    // Среднее отклонение
    deviationLabel->setText(QString::fromUtf8("Среднее отклонение: %1%")
        .arg(analysis.averageDeviation * 100.0f, 0, 'f', 1));
    
    // Настраиваем чекбокс в зависимости от наличия неровных долей
    markIrregularCheckbox->setChecked(analysis.hasIrregularBeats);
    markIrregularCheckbox->setEnabled(true); // Всегда доступен для выбора
    
    // Обновляем текст чекбокса в зависимости от наличия неровных долей
    if (analysis.hasIrregularBeats) {
        markIrregularCheckbox->setText(QString::fromUtf8("Пометить неровные доли (рекомендуется)"));
    } else {
        markIrregularCheckbox->setText(QString::fromUtf8("Пометить неровные доли (доли ровные)"));
    }
    
    // Кнопки
    fixButton->setEnabled(true);
    skipButton->setEnabled(true);
    
    // Если доли ровные и уверенность высокая, предлагаем пропустить
    if (!analysis.hasIrregularBeats && analysis.confidence > 0.8f) {
        skipButton->setText(QString::fromUtf8("Пропустить (доли ровные)"));
    } else {
        skipButton->setText(QString::fromUtf8("Пропустить"));
    }
}

void BPMFixDialog::onFixClicked()
{
    fix = true;
    accept();
}

void BPMFixDialog::onSkipClicked()
{
    fix = false;
    reject();
} 