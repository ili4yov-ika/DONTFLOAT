#ifndef DONTFLOAT_MINI_DAW_CLIP_MODEL_H
#define DONTFLOAT_MINI_DAW_CLIP_MODEL_H

/**
 * Клипы на дорожке мини-DAW: то, что настоящая DAW умеет делать с материалом —
 * резать, двигать, обрезать края и растягивать во времени.
 *
 * Модель намеренно вынесена из окна: плагин видит результат этих правок как
 * обычный поток блоков в process(), и проверять его поведение надо на том же
 * коде, который правит клипы в интерфейсе, а не на копии логики в тесте.
 * Ничего от GUI и от платформы здесь нет — модуль собирается и под Linux.
 */

#include <QtCore/QVector>
#include <QtCore/QtGlobal>

namespace MiniDaw {

/**
 * Кусок исходного файла со своей позицией на дорожке, длиной и растяжением.
 * `stretch` > 1 — клип звучит дольше исходного материала.
 */
struct Clip {
    qint64 timelineStart = 0;  ///< позиция на дорожке (кадры)
    qint64 sourceStart = 0;    ///< откуда берётся материал
    qint64 sourceLength = 0;   ///< сколько исходных кадров занимает
    double stretch = 1.0;      ///< >1 — растянут во времени

    qint64 timelineLength() const { return qint64(double(sourceLength) * stretch); }
    qint64 timelineEnd() const { return timelineStart + timelineLength(); }
};

/** Короче этого клип не режут и не обрезают — иначе остаётся щелчок. */
inline constexpr qint64 kMinClipFrames = 256;

/** Пределы растяжения: за ними материал разваливается. */
inline constexpr double kMinStretch = 0.25;
inline constexpr double kMaxStretch = 4.0;

/** Индекс клипа под позицией \a frame; -1 — там пусто. */
int clipAt(const QVector<Clip>& clips, qint64 frame);

/**
 * Собирает дорожку из клипов: растяжение — линейной интерполяцией по
 * исходному материалу, промежутки между клипами остаются тишиной.
 */
void renderTimeline(const QVector<Clip>& clips,
                    const QVector<float>& sourceLeft,
                    const QVector<float>& sourceRight,
                    QVector<float>& outLeft,
                    QVector<float>& outRight);

/** Границы клипов (начало и конец каждого) для отрисовки на дорожке. */
QVector<qint64> clipBoundaries(const QVector<Clip>& clips);

/**
 * Режет клип под позицией \a position на два.
 * @return индекс правой половины или -1, если резать нечего или рез у самого
 *         края (кусок короче kMinClipFrames никому не нужен).
 */
int splitClipAt(QVector<Clip>& clips, qint64 position);

/** Сдвигает клип по дорожке. @return false, если двигать нечего. */
bool moveClip(QVector<Clip>& clips, int index, qint64 deltaFrames);

/**
 * Двигает край клипа: \a startEdge — левый (вместе с точкой в материале),
 * иначе правый (меняется только длина куска).
 * @param sourceFrames длина исходного файла — за неё край не уходит
 * @return false, если сдвиг упёрся в предел и клип не изменился
 */
bool trimClip(QVector<Clip>& clips, int index, bool startEdge, qint64 deltaFrames,
              qint64 sourceFrames);

/**
 * Растягивает или сжимает клип во времени: коэффициент умножается на текущий
 * и зажимается в [kMinStretch, kMaxStretch].
 * @return false, если коэффициент не изменился
 */
bool stretchClip(QVector<Clip>& clips, int index, double factor);

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_CLIP_MODEL_H
