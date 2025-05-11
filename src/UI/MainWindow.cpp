#include "UI/MainWindow.h"
#include "Audio/AudioLoader.h"
#include "Audio/WaveformRenderer.h"
#include "Utils/FileDialogHelper.h"

#include <QFileInfo>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QFile>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), waveformRenderer(new WaveformRenderer(this)) {
    setupUI();
    setupMenu();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->addWidget(waveformRenderer);
    setCentralWidget(central);
}

void MainWindow::setupMenu() {
    QMenu *fileMenu = menuBar()->addMenu("File");

    QAction *openAction = new QAction("Open Audio File...", this);
    fileMenu->addAction(openAction);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    QAction *openLastAction = new QAction("Open Last File", this);
    fileMenu->addAction(openLastAction);
    connect(openLastAction, &QAction::triggered, this, &MainWindow::onOpenLastFile);
}

void MainWindow::onOpenFile() {
    QString filePath = FileDialogHelper::openAudioFile(this);
    if (!filePath.isEmpty()) {
        loadAudioFile(filePath);
    }
}

void MainWindow::onOpenLastFile() {
    QString lastPath = FileDialogHelper::getLastOpenedFilePath();

    if (lastPath.isEmpty()) {
        QMessageBox::information(this, "Open Last File", "No previously opened file found.");
        return;
    }

    if (!QFile::exists(lastPath)) {
        QMessageBox::warning(this, "File Not Found", "The previously opened file no longer exists.");
        return;
    }

    loadAudioFile(lastPath);
}

void MainWindow::loadAudioFile(const QString &filePath) {
    AudioData audioData = AudioLoader::loadFromFile(filePath);
    waveformRenderer->setAudioData(audioData);
    setWindowTitle(QString("DONTFLOAT - %1").arg(QFileInfo(filePath).fileName()));
}
