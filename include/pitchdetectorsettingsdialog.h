#ifndef PITCHDETECTORSETTINGSDIALOG_H
#define PITCHDETECTORSETTINGSDIALOG_H

#include <QDialog>
#include "pitchdetector.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

/**
 * Настройки питчера: эталон строя (A4) и параметры поиска нот.
 *
 * Изменения применяются к следующему анализу — уже найденные ноты не
 * пересчитываются, иначе правки в пианоролле терялись бы молча.
 */
class PitchDetectorSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PitchDetectorSettingsDialog(QWidget* parent = nullptr);

    void setOptions(const PitchDetector::Options& o);
    PitchDetector::Options getOptions() const;

signals:
    void optionsChanged(const PitchDetector::Options& o);

private slots:
    void onAnyChange();
    void onStandardPicked(int index);
    void onRestoreDefaults();

private:
    void buildUi();
    void refreshDerivedLabels();

    QComboBox*      standardCombo;
    QDoubleSpinBox* referenceSpin;
    QLabel*         referenceHintLabel;
    QDoubleSpinBox* minFreqSpin;
    QDoubleSpinBox* maxFreqSpin;
    QLabel*         rangeHintLabel;
    QSpinBox*       minDurationSpin;
    QDoubleSpinBox* minRmsSpin;
    QDoubleSpinBox* minConfidenceSpin;
    QPushButton*    defaultsBtn;
    QPushButton*    closeBtn;

    bool updating = false;
};

#endif // PITCHDETECTORSETTINGSDIALOG_H
