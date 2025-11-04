#ifndef METRONOMECONTROLLER_H
#define METRONOMECONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QMediaPlayer>
#include <QSoundEffect>
#include <QAudioOutput>
#include <QSettings>
#include <QDateTime>
#include <QtCore/QtGlobal>

class MetronomeController : public QObject
{
    Q_OBJECT

public:
    explicit MetronomeController(QObject *parent = nullptr);
    ~MetronomeController();

    void setBPM(float bpm);
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    
    void setPlaying(bool playing);
    void setStrongBeatVolume(int volume);
    void setWeakBeatVolume(int volume);
    int getStrongBeatVolume() const { return m_strongBeatVolume; }
    int getWeakBeatVolume() const { return m_weakBeatVolume; }
    
    void reset();

signals:
    void beatPlayed(bool isStrongBeat);

private slots:
    void onTimerTimeout();

private:
    void createMetronomeSound();
    void playBeat(bool isStrongBeat);

    QTimer *m_timer;
    QMediaPlayer *m_soundPlayer;
    QSoundEffect *m_soundEffect;
    
    bool m_enabled;
    bool m_playing;
    float m_bpm;
    int m_strongBeatVolume;
    int m_weakBeatVolume;
    
    qint64 m_lastBeatTime;
    int m_currentBeatNumber;
    QString m_soundFile;
};

#endif // METRONOMECONTROLLER_H

