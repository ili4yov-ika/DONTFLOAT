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
#include <QSplitter>
#include <QResizeEvent>
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
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
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
    void clearLoopStart();
    void clearLoopEnd();
    void togglePitchGrid();
    void analyzeKey();
    void showKeyContextMenu(const QPoint& pos);
    void showKeyContextMenu2(const QPoint& pos);
    void setKey(const QString& key);
    void setKey2(const QString& key);
    void updateScrollBarTransparency();
    void setRussianLanguage();
    void setEnglishLanguage();
    void toggleBeatDeviations();

private:
    void createMenus();
    void createActions();
    void createKeyContextMenu();
    void createKeyContextMenu2();
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
    QString formatTime(qint64 msPosition);
    void updateHorizontalScrollBar(float zoom);
    void updateHorizontalScrollBarFromOffset(float offset);
    void constrainWindowSize();

    // UI components
    Ui::MainWindow *ui;
    WaveformView *waveformView;
    PitchGridWidget *pitchGridWidget;
    QScrollBar *horizontalScrollBar;
    QScrollBar *pitchGridVerticalScrollBar;
    QSplitter *mainSplitter;
    
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
    QAction *togglePitchGridAct;
    QAction *toggleBeatDeviationsAct;
    QAction *russianAction;
    QAction *englishAction;

    // File management
    QString currentFileName;
    bool hasUnsavedChanges;
    
    // Playback components
    bool isPlaying;
    qint64 currentPosition;
    QTimer *playbackTimer;
    QMediaPlayer *mediaPlayer;
    QAudioOutput *audioOutput;

    // Settings
    QSettings settings;

    // Metronome components
    QTimer *metronomeTimer;
    QMediaPlayer *metronomeSound;
    bool isMetronomeEnabled;
    qint64 lastBeatTime;
    
    // Loop components
    bool isLoopEnabled;
    qint64 loopStartPosition;
    qint64 loopEndPosition;

    // Pitch grid visibility
    bool isPitchGridVisible;

    // Key analysis
    QString currentKey;
    QString currentKey2; // For modulation
    QMenu *keyContextMenu;
    QMenu *keyContextMenu2; // For second key field
    QAction *keyActions[28]; // 14 major + 14 minor keys
    QAction *keyActions2[28]; // 14 major + 14 minor keys for second field

    // Undo/Redo stack
    QUndoStack *undoStack;
};

#endif // MAINWINDOW_H
