#ifndef LOADFILEDIALOG_H
#define LOADFILEDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QString>
#include "bpmanalyzer.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;
QT_END_NAMESPACE

namespace Ui {
class LoadFileDialog;
}

class LoadFileDialog : public QDialog
{
    Q_OBJECT

public:
    /** \a analysisResult не используется в конструкторе; данные подставляются через showResult(). */
    LoadFileDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult& analysisResult);
    ~LoadFileDialog();
    bool shouldFixBeats() const { return fix; }
    bool keepMarkersOnSkip() const;
    void setKeepMarkersOnSkip(bool keep);
    void updateProgress(const QString& status, int progress);
    void showResult(const BPMAnalyzer::AnalysisResult& analysis);

    /** Установить вычисленный размер такта в поле (например 4 для 4/4). */
    void setBeatsPerBar(int bpb);
    /** Текущее значение из поля (числитель размера такта, 1–32). */
    int getBeatsPerBar() const;

private:
    Ui::LoadFileDialog *ui;
    bool fix;

private slots:
    void on_fixButton_clicked();
    void on_skipButton_clicked();
};

#endif // LOADFILEDIALOG_H
