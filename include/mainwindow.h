#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QtGui/QShortcut>
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
#include <QUrl>
#include <QTranslator>
#include <functional>
#include "waveformview.h"
#include "spectrogramsettingsdialog.h"
#include "reverbsc_engine.h"
#include "pitchshiftsettingsdialog.h"
#include "granularpitchshifter_engine.h"
#include "pitchgridwidget.h"
#include "shortcutsdialog.h"
#include "metronomecontroller.h"
#include "keyselectionmenu.h"

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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    void toggleBeatWaveform();
    void applyTimeStretch();
    void updatePlaybackAfterMarkerDrag(); // Обновление воспроизведения после перетаскивания метки
    void onUndoStackChanged();
    void createOnsetMarkersAuto();        // Авто-метки по транзиентам (Onset detection)
    void snapAllMarkersToGrid();          // Привязать все метки к тактовой сетке
    void shiftBeatGridBackward();         // Сдвинуть тактовую сетку на один удар назад
    void shiftBeatGridForward();          // Сдвинуть тактовую сетку на один удар вперёд

private:
    void createMenus();
    void createActions();
    void updatePitchGridLayout();
    void updateWindowTitle();
    void setupConnections();
    // Применение цветовой схемы: фон виджетов и стили скроллбаров (см. :/styles/*.qss).
    // scheme: "dark" | "light" | "default" (тёмный фон) | "system" (сброс стилей).
    void applyWidgetBackgrounds(const QString& scheme);
    void applyScrollBarStyles(const QString& scheme);
    static QString loadStyleSheet(const QString& resourcePath);
    void readSettings();
    void writeSettings();
    void setupShortcuts();
    void applyShortcuts();
    bool maybeSave();
    bool doSaveAudioFile();
    void resetAudioState();
    void processAudioFile(const QString& filePath);
    /// Декодирует аудиофайл в его нативном формате (без принудительного ресемплинга).
    /// \a ok (если задан) выставляется в true только при успешном декодировании.
    /// \a onProgress (если задан) вызывается с процентом декодирования (0..100).
    QVector<QVector<float>> loadAudioFile(const QString& filePath,
                                          bool* ok = nullptr,
                                          const std::function<void(int)>& onProgress = {});
    /// Сброс A/B цикла и кнопок после загрузки нового файла
    void resetLoopStateAfterNewFile();
    void createDeviationMarkers(float tolerancePercent, bool neutralMarkers = false);
    void retranslateMenus();
    QString formatTimeAndBars(qint64 msPosition);
    void updateHorizontalScrollBar(float zoom);
    void updateHorizontalScrollBarFromOffset(float offset);
    void constrainWindowSize();
    void shiftBeatGridByBeats(int beatDelta);

    // Вспомогательные методы для рефакторинга
    void updateUIAfterAnalysis(const QVector<QVector<float>>& audioData,
                                const BPMAnalyzer::AnalysisResult& analysis,
                                int beatsPerBar);
    void updateUIAfterBeatFix(const QVector<QVector<float>>& fixedData,
                              const BPMAnalyzer::AnalysisResult& analysis,
                              int beatsPerBar);
    void setBPMAndBeatsPerBar(float bpm, int beatsPerBar);
    // Метки по каждой доле выровненной сетки (для «Выровнять» — как при «Пропустить» + оставить метки)
    QVector<Marker> createAlignedBeatMarkers(const QVector<BPMAnalyzer::BeatInfo>& alignedBeats,
                                              qint64 totalSamples, int sampleRate);
    QVector<BPMAnalyzer::BeatInfo> createAlignedBeatGrid(float bpm, qint64 gridStartSample,
                                                         qint64 totalSamples, int sampleRate,
                                                         const QVector<QVector<float>>& audioData);
    void applyBeatFixToWaveform(const QVector<QVector<float>>& originalData,
                                const QVector<QVector<float>>& fixedData,
                                const BPMAnalyzer::AnalysisResult& analysis,
                                int beatsPerBar);

    // Сохранение обработанного аудио во временный WAV-файл
    // и возврат пути к нему. Используется для того, чтобы
    // QMediaPlayer воспроизводил уже обработанное аудио.
    QString saveProcessedAudioToTempWav(const QVector<QVector<float>> &data, int sampleRate) const;
    void syncPlaybackWithWaveform();

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
    QMenu *themesMenu;
    QMenu *languageMenu;
    QMenu *waveformViewMenu;

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
    QAction *toggleBeatWaveformAct;
    QAction *waveformPeaksAct;
    QAction *waveformSpectrogramAct;
    QAction *spectrogramSettingsAct;
    SpectrogramSettingsDialog* spectrogramSettingsDialog;
    QAction *reverbAct;
    ReverbEngine::ReverbParams reverbParams;
    QAction *pitchShiftSettingsAct;
    PitchShiftSettingsDialog* pitchShiftSettingsDialog;
    GranularEngine::Params pitchShiftParams;
    QAction *russianAction;
    QAction *englishAction;
    QAction *applyTimeStretchAct;

    // File management
    QString currentFileName;
    bool hasUnsavedChanges;

    // Playback components
    bool isPlaying;
    qint64 currentPosition;
    QTimer *playbackTimer;
    QMediaPlayer *mediaPlayer;
    QAudioOutput *audioOutput;
    QTimer *markerPreviewTimer; // debounce для обновления воспроизведения после перетаскивания меток

    // Settings
    QSettings settings;
    QTranslator* m_appTranslator;

    // Metronome components
    MetronomeController *metronomeController;

    // Loop components
    bool isLoopEnabled;
    qint64 loopStartPosition;
    qint64 loopEndPosition;

    // Pitch grid visibility
    bool isPitchGridVisible;

    // Key analysis
    QString currentKey;
    QString currentKey2; // For modulation
    KeySelectionMenu *keyMenu;  // Контекстное меню для основного поля тональности
    KeySelectionMenu *keyMenu2; // Контекстное меню для поля модуляции

    // Undo/Redo stack
    QUndoStack *undoStack;

    // Shortcuts (QShortcut objects, keys updated in applyShortcuts)
    QShortcut *playShortcut;
    QShortcut *shiftAShortcut;
    QShortcut *shiftBShortcut;
    QShortcut *markerShortcut;
};

#endif // MAINWINDOW_H
