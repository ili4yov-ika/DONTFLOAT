#include "../include/timestretchprocessor.h"
#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

QVector<float> TimeStretchProcessor::processSegment(const QVector<float>& input, float stretchFactor, bool preservePitch)
{
    if (input.isEmpty() || stretchFactor <= 0.0f) {
        return input;
    }
    
    // Если коэффициент равен 1.0, возвращаем исходные данные
    if (qAbs(stretchFactor - 1.0f) < 0.001f) {
        return input;
    }
    
    // Выбираем метод обработки
    if (preservePitch) {
        return processWithPitchPreservation(input, stretchFactor);
    } else {
        return processWithSimpleInterpolation(input, stretchFactor);
    }
}

QVector<float> TimeStretchProcessor::processWithSimpleInterpolation(const QVector<float>& input, float stretchFactor)
{
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

QVector<float> TimeStretchProcessor::processWithPitchPreservation(const QVector<float>& input, float stretchFactor)
{
    // Для сохранения pitch используем улучшенный алгоритм:
    // Вместо простой интерполяции используем ресемплинг с сохранением частотных характеристик
    
    int inputSize = input.size();
    int targetSize = static_cast<int>(inputSize * stretchFactor);
    
    if (targetSize <= 0) {
        return QVector<float>();
    }
    
    // Для тонкомпенсации используем более качественный алгоритм:
    // 1. Разбиваем на окна с перекрытием (overlap-add)
    // 2. Применяем оконную функцию для плавности
    // 3. Используем кубическую интерполяцию для качественного ресемплинга
    
    const int windowSize = 2048; // Размер окна для обработки
    const int hopSize = windowSize / 4; // Шаг окна (75% перекрытие)
    
    QVector<float> output;
    output.resize(targetSize, 0.0f);
    QVector<float> windowSum(targetSize, 0.0f); // Для нормализации
    
    // Оконная функция (Hann window)
    QVector<float> window(windowSize);
    for (int i = 0; i < windowSize; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (windowSize - 1)));
    }
    
    // Обрабатываем входной сигнал окнами
    for (int inputPos = 0; inputPos < inputSize - windowSize; inputPos += hopSize) {
        // Вычисляем соответствующую позицию в выходном сигнале
        float outputPosFloat = (static_cast<float>(inputPos) / static_cast<float>(inputSize)) * static_cast<float>(targetSize);
        int outputPos = static_cast<int>(outputPosFloat);
        
        if (outputPos + windowSize > targetSize) {
            break;
        }
        
        // Извлекаем окно из входного сигнала
        QVector<float> inputWindow(windowSize);
        for (int i = 0; i < windowSize; ++i) {
            int idx = inputPos + i;
            if (idx < inputSize) {
                inputWindow[i] = input[idx] * window[i];
            } else {
                inputWindow[i] = 0.0f;
            }
        }
        
        // Растягиваем/сжимаем окно с сохранением pitch через ресемплинг
        // Для сохранения pitch изменяем размер окна, но не частоту
        int windowOutputSize = static_cast<int>(windowSize * stretchFactor);
        QVector<float> outputWindow(windowOutputSize);
        
        // Ресемплируем окно
        for (int i = 0; i < windowOutputSize; ++i) {
            float pos = (static_cast<float>(i) / static_cast<float>(windowOutputSize - 1)) * static_cast<float>(windowSize - 1);
            pos = qBound(0.0f, pos, static_cast<float>(windowSize - 1));
            
            int index = static_cast<int>(pos);
            float fraction = pos - static_cast<float>(index);
            
            // Кубическая интерполяция
            int i0 = qMax(0, index - 1);
            int i1 = index;
            int i2 = qMin(windowSize - 1, index + 1);
            int i3 = qMin(windowSize - 1, index + 2);
            
            float y0 = inputWindow[i0];
            float y1 = inputWindow[i1];
            float y2 = inputWindow[i2];
            float y3 = inputWindow[i3];
            
            outputWindow[i] = cubicInterpolate(y0, y1, y2, y3, fraction);
        }
        
        // Применяем оконную функцию к выходному окну
        for (int i = 0; i < windowOutputSize && i < windowSize; ++i) {
            if (outputPos + i < targetSize) {
                outputWindow[i] *= window[i];
            }
        }
        
        // Добавляем окно к выходному сигналу (overlap-add)
        for (int i = 0; i < windowOutputSize && outputPos + i < targetSize; ++i) {
            output[outputPos + i] += outputWindow[i];
            windowSum[outputPos + i] += window[i % windowSize];
        }
    }
    
    // Нормализуем выходной сигнал
    for (int i = 0; i < targetSize; ++i) {
        if (windowSum[i] > 0.001f) {
            output[i] /= windowSum[i];
        }
    }
    
    // Если результат слишком короткий, используем простую интерполяцию как fallback
    if (output.size() < targetSize * 0.9f) {
        return processWithSimpleInterpolation(input, stretchFactor);
    }
    
    return output;
}

QVector<float> TimeStretchProcessor::resample(const QVector<float>& input, float inputSampleRate, float outputSampleRate)
{
    if (input.isEmpty() || inputSampleRate <= 0.0f || outputSampleRate <= 0.0f) {
        return input;
    }
    
    // Если частоты совпадают, возвращаем исходные данные
    if (qAbs(inputSampleRate - outputSampleRate) < 0.1f) {
        return input;
    }
    
    float ratio = outputSampleRate / inputSampleRate;
    int outputSize = static_cast<int>(input.size() * ratio);
    
    if (outputSize <= 0) {
        return QVector<float>();
    }
    
    QVector<float> output;
    output.reserve(outputSize);
    
    // Используем кубическую интерполяцию для качественного ресемплинга
    for (int i = 0; i < outputSize; ++i) {
        float inputPos = static_cast<float>(i) / ratio;
        inputPos = qBound(0.0f, inputPos, static_cast<float>(input.size() - 1));
        
        int index = static_cast<int>(inputPos);
        float fraction = inputPos - static_cast<float>(index);
        
        // Кубическая интерполяция
        int i0 = qMax(0, index - 1);
        int i1 = index;
        int i2 = qMin(static_cast<int>(input.size()) - 1, index + 1);
        int i3 = qMin(static_cast<int>(input.size()) - 1, index + 2);
        
        float y0 = input[i0];
        float y1 = input[i1];
        float y2 = input[i2];
        float y3 = input[i3];
        
        float value = cubicInterpolate(y0, y1, y2, y3, fraction);
        output.append(value);
    }
    
    return output;
}

QVector<QVector<float>> TimeStretchProcessor::processChannels(const QVector<QVector<float>>& input, float stretchFactor, bool preservePitch)
{
    QVector<QVector<float>> output;
    output.reserve(input.size());
    
    for (const auto& channel : input) {
        output.append(processSegment(channel, stretchFactor, preservePitch));
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

