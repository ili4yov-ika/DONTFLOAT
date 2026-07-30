#ifndef DONTFLOAT_PLUGIN_TESTER_WINDOW_H
#define DONTFLOAT_PLUGIN_TESTER_WINDOW_H

#include "plugin_host_probe.h"

#include <QMainWindow>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;

class PluginTesterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PluginTesterWindow(QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();
    void browsePath();
    void useDefaultPath();
    void runProbe();
    void runAllProbes();

private:
    void setupUi();
    void appendLog(const QString& text);
    Dontfloat::PluginTester::PluginFormat currentFormat() const;
    Dontfloat::PluginTester::PluginProduct currentProduct() const;

    QComboBox* formatCombo_ = nullptr;
    QComboBox* productCombo_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPlainTextEdit* logEdit_ = nullptr;
    QPushButton* probeButton_ = nullptr;
    QPushButton* probeAllButton_ = nullptr;
};

#endif
