#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class WaveformRenderer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void onOpenLastFile();

private:
    void setupUI();
    void setupMenu();
    void loadAudioFile(const QString &filePath);

    WaveformRenderer *waveformRenderer;
};

#endif // MAINWINDOW_H
