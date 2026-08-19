#include "../include/waveformpeaks.h"

#include <algorithm>

void WaveformPeaks::clear()
{
    levels_.clear();
    sampleCount_ = 0;
}

void WaveformPeaks::build(const QVector<float>& samples)
{
    clear();
    if (samples.isEmpty()) {
        return;
    }
    sampleCount_ = samples.size();

    // Нижний уровень: min/max по корзинам сырых сэмплов
    Level base;
    base.bucketSamples = kBaseBucketSamples;
    const qint64 baseCount = (sampleCount_ + kBaseBucketSamples - 1) / kBaseBucketSamples;
    base.buckets.resize(int(baseCount));
    for (qint64 bucket = 0; bucket < baseCount; ++bucket) {
        const qint64 from = bucket * kBaseBucketSamples;
        const qint64 to = std::min<qint64>(from + kBaseBucketSamples, sampleCount_);
        float minValue = samples[int(from)];
        float maxValue = minValue;
        for (qint64 i = from + 1; i < to; ++i) {
            const float value = samples[int(i)];
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
        base.buckets[int(bucket)] = Bucket { minValue, maxValue };
    }
    levels_.append(std::move(base));

    // Каждый следующий уровень вдвое грубее предыдущего
    while (levels_.last().buckets.size() > 1) {
        const Level& previous = levels_.last();
        Level next;
        next.bucketSamples = previous.bucketSamples * 2;
        next.buckets.resize((previous.buckets.size() + 1) / 2);
        for (int i = 0; i < next.buckets.size(); ++i) {
            const Bucket& left = previous.buckets[i * 2];
            const bool hasRight = i * 2 + 1 < previous.buckets.size();
            const Bucket& right = hasRight ? previous.buckets[i * 2 + 1] : left;
            next.buckets[i] = Bucket { std::min(left.min, right.min),
                                       std::max(left.max, right.max) };
        }
        levels_.append(std::move(next));
    }
}

bool WaveformPeaks::range(const QVector<float>& samples, qint64 from, qint64 to,
                          float& minValue, float& maxValue) const
{
    if (samples.size() != sampleCount_ || sampleCount_ <= 0) {
        return false;  // пирамида не от этих сэмплов — пусть считает вызывающий
    }
    from = std::max<qint64>(0, from);
    to = std::min<qint64>(to, sampleCount_);
    if (to <= from) {
        return false;
    }

    const qint64 span = to - from;
    minValue = samples[int(from)];
    maxValue = minValue;

    // Вблизи (несколько сэмплов на пиксель) дешевле и точнее прочитать напрямую
    if (span <= kBaseBucketSamples * 2 || levels_.isEmpty()) {
        for (qint64 i = from + 1; i < to; ++i) {
            const float value = samples[int(i)];
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
        return true;
    }

    // Самый мелкий уровень, у которого в отрезок помещается хотя бы
    // kMinBucketsPerRange корзин: тогда захват соседних сэмплов по краям не
    // больше доли отрезка, а чтений — десятки вместо тысяч
    int levelIndex = 0;
    for (int i = 0; i < levels_.size(); ++i) {
        if (levels_[i].bucketSamples * kMinBucketsPerRange > span) {
            break;
        }
        levelIndex = i;
    }
    const Level& level = levels_[levelIndex];
    const qint64 bucketSamples = level.bucketSamples;

    // Берём все корзины, задевающие отрезок. Края слегка захватываются с
    // запасом: для волны важно не потерять всплеск, а лишний сэмпл соседней
    // корзины на глаз неразличим (и всё равно попадёт в соседний пиксель)
    const qint64 firstBucket = from / bucketSamples;
    const qint64 lastBucket = std::min<qint64>((to - 1) / bucketSamples,
                                               level.buckets.size() - 1);
    for (qint64 bucket = firstBucket; bucket <= lastBucket; ++bucket) {
        const Bucket& value = level.buckets[int(bucket)];
        minValue = std::min(minValue, value.min);
        maxValue = std::max(maxValue, value.max);
    }
    return true;
}
