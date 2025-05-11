#pragma once

#include <QString>
#include <vector>

struct AudioData {
    int sampleRate;
    int channels;
    std::vector<float> samples; // интерлированные данные
};

class AudioLoader {
public:
    static AudioData loadFromFile(const QString &filePath);
};
