#ifndef DONTFLOAT_SHARED_NOTES_H
#define DONTFLOAT_SHARED_NOTES_H

/**
 * Общая на процесс доска нот: экземпляр плагина выкладывает на неё свои
 * разобранные ноты, а соседние экземпляры берут их как референс.
 *
 * Зачем: в DAW на одной дорожке висит DONTFLOAT (или Pitcher), на другой —
 * второй экземпляр; ноты и тональности первого нужны второму фоном для сверки.
 * Все экземпляры формата живут в одном процессе хоста, поэтому статической
 * доски достаточно — межпроцессный канал не нужен.
 *
 * Координаты нот — сэмплы таймлайна DAW (см. TrackToolSession::writeHostFrames),
 * поэтому на другой дорожке они встают на те же такты. Частота дискретизации
 * публикуется вместе с нотами: получатель пересчитывает позиции, если у него
 * она другая.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "dontfloat_plugin_core.h"

namespace Dontfloat::PluginCore {

/** Кто и когда выложил ноты — дешёвая проверка «изменилось ли». */
struct SharedNoteStamp {
    std::uint64_t publisherId = 0;  ///< 0 — на доске никого нет
    std::uint64_t revision = 0;     ///< растёт на каждой публикации
};

/** Ноты соседнего экземпляра вместе с тем, чьи они. */
struct SharedNoteSet {
    SharedNoteStamp stamp;
    std::string publisherName;  ///< имя редакции: «DONTFLOAT Pitcher» и т. п.
    int sampleRate = 44100;
    std::vector<TrackPitchNote> notes;
    bool empty() const { return notes.empty(); }
};

/**
 * Доска нот. Все методы потокобезопасны; вызываются из UI-потока редактора.
 */
class SharedNoteBoard {
public:
    /** Регистрирует экземпляр и выдаёт ему идентификатор (никогда не 0). */
    static std::uint64_t registerInstance();
    /** Убирает экземпляр и его ноты с доски (вызывать в деструкторе). */
    static void unregisterInstance(std::uint64_t instanceId);

    /** Выкладывает ноты экземпляра; пустой список стирает его запись. */
    static void publish(std::uint64_t instanceId, const std::string& publisherName,
                        int sampleRate, const std::vector<TrackPitchNote>& notes);

    /**
     * Отметка последней публикации **не** от \a excludeInstanceId.
     * Сравнив её с уже применённой, редактор понимает, надо ли забирать ноты.
     */
    static SharedNoteStamp latestStamp(std::uint64_t excludeInstanceId);

    /** Сами ноты последней публикации не от \a excludeInstanceId. */
    static SharedNoteSet latestFrom(std::uint64_t excludeInstanceId);

    /** Только для тестов: полностью очищает доску. */
    static void resetForTests();
};

} // namespace Dontfloat::PluginCore

#endif // DONTFLOAT_SHARED_NOTES_H
