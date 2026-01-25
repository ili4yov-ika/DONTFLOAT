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
    LoadFileDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult&);
    ~LoadFileDialog();
    bool shouldFixBeats() const { return fix; }
    bool keepMarkersOnSkip() const;
    void setKeepMarkersOnSkip(bool keep);
    void updateProgress(const QString& status, int progress);
    void showResult(const BPMAnalyzer::AnalysisResult& analysis);

private:
    Ui::LoadFileDialog *ui;
    bool fix;

private slots:
    void on_fixButton_clicked();
    void on_skipButton_clicked();
};

#endif // LOADFILEDIALOG_H
