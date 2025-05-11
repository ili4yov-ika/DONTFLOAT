#include "AudioLoader.h"
#include <QDebug>

// Заглушка — заменить на libsndfile, miniaudio или Qt Multimedia
AudioData AudioLoader::loadFromFile(const QString &filePath) {
    qDebug() << "Loading audio file:" << filePath;

    AudioData data;
    data.sampleRate = 44100;
    data.channels = 2;
    data.samples = std::vector<float>(44100 * 2); // 1 секунда тишины

    return data;
}
