#include "mini_daw_clip_model.h"

#include <algorithm>
#include <cmath>

namespace MiniDaw {

int clipAt(const QVector<Clip>& clips, qint64 frame)
{
    for (int i = 0; i < clips.size(); ++i) {
        if (frame >= clips[i].timelineStart && frame < clips[i].timelineEnd()) {
            return i;
        }
    }
    return -1;
}

void renderTimeline(const QVector<Clip>& clips,
                    const QVector<float>& sourceLeft,
                    const QVector<float>& sourceRight,
                    QVector<float>& outLeft,
                    QVector<float>& outRight)
{
    outLeft.clear();
    outRight.clear();
    if (clips.isEmpty() || sourceLeft.isEmpty()) {
        return;
    }

    qint64 total = 0;
    for (const Clip& clip : clips) {
        total = std::max(total, clip.timelineEnd());
    }
    if (total <= 0) {
        return;
    }
    outLeft.assign(int(total), 0.0f);
    outRight.assign(int(total), 0.0f);

    const QVector<float>& right = sourceRight.isEmpty() ? sourceLeft : sourceRight;
    const int sourceFrames = int(std::min(sourceLeft.size(), right.size()));
    for (const Clip& clip : clips) {
        const qint64 length = clip.timelineLength();
        for (qint64 i = 0; i < length; ++i) {
            const qint64 outIndex = clip.timelineStart + i;
            if (outIndex < 0 || outIndex >= total) {
                continue;
            }
            // Растяжение — линейная интерполяция по исходному материалу
            const double sourcePos = double(clip.sourceStart) + double(i) / clip.stretch;
            const int base = int(sourcePos);
            if (base < 0 || base >= sourceFrames) {
                continue;
            }
            const int next = std::min(base + 1, sourceFrames - 1);
            const float t = float(sourcePos - double(base));
            outLeft[int(outIndex)] = sourceLeft[base] * (1.0f - t) + sourceLeft[next] * t;
            outRight[int(outIndex)] = right[base] * (1.0f - t) + right[next] * t;
        }
    }
}

QVector<qint64> clipBoundaries(const QVector<Clip>& clips)
{
    QVector<qint64> boundaries;
    boundaries.reserve(clips.size() * 2);
    for (const Clip& clip : clips) {
        boundaries.append(clip.timelineStart);
        boundaries.append(clip.timelineEnd());
    }
    return boundaries;
}

int splitClipAt(QVector<Clip>& clips, qint64 position)
{
    const int index = clipAt(clips, position);
    if (index < 0) {
        return -1;
    }

    Clip& clip = clips[index];
    const qint64 offsetInClip = position - clip.timelineStart;
    if (offsetInClip < kMinClipFrames || clip.timelineLength() - offsetInClip < kMinClipFrames) {
        return -1;  // рез у самого края: остался бы огрызок
    }

    // Правая половина начинается там, где закончилась левая — и по дорожке,
    // и по исходному материалу (с учётом растяжения)
    const qint64 sourceOffset = qint64(double(offsetInClip) / clip.stretch);
    Clip tail = clip;
    tail.timelineStart = position;
    tail.sourceStart = clip.sourceStart + sourceOffset;
    tail.sourceLength = clip.sourceLength - sourceOffset;
    clip.sourceLength = sourceOffset;
    clips.insert(index + 1, tail);
    return index + 1;
}

bool moveClip(QVector<Clip>& clips, int index, qint64 deltaFrames)
{
    if (index < 0 || index >= clips.size() || deltaFrames == 0) {
        return false;
    }
    Clip& clip = clips[index];
    const qint64 moved = std::max<qint64>(0, clip.timelineStart + deltaFrames);
    if (moved == clip.timelineStart) {
        return false;
    }
    clip.timelineStart = moved;
    return true;
}

bool trimClip(QVector<Clip>& clips, int index, bool startEdge, qint64 deltaFrames,
              qint64 sourceFrames)
{
    if (index < 0 || index >= clips.size() || deltaFrames == 0 || sourceFrames <= 0) {
        return false;
    }
    Clip& clip = clips[index];

    if (startEdge) {
        // Левый край: двигаем и позицию на дорожке, и точку в материале
        const qint64 sourceDelta = qint64(double(deltaFrames) / clip.stretch);
        const qint64 newSourceStart =
            std::clamp<qint64>(clip.sourceStart + sourceDelta, 0, sourceFrames - kMinClipFrames);
        const qint64 applied = newSourceStart - clip.sourceStart;
        if (applied == 0 || clip.sourceLength - applied < kMinClipFrames) {
            return false;
        }
        clip.sourceStart = newSourceStart;
        clip.sourceLength -= applied;
        clip.timelineStart =
            std::max<qint64>(0, clip.timelineStart + qint64(double(applied) * clip.stretch));
        return true;
    }

    // Правый край: меняем только длину куска материала
    const qint64 sourceDelta = qint64(double(deltaFrames) / clip.stretch);
    const qint64 maxLength = sourceFrames - clip.sourceStart;
    const qint64 newLength =
        std::clamp<qint64>(clip.sourceLength + sourceDelta, kMinClipFrames, maxLength);
    if (newLength == clip.sourceLength) {
        return false;
    }
    clip.sourceLength = newLength;
    return true;
}

bool stretchClip(QVector<Clip>& clips, int index, double factor)
{
    if (index < 0 || index >= clips.size() || factor <= 0.0) {
        return false;
    }
    Clip& clip = clips[index];
    const double stretch = std::clamp(clip.stretch * factor, kMinStretch, kMaxStretch);
    if (std::fabs(stretch - clip.stretch) < 1e-6) {
        return false;
    }
    clip.stretch = stretch;
    return true;
}

} // namespace MiniDaw
