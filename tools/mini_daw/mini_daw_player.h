#ifndef DONTFLOAT_MINI_DAW_PLAYER_H
#define DONTFLOAT_MINI_DAW_PLAYER_H

// Транспорт мини-DAW: проигрывает стерео-буфер (уже прогнанный через плагин)
// через QAudioSink и отдаёт позицию каретки для полоски плейбека.

#include <QtCore/QByteArray>
#include <QtCore/QIODevice>
#include <QtCore/QObject>
#include <QtCore/QVector>

#include <atomic>

QT_BEGIN_NAMESPACE
class QAudioSink;
QT_END_NAMESPACE

namespace MiniDaw {

class Player : public QObject {
    Q_OBJECT

public:
    explicit Player(QObject* parent = nullptr);
    ~Player() override;

    /** Задаёт трек (левый/правый каналы одинаковой длины). */
    void setAudio(const QVector<float>& left, const QVector<float>& right, int sampleRate);
    void clear();

    void play();
    void stop();
    /** Перемотка; позиция ограничивается длиной трека. */
    void seek(qint64 frame);

    bool isPlaying() const;
    qint64 positionFrames() const;
    qint64 totalFrames() const { return totalFrames_; }
    int sampleRate() const { return sampleRate_; }
    bool hasAudio() const { return totalFrames_ > 0; }

signals:
    /** Дошли до конца трека (транспорт остановлен). */
    void finished();

private:
    /** Читает интерливнутый float32 из общего буфера, двигая позицию. */
    class StreamDevice : public QIODevice {
    public:
        explicit StreamDevice(Player* owner) : QIODevice(owner), owner_(owner) {}
        bool isSequential() const override { return true; }
        qint64 bytesAvailable() const override;

    protected:
        qint64 readData(char* data, qint64 maxSize) override;
        qint64 writeData(const char*, qint64) override { return -1; }

    private:
        Player* owner_ = nullptr;
    };

    void destroySink();

    QByteArray interleaved_;          ///< float32 L,R,L,R…
    std::atomic<qint64> position_ { 0 };  ///< позиция в фреймах
    qint64 totalFrames_ = 0;
    int sampleRate_ = 44100;
    QAudioSink* sink_ = nullptr;
    StreamDevice* device_ = nullptr;
};

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_PLAYER_H
