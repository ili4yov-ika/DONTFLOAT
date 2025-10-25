#ifndef BEATVISUALIZATIONSETTINGSDIALOG_H
#define BEATVISUALIZATIONSETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include "beatvisualizer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class BeatVisualizationSettingsDialog; }
QT_END_NAMESPACE

class BeatVisualizationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BeatVisualizationSettingsDialog(QWidget *parent = nullptr);
    ~BeatVisualizationSettingsDialog();

    BeatVisualizer::VisualizationSettings getSettings() const;
    void setSettings(const BeatVisualizer::VisualizationSettings& settings);

signals:
    void settingsChanged();

private slots:
    void onMarkerOpacityChanged(int value);
    void onSpectrogramOpacityChanged(int value);
    void onSpectrogramHeightChanged(int value);
    void onSettingsChanged();

private:
    void setupUI();
    void updateLabels();

    Ui::BeatVisualizationSettingsDialog *ui;
    
    // UI элементы
    QCheckBox *showBeatMarkersCheckBox;
    QCheckBox *showBeatTypesCheckBox;
    QCheckBox *showEnergyLevelsCheckBox;
    QCheckBox *showFrequencyInfoCheckBox;
    QCheckBox *useColorCodingCheckBox;
    QCheckBox *useSizeCodingCheckBox;
    QCheckBox *showSpectrogramCheckBox;
    
    QSlider *markerOpacitySlider;
    QSlider *spectrogramOpacitySlider;
    QSpinBox *spectrogramHeightSpinBox;
    
    QLabel *markerOpacityLabel;
    QLabel *spectrogramOpacityLabel;
    
    QDialogButtonBox *buttonBox;
    
    BeatVisualizer::VisualizationSettings currentSettings;
};

#endif // BEATVISUALIZATIONSETTINGSDIALOG_H
