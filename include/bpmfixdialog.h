#ifndef BPMFIXDIALOG_H
#define BPMFIXDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include "bpmanalyzer.h"

class BPMFixDialog : public QDialog
{
    Q_OBJECT

public:
    BPMFixDialog(QWidget *parent, const BPMAnalyzer::AnalysisResult&);
    bool shouldFixBeats() const { return fix; }
    void updateProgress(const QString& status, int progress);
    void showResult(const BPMAnalyzer::AnalysisResult& analysis);

private:
    QLabel *statusLabel;
    QProgressBar *progressBar;
    QLabel *infoLabel;
    QPushButton *fixButton;
    QPushButton *skipButton;
    bool fix;

private slots:
    void onFixClicked();
    void onSkipClicked();
};

#endif // BPMFIXDIALOG_H 