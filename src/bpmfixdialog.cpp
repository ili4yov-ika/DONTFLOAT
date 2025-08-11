#include "bpmfixdialog.h"
#include <QStyle>
#include <QApplication>
#include <QTimer>

BPMFixDialog::BPMFixDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult&)
    : QDialog(parent)
    , fix(false)
{
    setWindowTitle(QString::fromUtf8("Анализ и выравнивание долей"));
    setMinimumWidth(400);
    setMinimumHeight(200);

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
    QString message;
    QString tempoType = analysis.isFixedTempo ? 
        QString::fromUtf8("фиксированный") : 
        QString::fromUtf8("переменный");
    
    if (analysis.hasIrregularBeats) {
        message = QString::fromUtf8(
            "Обнаружены неровные доли в треке:\n\n"
            "BPM: %1 (уверенность: %2%%)\n"
            "Тип темпа: %3\n"
            "Среднее отклонение: %4%%\n"
            "Количество неровных долей: %5 из %6\n\n"
            "Рекомендуется выровнять доли для лучшего звучания.")
            .arg(analysis.bpm, 0, 'f', 1)
            .arg(analysis.confidence * 100.0f, 0, 'f', 0)
            .arg(tempoType)
            .arg(analysis.averageDeviation * 100.0f, 0, 'f', 1)
            .arg(int(analysis.beats.size() * analysis.averageDeviation))
            .arg(analysis.beats.size());
    } else {
        message = QString::fromUtf8(
            "Анализ завершен:\n\n"
            "BPM: %1 (уверенность: %2%%)\n"
            "Тип темпа: %3\n"
            "Среднее отклонение: %4%%\n"
            "Доли расположены достаточно ровно.")
            .arg(analysis.bpm, 0, 'f', 1)
            .arg(analysis.confidence * 100.0f, 0, 'f', 0)
            .arg(tempoType)
            .arg(analysis.averageDeviation * 100.0f, 0, 'f', 1);
    }

    infoLabel->setText(message);
    fixButton->setEnabled(analysis.hasIrregularBeats);
    skipButton->setEnabled(true);
    
    // Если доли ровные и уверенность высокая, автоматически закрываем диалог
    if (!analysis.hasIrregularBeats && analysis.confidence > 0.8f) {
        QTimer::singleShot(2000, this, &QDialog::accept);
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