#ifndef WAVEFORMPEAKS_H
#define WAVEFORMPEAKS_H

/**
 * @brief Пирамида пиков (min/max) для быстрой отрисовки волны.
 *
 * Раньше на каждый пиксель волна пересчитывалась по сырым сэмплам, причём с
 * прореживанием «не больше 200 проверок на пиксель»: на длинном файле это и
 * медленно, и неточно — короткие всплески между проверками терялись.
 *
 * Здесь min/max считаются один раз при загрузке дорожки и складываются в
 * уровни (каждый следующий вдвое грубее). Отрисовка берёт готовые значения
 * подходящего уровня — десятки чтений на столбец вместо сотен, и ни один
 * всплеск не теряется. Память: около 1.5% от размера самого аудио.
 */

#include <QtCore/QVector>
#include <QtCore/QtGlobal>

class WaveformPeaks
{
public:
    /** Сэмплов в корзине самого мелкого уровня. */
    static constexpr int kBaseBucketSamples = 256;
    /** Сколько корзин минимум должно попасть в запрошенный отрезок. */
    static constexpr int kMinBucketsPerRange = 16;

    /** Пересобирает пирамиду под \a samples (пустой вектор — очистка). */
    void build(const QVector<float>& samples);
    void clear();

    bool isValid() const { return sampleCount_ > 0 && !levels_.isEmpty(); }
    qint64 sampleCount() const { return sampleCount_; }

    /**
     * Точные min/max на отрезке [from, to) исходных сэмплов.
     * @param samples тот же вектор, по которому строилась пирамида
     * @return false, если отрезок пуст или пирамида не подходит к \a samples
     */
    bool range(const QVector<float>& samples, qint64 from, qint64 to,
               float& minValue, float& maxValue) const;

private:
    struct Bucket {
        float min = 0.0f;
        float max = 0.0f;
    };

    /** Уровень пирамиды: корзины по bucketSamples сэмплов. */
    struct Level {
        qint64 bucketSamples = kBaseBucketSamples;
        QVector<Bucket> buckets;
    };

    QVector<Level> levels_;
    qint64 sampleCount_ = 0;
};

#endif // WAVEFORMPEAKS_H
