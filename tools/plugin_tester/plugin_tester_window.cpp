#include "plugin_tester_window.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

using Dontfloat::PluginTester::PluginFormat;
using Dontfloat::PluginTester::PluginProduct;
using Dontfloat::PluginTester::ProbeResult;

PluginTesterWindow::PluginTesterWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("DONTFLOAT Plugin Tester"));
    resize(780, 520);
    setupUi();
    onSelectionChanged();
}

void PluginTesterWindow::setupUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("Format:"), central));
    formatCombo_ = new QComboBox(central);
    formatCombo_->addItem(QStringLiteral("CLAP"), int(PluginFormat::Clap));
    formatCombo_->addItem(QStringLiteral("VST3"), int(PluginFormat::Vst3));
    formatCombo_->addItem(QStringLiteral("LV2"), int(PluginFormat::Lv2));
    row->addWidget(formatCombo_, 1);

    row->addWidget(new QLabel(QStringLiteral("Plugin:"), central));
    productCombo_ = new QComboBox(central);
    productCombo_->addItem(QStringLiteral("DONTFLOAT"), int(PluginProduct::Full));
    productCombo_->addItem(QStringLiteral("DONTFLOAT Scratch"), int(PluginProduct::Scratch));
    productCombo_->addItem(QStringLiteral("DONTFLOAT Pitcher"), int(PluginProduct::Pitcher));
    row->addWidget(productCombo_, 1);
    root->addLayout(row);

    auto* pathRow = new QHBoxLayout();
    pathEdit_ = new QLineEdit(central);
    pathRow->addWidget(pathEdit_, 1);
    auto* browseBtn = new QPushButton(QStringLiteral("Browse…"), central);
    auto* defaultBtn = new QPushButton(QStringLiteral("Default path"), central);
    pathRow->addWidget(browseBtn);
    pathRow->addWidget(defaultBtn);
    root->addLayout(pathRow);

    auto* actionRow = new QHBoxLayout();
    probeButton_ = new QPushButton(QStringLiteral("Probe selected"), central);
    probeAllButton_ = new QPushButton(QStringLiteral("Probe all 9"), central);
    actionRow->addWidget(probeButton_);
    actionRow->addWidget(probeAllButton_);
    actionRow->addStretch(1);
    root->addLayout(actionRow);

    statusLabel_ = new QLabel(QStringLiteral("Ready"), central);
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    logEdit_ = new QPlainTextEdit(central);
    logEdit_->setReadOnly(true);
    logEdit_->setPlaceholderText(QStringLiteral("Probe log…"));
    root->addWidget(logEdit_, 1);

    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PluginTesterWindow::onSelectionChanged);
    connect(productCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PluginTesterWindow::onSelectionChanged);
    connect(browseBtn, &QPushButton::clicked, this, &PluginTesterWindow::browsePath);
    connect(defaultBtn, &QPushButton::clicked, this, &PluginTesterWindow::useDefaultPath);
    connect(probeButton_, &QPushButton::clicked, this, &PluginTesterWindow::runProbe);
    connect(probeAllButton_, &QPushButton::clicked, this, &PluginTesterWindow::runAllProbes);
}

PluginFormat PluginTesterWindow::currentFormat() const
{
    return static_cast<PluginFormat>(formatCombo_->currentData().toInt());
}

PluginProduct PluginTesterWindow::currentProduct() const
{
    return static_cast<PluginProduct>(productCombo_->currentData().toInt());
}

void PluginTesterWindow::onSelectionChanged()
{
    pathEdit_->setText(QDir::toNativeSeparators(
        Dontfloat::PluginTester::resolvePluginPath(currentFormat(), currentProduct())));
}

void PluginTesterWindow::browsePath()
{
    const PluginFormat format = currentFormat();
    QString path;
    if (format == PluginFormat::Clap) {
        path = QFileDialog::getOpenFileName(this, QStringLiteral("Select CLAP"), pathEdit_->text(),
                                            QStringLiteral("CLAP (*.clap);;All (*.*)"));
    } else if (format == PluginFormat::Vst3) {
        path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select VST3 bundle (.vst3)"),
                                                 pathEdit_->text());
        if (path.isEmpty()) {
            path = QFileDialog::getOpenFileName(this, QStringLiteral("Select VST3 module"),
                                                pathEdit_->text(),
                                                QStringLiteral("VST3 (*.vst3);;DLL (*.dll);;All (*.*)"));
        }
    } else {
        path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select LV2 bundle"),
                                                 pathEdit_->text());
    }
    if (!path.isEmpty()) {
        pathEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void PluginTesterWindow::useDefaultPath()
{
    onSelectionChanged();
}

void PluginTesterWindow::appendLog(const QString& text)
{
    logEdit_->appendPlainText(text);
}

void PluginTesterWindow::runProbe()
{
    const PluginFormat format = currentFormat();
    const PluginProduct product = currentProduct();
    const QString path = pathEdit_->text().trimmed();

    appendLog(QStringLiteral("—— %1 / %2 ——")
                  .arg(Dontfloat::PluginTester::formatLabel(format),
                       Dontfloat::PluginTester::productLabel(product)));

    const ProbeResult result = Dontfloat::PluginTester::probePlugin(format, product, path);
    for (const QString& line : result.details) {
        appendLog(QStringLiteral("  %1").arg(line));
    }
    appendLog(QStringLiteral("  path: %1").arg(result.path));
    appendLog(result.ok ? QStringLiteral("  OK: %1").arg(result.summary)
                        : QStringLiteral("  FAIL: %1").arg(result.summary));
    appendLog({});

    statusLabel_->setText(result.summary);
    statusLabel_->setStyleSheet(result.ok ? QStringLiteral("color: #1b7f2a;")
                                          : QStringLiteral("color: #b00020;"));
}

void PluginTesterWindow::runAllProbes()
{
    logEdit_->clear();
    int okCount = 0;
    int total = 0;
    for (int f = 0; f < 3; ++f) {
        for (int p = 0; p < 3; ++p) {
            ++total;
            const auto format = static_cast<PluginFormat>(f);
            const auto product = static_cast<PluginProduct>(p);
            formatCombo_->setCurrentIndex(f);
            productCombo_->setCurrentIndex(p);
            onSelectionChanged();
            const ProbeResult result =
                Dontfloat::PluginTester::probePlugin(format, product, pathEdit_->text());
            appendLog(QStringLiteral("%1 | %2 | %3 | %4")
                          .arg(Dontfloat::PluginTester::formatLabel(format),
                               Dontfloat::PluginTester::productLabel(product),
                               result.ok ? QStringLiteral("OK") : QStringLiteral("FAIL"),
                               result.summary));
            if (result.ok) {
                ++okCount;
            }
        }
    }
    const QString summary = QStringLiteral("All probes: %1/%2 OK").arg(okCount).arg(total);
    statusLabel_->setText(summary);
    statusLabel_->setStyleSheet(okCount == total ? QStringLiteral("color: #1b7f2a;")
                                                 : QStringLiteral("color: #b00020;"));
    appendLog({});
    appendLog(summary);
}
