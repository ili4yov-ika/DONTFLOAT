#include "../include/beatvisualizationsettingsdialog.h"
#include "ui_beatvisualizationsettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDebug>
#include <QColorDialog>
#include <QPushButton>

BeatVisualizationSettingsDialog::BeatVisualizationSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BeatVisualizationSettingsDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8("Настройки визуализации ударных"));
    setModal(true);
    resize(400, 500);
}

BeatVisualizationSettingsDialog::~BeatVisualizationSettingsDialog()
{
    delete ui;
}

BeatVisualizer::VisualizationSettings BeatVisualizationSettingsDialog::getSettings() const
{
    return currentSettings;
}

void BeatVisualizationSettingsDialog::setSettings(const BeatVisualizer::VisualizationSettings& settings)
{
    currentSettings = settings;
}

void BeatVisualizationSettingsDialog::onMarkerOpacityChanged(int value)
{
    currentSettings.markerOpacity = value / 100.0;
    emit settingsChanged();
}

void BeatVisualizationSettingsDialog::onSpectrogramOpacityChanged(int value)
{
    currentSettings.spectrogramOpacity = value / 100.0;
    emit settingsChanged();
}

void BeatVisualizationSettingsDialog::onSpectrogramHeightChanged(int value)
{
    currentSettings.spectrogramHeight = value;
    emit settingsChanged();
}

void BeatVisualizationSettingsDialog::onSettingsChanged()
{
    emit settingsChanged();
}
