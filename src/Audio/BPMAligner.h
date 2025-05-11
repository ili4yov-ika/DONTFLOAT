#pragma once

#include "AudioLoader.h"

class BPMAligner {
public:
    static AudioData alignBPM(const AudioData &inputData, float targetBPM);
};
