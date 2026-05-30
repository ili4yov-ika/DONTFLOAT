#ifndef WAVWRITER_H
#define WAVWRITER_H

#include <QtCore/QString>
#include <QtCore/QVector>

// Запись аудио в WAV (PCM 16/24-bit, IEEE float 32-bit, little-endian).
// Единая реализация для всех мест сохранения, чтобы не дублировать заголовок WAV
// и не расходиться в деталях (клэмпинг, размеры чанков и т.п.).
namespace WavWriter {

enum class SampleFormat {
    Pcm16,
    Pcm24,
    Float32
};

struct WriteOptions {
    SampleFormat format = SampleFormat::Pcm16;
    bool dither = true; // TPDF при квантовании в PCM16
};

// Пишет аудио (каналы x сэмплы float) в WAV по пути filePath.
// Для PCM сэмплы вне [-1; 1] ограничиваются; float пишется как есть.
// Размеры чанков считаются в 64-битах, поэтому переполнение заголовка
// для длинных файлов исключено (ограничение WAV — 4 ГБ на чанк data).
// Возвращает true при успехе; при ошибке (если задан errorMessage) пишет туда описание.
bool writeFile(const QString& filePath,
               const QVector<QVector<float>>& channels,
               int sampleRate,
               QString* errorMessage = nullptr,
               const WriteOptions& options = {});

} // namespace WavWriter

#endif // WAVWRITER_H
