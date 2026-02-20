#include "../include/loadfiledialog.h"
#include "../ui_loadfiledialog.h"
#include <QStyle>
#include <QApplication>
#include <QTimer>

LoadFileDialog::LoadFileDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult& analysisResult)
    : QDialog(parent)
    , ui(new Ui::LoadFileDialog)
    , fix(false)
{
    Q_UNUSED(analysisResult); // данные подставляются через showResult()
    ui->setupUi(this);

    // Принудительно устанавливаем белый цвет для всех текстовых элементов
    ui->statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: white;");
    ui->infoLabel->setStyleSheet("font-size: 12px; color: white; padding: 5px;");
    ui->preliminaryBPMLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->computedBPMLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->deviationLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->timeSignatureLabel->setStyleSheet("font-size: 11px; color: white; padding: 2px;");
    ui->timeSignatureCombo->setStyleSheet("font-size: 11px; color: white; padding: 4px; background: palette(base);");
    ui->keepMarkersOnSkipCheckbox->setStyleSheet("font-size: 12px; color: white; padding: 5px;");

    // ItemData для размера такта: числитель (доли в такте) — 4, 3, 1, 2, 6, 12
    const QList<int> bpbValues = { 4, 3, 1, 2, 6, 12 };
    for (int i = 0; i < qMin(ui->timeSignatureCombo->count(), bpbValues.size()); ++i) {
        ui->timeSignatureCombo->setItemData(i, bpbValues[i]);
    }

    // Явное подключение кнопок (не полагаемся на connectSlotsByName из .ui)
    connect(ui->fixButton, &QPushButton::clicked, this, &LoadFileDialog::on_fixButton_clicked);
    connect(ui->skipButton, &QPushButton::clicked, this, &LoadFileDialog::on_skipButton_clicked);

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

    // Настраиваем чекбокс "Оставить метки" в зависимости от наличия неровных долей
    ui->keepMarkersOnSkipCheckbox->setChecked(analysis.hasIrregularBeats);
    ui->keepMarkersOnSkipCheckbox->setEnabled(true);

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

void LoadFileDialog::setBeatsPerBar(int bpb)
{
    bpb = qBound(1, bpb, 32);
    for (int i = 0; i < ui->timeSignatureCombo->count(); ++i) {
        if (ui->timeSignatureCombo->itemData(i).toInt() == bpb) {
            ui->timeSignatureCombo->setCurrentIndex(i);
            return;
        }
    }
    ui->timeSignatureCombo->setCurrentIndex(0); // 4/4 по умолчанию
}

int LoadFileDialog::getBeatsPerBar() const
{
    QVariant v = ui->timeSignatureCombo->currentData();
    if (v.isValid()) {
        int bpb = v.toInt();
        if (bpb >= 1 && bpb <= 32) return bpb;
    }
    return 4;
}

void LoadFileDialog::on_fixButton_clicked()
{
    fix = true;
    accept();
}

void LoadFileDialog::on_skipButton_clicked()
{
    fix = false;
    reject();
}

bool LoadFileDialog::keepMarkersOnSkip() const
{
    return ui->keepMarkersOnSkipCheckbox->isChecked();
}

void LoadFileDialog::setKeepMarkersOnSkip(bool keep)
{
    ui->keepMarkersOnSkipCheckbox->setChecked(keep);
}
