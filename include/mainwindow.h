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
#include <QScrollBar>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QAudioDecoder>
#include <QSoundEffect>
#include <QUndoStack>
#include "waveformview.h"
#include "pitchgridwidget.h"

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
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void openAudioFile();
    void saveAudioFile();
    void playAudio();
    void stopAudio();
    void updateTime();
    void updateBPM();
    void increaseBPM();
    void decreaseBPM();
    void updateTimeLabel(qint64 msPosition);
    void updatePlaybackPosition(qint64 position);
    void toggleMetronome();
    void toggleLoop();
    void updateLoopPoints();
    void showMetronomeSettings();
    void setColorScheme(const QString& scheme);
    void setTheme(const QString& theme);
    void showKeyboardShortcuts();
    void setLoopStart();
    void setLoopEnd();

private:
    void createMenus();
    void createActions();
    void updateWindowTitle();
    void setupConnections();
    void readSettings();
    void writeSettings();
    void setupShortcuts();
    bool maybeSave();
    bool doSaveAudioFile();
    void resetAudioState();
    void processAudioFile(const QString& filePath);
    QVector<QVector<float>> loadAudioFile(const QString& filePath);
    void createSimpleMetronomeSound();

    // UI components
    Ui::MainWindow *ui;
    WaveformView *waveformView;
    PitchGridWidget *pitchGridWidget;
    QScrollBar *horizontalScrollBar;
    QScrollBar *waveformVerticalScrollBar;
    QScrollBar *pitchGridVerticalScrollBar;
    
    // Menu components
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *viewMenu;
    QMenu *settingsMenu;
    QMenu *colorSchemeMenu;
    
    // Action components
    QAction *openAct;
    QAction *saveAct;
    QAction *exitAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *defaultThemeAct;
    QAction *darkSchemeAct;
    QAction *lightSchemeAct;
    QAction *metronomeSettingsAct;
    QAction *keyboardShortcutsAct;
    QAction *playPauseAct;
    QAction *stopAct;
    QAction *metronomeAct;
    QAction *loopStartAct;
    QAction *loopEndAct;

    // File management
    QString currentFileName;
    bool hasUnsavedChanges = false;
    
    // Playback components
    bool isPlaying = false;
    qint64 currentPosition = 0;
    QTimer *playbackTimer = nullptr;
    QMediaPlayer *mediaPlayer = nullptr;
    QAudioOutput *audioOutput = nullptr;

    // Settings
    QSettings settings;

    // Metronome components
    QTimer *metronomeTimer = nullptr;
    QSoundEffect *metronomeSound = nullptr;
    bool isMetronomeEnabled = false;
    qint64 lastBeatTime = 0;
    
    // Loop components
    bool isLoopEnabled = false;
    qint64 loopStartPosition = 0;
    qint64 loopEndPosition = 0;

    // Undo/Redo stack
    QUndoStack *undoStack;
};

#endif // MAINWINDOW_H
