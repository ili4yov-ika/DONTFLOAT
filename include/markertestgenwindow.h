#ifndef MARKERTESTGENWINDOW_H
#define MARKERTESTGENWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include "markersfile.h"
#include "markerengine.h"

class QCloseEvent;
class QKeyEvent;
class QTimer;
class WaveformView;
class QLineEdit;
class QComboBox;
class QScrollBar;
class QPushButton;
class QLabel;

/**
 * @brief Утилита разметки тестовых файлов (tests/source4test).
 */
class MarkerTestGenWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MarkerTestGenWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openAudioFile();
    void saveProject();
    void playPause();
    void stopPlayback();
    void updateTime();
    void shiftGridBackward();
    void shiftGridForward();
    void snapMarkersToGrid();
    void onBpmEdited();
    void onBeatsPerBarChanged(int index);

private:
    void setupUi();
    void setupConnections();
    bool analyzeAndPlaceBeatMarkers();
    void alignViewToGrid();
    void shiftGridByBeats(int beatDelta);
    void syncPlaybackSource();
    void updateTimeLabel(qint64 ms);
    void setStatus(const QString& text, int timeoutMs = 3000);
    bool maybeSave();
    QVector<Marker> buildBeatMarkersFromGrid() const;

    WaveformView* m_waveformView = nullptr;
    QScrollBar* m_horizontalScrollBar = nullptr;
    QLineEdit* m_bpmEdit = nullptr;
    QComboBox* m_barsCombo = nullptr;
    QLabel* m_timeLabel = nullptr;
    QPushButton* m_playButton = nullptr;

    QMediaPlayer* m_mediaPlayer = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QTimer* m_playbackTimer = nullptr;

    QString m_audioPath;
    QString m_markersPath;
    QVector<QVector<float>> m_audioData;
    MarkersFileMeta m_meta;
    bool m_dirty = false;
    bool m_isPlaying = false;
};

#endif // MARKERTESTGENWINDOW_H
