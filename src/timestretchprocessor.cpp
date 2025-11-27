#include "../include/timestretchprocessor.h"
#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <algorithm>
#include <cmath>

QVector<float> TimeStretchProcessor::processSegment(const QVector<float>& input, float stretchFactor)
{
    if (input.isEmpty() || stretchFactor <= 0.0f) {
        return input;
    }
    
    // Если коэффициент равен 1.0, возвращаем исходные данные
    if (qAbs(stretchFactor - 1.0f) < 0.001f) {
        return input;
    }
    
    int inputSize = input.size();
    int outputSize = static_cast<int>(inputSize * stretchFactor);
    
    if (outputSize <= 0) {
        return QVector<float>();
    }
    
    QVector<float> output;
    output.reserve(outputSize);
    
    // Используем кубическую интерполяцию для более качественного результата
    for (int i = 0; i < outputSize; ++i) {
        // Вычисляем позицию во входном массиве
        float inputPos = (static_cast<float>(i) / static_cast<float>(outputSize - 1)) * static_cast<float>(inputSize - 1);
        
        // Ограничиваем позицию границами массива
        inputPos = qBound(0.0f, inputPos, static_cast<float>(inputSize - 1));
        
        int index = static_cast<int>(inputPos);
        float fraction = inputPos - static_cast<float>(index);
        
        // Для кубической интерполяции нужны 4 точки
        int i0 = qMax(0, index - 1);
        int i1 = index;
        int i2 = qMin(inputSize - 1, index + 1);
        int i3 = qMin(inputSize - 1, index + 2);
        
        float y0 = input[i0];
        float y1 = input[i1];
        float y2 = input[i2];
        float y3 = input[i3];
        
        float value = cubicInterpolate(y0, y1, y2, y3, fraction);
        output.append(value);
    }
    
    return output;
}

QVector<QVector<float>> TimeStretchProcessor::processChannels(const QVector<QVector<float>>& input, float stretchFactor)
{
    QVector<QVector<float>> output;
    output.reserve(input.size());
    
    for (const auto& channel : input) {
        output.append(processSegment(channel, stretchFactor));
    }
    
    return output;
}

float TimeStretchProcessor::lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float TimeStretchProcessor::cubicInterpolate(float y0, float y1, float y2, float y3, float t)
{
    // Кубическая интерполяция Catmull-Rom
    float t2 = t * t;
    float t3 = t2 * t;
    
    float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float a2 = -0.5f * y0 + 0.5f * y2;
    float a3 = y1;
    
    return a0 * t3 + a1 * t2 + a2 * t + a3;
}

