#ifndef REVERBSETTINGSDIALOG_H
#define REVERBSETTINGSDIALOG_H

#include <QDialog>
#include "reverbsc_engine.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QSlider;
class QLabel;
class QPushButton;
class QDoubleSpinBox;
QT_END_NAMESPACE

class ReverbSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReverbSettingsDialog(QWidget* parent = nullptr);

    void setParams(const ReverbEngine::ReverbParams& p);
    ReverbEngine::ReverbParams getParams() const;

signals:
    void paramsChanged(const ReverbEngine::ReverbParams& p);

private slots:
    void onAnyChange();

private:
    void buildUi();

    QCheckBox*      enableCheck;
    QSlider*        feedbackSlider;
    QLabel*         feedbackLabel;
    QSlider*        lpFreqSlider;
    QLabel*         lpFreqLabel;
    QSlider*        wetSlider;
    QLabel*         wetLabel;
    QPushButton*    closeBtn;
};

#endif // REVERBSETTINGSDIALOG_H
