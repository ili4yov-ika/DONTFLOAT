#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QScreen>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QSettings>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QAudioDecoder>
#include "waveformview.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openAudioFile();
    void playAudio();
    void stopAudio();
    void updateTime();
    void updateBPM();
    void processAudioFile(const QString& filePath);

private:
    void createMenus();
    void createActions();
    void updateWindowTitle();
    void setupConnections();
    void readSettings();
    void writeSettings();
    QVector<QVector<float>> loadAudioFile(const QString& filePath);

    Ui::MainWindow *ui;
    WaveformView *waveformView;
    
    // Menus
    QMenu *fileMenu;
    
    // Actions
    QAction *openAct;
    QAction *exitAct;

    // Current file
    QString currentFileName;
    
    // Playback state
    bool isPlaying;
    qint64 currentPosition;
    QTimer *playbackTimer;
    QMediaPlayer *mediaPlayer;
    QAudioOutput *audioOutput;

    // Settings
    QSettings settings;
};
#endif // MAINWINDOW_H
