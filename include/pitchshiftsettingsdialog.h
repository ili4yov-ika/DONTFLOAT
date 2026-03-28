#ifndef PITCHSHIFTSETTINGSDIALOG_H
#define PITCHSHIFTSETTINGSDIALOG_H

#include <QDialog>
#include "granularpitchshifter_engine.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QSlider;
class QLabel;
class QPushButton;
class QDoubleSpinBox;
QT_END_NAMESPACE

class PitchShiftSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PitchShiftSettingsDialog(QWidget* parent = nullptr);

    void setParams(const GranularEngine::Params& p);
    GranularEngine::Params getParams() const;

signals:
    void paramsChanged(const GranularEngine::Params& p);

private slots:
    void onAnyChange();

private:
    void buildUi();

    QCheckBox*  enableCheck;
    QSlider*    pitchSlider;
    QLabel*     pitchLabel;
    QSlider*    grainHzSlider;
    QLabel*     grainHzLabel;
    QSlider*    shapeSlider;
    QLabel*     shapeLabel;
    QSlider*    jitterSlider;
    QLabel*     jitterLabel;
    QSlider*    wetSlider;
    QLabel*     wetLabel;
    QCheckBox*  prefilterCheck;
    QPushButton* closeBtn;
};

#endif // PITCHSHIFTSETTINGSDIALOG_H
