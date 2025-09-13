#include "../include/loadfiledialog.h"
#include "../ui_loadfiledialog.h"
#include <QStyle>
#include <QApplication>
#include <QTimer>

LoadFileDialog::LoadFileDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult&)
    : QDialog(parent)
    , ui(new Ui::LoadFileDialog)
    , fix(false)
{
    ui->setupUi(this);
    
    // Принудительно устанавливаем белый цвет для всех текстовых элементов
    ui->statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    ui->infoLabel->setStyleSheet("font-size: 12px; color: white; padding: 5px;");
    ui->preliminaryBPMLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->computedBPMLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->deviationLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->markIrregularCheckbox->setStyleSheet("font-size: 12px; color: white; padding: 5px;");
    
    // Подключаем сигналы кнопок
    connect(ui->fixButton, &QPushButton::clicked, this, &LoadFileDialog::onFixClicked);
    connect(ui->skipButton, &QPushButton::clicked, this, &LoadFileDialog::onSkipClicked);

    // Начальное состояние
    updateProgress(QString::fromUtf8("Анализ аудио..."), 0);
}

LoadFileDialog::~LoadFileDialog()
{
    delete ui;
}

void LoadFileDialog::updateProgress(const QString& status, int progress)
{
    ui->statusLabel->setText(status);
    ui->statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    ui->progressBar->setValue(progress);
    QApplication::processEvents();
}

void LoadFileDialog::showResult(const BPMAnalyzer::AnalysisResult& analysis)
{
    // Общее сообщение
    ui->infoLabel->setText(QString::fromUtf8("Анализ завершён."));
    ui->infoLabel->setStyleSheet("font-size: 12px; color: white; padding: 5px;");
    
    // Предварительный BPM (на основе Mixxx), если доступен
    if (analysis.hasPreliminaryBPM) {
        ui->preliminaryBPMLabel->setText(QString::fromUtf8("Предварительный BPM (на основе Mixxx): %1 BPM")
            .arg(analysis.preliminaryBPM, 0, 'f', 3));
    } else {
        ui->preliminaryBPMLabel->setText(QString::fromUtf8("Предварительный BPM (на основе Mixxx): -- BPM"));
    }
    ui->preliminaryBPMLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    
    // Вычисленный BPM (уверенность)
    ui->computedBPMLabel->setText(QString::fromUtf8("Вычисленный BPM (уверенность %1%): %2 BPM")
        .arg(analysis.confidence * 100.0f, 0, 'f', 0)
        .arg(analysis.bpm, 0, 'f', 0));
    ui->computedBPMLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    
    // Среднее отклонение
    ui->deviationLabel->setText(QString::fromUtf8("Среднее отклонение: %1%")
        .arg(analysis.averageDeviation * 100.0f, 0, 'f', 1));
    ui->deviationLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    
    // Настраиваем чекбокс в зависимости от наличия неровных долей
    ui->markIrregularCheckbox->setChecked(analysis.hasIrregularBeats);
    ui->markIrregularCheckbox->setEnabled(true); // Всегда доступен для выбора
    
    // Обновляем текст чекбокса в зависимости от наличия неровных долей
    if (analysis.hasIrregularBeats) {
        ui->markIrregularCheckbox->setText(QString::fromUtf8("Пометить неровные доли (рекомендуется)"));
    } else {
        ui->markIrregularCheckbox->setText(QString::fromUtf8("Пометить неровные доли (доли ровные)"));
    }
    
    // Кнопки
    ui->fixButton->setEnabled(true);
    ui->skipButton->setEnabled(true);
    
    // Если доли ровные и уверенность высокая, предлагаем пропустить
    if (!analysis.hasIrregularBeats && analysis.confidence > 0.8f) {
        ui->skipButton->setText(QString::fromUtf8("Пропустить (доли ровные)"));
    } else {
        ui->skipButton->setText(QString::fromUtf8("Пропустить"));
    }
}

void LoadFileDialog::onFixClicked()
{
    fix = true;
    accept();
}

void LoadFileDialog::onSkipClicked()
{
    fix = false;
    reject();
}

bool LoadFileDialog::markIrregularBeats() const
{
    return ui->markIrregularCheckbox->isChecked();
}

void LoadFileDialog::setMarkIrregularBeats(bool mark)
{
    ui->markIrregularCheckbox->setChecked(mark);
} 