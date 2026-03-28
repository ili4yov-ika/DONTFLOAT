#ifndef SPECTROGRAMSETTINGSDIALOG_H
#define SPECTROGRAMSETTINGSDIALOG_H

#include <QDialog>
#include "waveformview.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QSlider;
class QLabel;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
QT_END_NAMESPACE

class SpectrogramSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SpectrogramSettingsDialog(QWidget* parent = nullptr);

    void setSettings(const WaveformView::SpectrogramSettings& s);
    WaveformView::SpectrogramSettings getSettings() const;

signals:
    void settingsChanged(const WaveformView::SpectrogramSettings& s);

private slots:
    void onAnyChange();

private:
    void buildUi();

    QComboBox*      windowSizeCombo;
    QComboBox*      windowFuncCombo;
    QSlider*        maxFramesSlider;
    QLabel*         maxFramesLabel;
    QSlider*        freqBinsSlider;
    QLabel*         freqBinsLabel;
    QCheckBox*      logFreqCheck;
    QCheckBox*      dbAmplCheck;
    QDoubleSpinBox* floorDbSpin;
    QComboBox*      colorSchemeCombo;
    QPushButton*    closeBtn;
};

#endif // SPECTROGRAMSETTINGSDIALOG_H
