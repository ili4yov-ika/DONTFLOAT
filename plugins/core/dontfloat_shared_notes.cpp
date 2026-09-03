#include "dontfloat_shared_notes.h"

#include "dontfloat_diagnostics.h"

#include <cstdio>

#include <map>
#include <mutex>

namespace Dontfloat::PluginCore {
namespace {

struct BoardState {
    std::mutex mutex;
    std::uint64_t nextInstanceId = 1;
    std::uint64_t nextRevision = 1;
    /** Записи экземпляров: id → последняя публикация. */
    std::map<std::uint64_t, SharedNoteSet> entries;
};

/** Одна доска на процесс; создаётся при первом обращении. */
BoardState& board()
{
    static BoardState state;
    return state;
}

} // namespace

std::uint64_t SharedNoteBoard::registerInstance()
{
    BoardState& state = board();
    const std::lock_guard<std::mutex> lock(state.mutex);
    return state.nextInstanceId++;
}

void SharedNoteBoard::unregisterInstance(std::uint64_t instanceId)
{
    BoardState& state = board();
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.entries.erase(instanceId);
}

void SharedNoteBoard::publish(std::uint64_t instanceId, const std::string& publisherName,
                              int sampleRate, const std::vector<TrackPitchNote>& notes)
{
    if (instanceId == 0) {
        return;
    }

    BoardState& state = board();
    const std::lock_guard<std::mutex> lock(state.mutex);
    if (notes.empty()) {
        state.entries.erase(instanceId);  // нечего показывать соседям
        return;
    }

    SharedNoteSet& entry = state.entries[instanceId];
    entry.stamp.publisherId = instanceId;
    entry.stamp.revision = state.nextRevision++;
    if (Diagnostics::enabled()) {
        char line[256];
        std::snprintf(line, sizeof(line),
                      "notes.publish instance=%llu revision=%llu count=%zu rate=%d name=%s",
                      static_cast<unsigned long long>(instanceId),
                      static_cast<unsigned long long>(entry.stamp.revision),
                      notes.size(), sampleRate, publisherName.c_str());
        Diagnostics::log(line);
    }
    entry.publisherName = publisherName;
    entry.sampleRate = sampleRate > 0 ? sampleRate : 44100;
    entry.notes = notes;
}

SharedNoteStamp SharedNoteBoard::latestStamp(std::uint64_t excludeInstanceId)
{
    BoardState& state = board();
    const std::lock_guard<std::mutex> lock(state.mutex);

    SharedNoteStamp latest;
    for (const auto& [instanceId, entry] : state.entries) {
        if (instanceId == excludeInstanceId) {
            continue;  // свои ноты референсом не показываем
        }
        if (entry.stamp.revision > latest.revision) {
            latest = entry.stamp;
        }
    }
    return latest;
}

SharedNoteSet SharedNoteBoard::latestFrom(std::uint64_t excludeInstanceId)
{
    BoardState& state = board();
    const std::lock_guard<std::mutex> lock(state.mutex);

    SharedNoteSet latest;
    for (const auto& [instanceId, entry] : state.entries) {
        if (instanceId == excludeInstanceId) {
            continue;
        }
        if (entry.stamp.revision > latest.stamp.revision) {
            latest = entry;
        }
    }
    if (Diagnostics::enabled() && !latest.empty()) {
        char line[256];
        std::snprintf(line, sizeof(line),
                      "notes.consume instance=%llu from=%llu revision=%llu count=%zu",
                      static_cast<unsigned long long>(excludeInstanceId),
                      static_cast<unsigned long long>(latest.stamp.publisherId),
                      static_cast<unsigned long long>(latest.stamp.revision),
                      latest.notes.size());
        Diagnostics::log(line);
    }
    return latest;
}

void SharedNoteBoard::resetForTests()
{
    BoardState& state = board();
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.entries.clear();
    state.nextInstanceId = 1;
    state.nextRevision = 1;
}

} // namespace Dontfloat::PluginCore
