#ifndef AUDIOFILESERVICE_H
#define AUDIOFILESERVICE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <functional>

// Декодирование аудиофайлов в float-сэмплы. UI-независимо (QtCore + QtMultimedia).
// Файл декодируется в НАТИВНОМ формате (без принудительного ресемплинга), любой формат
// сэмплов (UInt8/Int16/Int32/Float) конвертируется во float [-1; 1]. Берутся первые два канала.
namespace AudioFileService {

struct DecodeResult {
    QVector<QVector<float>> channels; // 1 (моно) или 2 (стерео) канала
    int sampleRate = 0;               // нативная частота дискретизации файла
    bool ok = false;
    QString error;                    // текст ошибки, когда ok == false
};

// Декодирует файл. onProgress (если задан) вызывается с процентом декодирования (0..99).
DecodeResult decode(const QString& filePath,
                    const std::function<void(int)>& onProgress = {});

// Усреднение каналов в моно-сигнал (для анализа BPM/тональности).
QVector<float> toMono(const QVector<QVector<float>>& channels);

} // namespace AudioFileService

#endif // AUDIOFILESERVICE_H
