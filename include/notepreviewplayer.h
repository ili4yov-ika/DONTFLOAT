#ifndef NOTEPREVIEWPLAYER_H
#define NOTEPREVIEWPLAYER_H

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QIODevice>

QT_BEGIN_NAMESPACE
class QAudioSink;
QT_END_NAMESPACE

/**
 * @brief Зацикленное прослушивание сегмента ноты при зажатом блоке на пианоролле.
 *
 * Пока блок ноты удерживается мышью, сегмент играет по кругу через QAudioSink.
 * Высота меняется мгновенно при перетаскивании: сегмент ресемплируется
 * (varispeed, 2^(полутоны/12)) и буфер цикла подменяется на лету.
 */
class NotePreviewPlayer : public QObject
{
    Q_OBJECT

public:
    explicit NotePreviewPlayer(QObject* parent = nullptr);
    ~NotePreviewPlayer() override;

    /** Запускает зацикленное воспроизведение сегмента со сдвигом в полутонах. */
    void start(const QVector<float>& monoSegment, int sampleRate, float semitoneOffset);
    /** Меняет сдвиг высоты (в полутонах от исходной) без остановки цикла. */
    void setSemitoneOffset(float semitones);
    void stop();
    bool isActive() const { return m_sink != nullptr; }

private:
    /** QIODevice, читающий буфер по кругу (для pull-режима QAudioSink). */
    class LoopDevice : public QIODevice
    {
    public:
        explicit LoopDevice(QObject* parent = nullptr) : QIODevice(parent) {}
        void setData(const QByteArray& bytes);
        bool isSequential() const override { return true; }
        qint64 bytesAvailable() const override;

    protected:
        qint64 readData(char* out, qint64 maxLen) override;
        qint64 writeData(const char*, qint64) override { return -1; }

    private:
        mutable QMutex m_mutex;
        QByteArray m_data;
        qint64 m_pos = 0;
    };

    QByteArray renderLoopBuffer() const;

    QAudioSink* m_sink = nullptr;
    LoopDevice* m_device = nullptr;
    QVector<float> m_segment;
    int m_sampleRate = 0;
    float m_semitones = 0.0f;
};

#endif // NOTEPREVIEWPLAYER_H
