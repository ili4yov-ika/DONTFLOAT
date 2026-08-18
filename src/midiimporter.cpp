#include "../include/midiimporter.h"

#include "../include/keyanalyzer.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPair>

#include <algorithm>
#include <cmath>

namespace MidiImporter {
namespace {

/** Событие ноты в тиках — промежуточный вид при разборе дорожек. */
struct RawEvent {
    int tick = 0;
    bool on = false;
    int pitch = 0;
};

struct ParsedNote {
    int startTick = 0;
    int endTick = 0;
    int pitch = 60;
};

int readVlq(const QByteArray& data, int& index)
{
    int value = 0;
    while (index < data.size()) {
        const auto byte = static_cast<unsigned char>(data.at(index++));
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    return value;
}

/** Разбор SMF: ноты в тиках и темп из мета-события. */
bool parseSmf(const QByteArray& data, QVector<ParsedNote>* notes, int* ticksPerQuarter, float* bpm)
{
    if (data.size() < 14 || !data.startsWith("MThd")) {
        return false;
    }

    const auto u16 = [&](int offset) {
        return (static_cast<unsigned char>(data.at(offset)) << 8)
            | static_cast<unsigned char>(data.at(offset + 1));
    };
    const auto u32 = [&](int offset) {
        return (static_cast<unsigned char>(data.at(offset)) << 24)
            | (static_cast<unsigned char>(data.at(offset + 1)) << 16)
            | (static_cast<unsigned char>(data.at(offset + 2)) << 8)
            | static_cast<unsigned char>(data.at(offset + 3));
    };

    const int trackCount = u16(10);
    *ticksPerQuarter = std::max(1, u16(12));

    QVector<RawEvent> raw;
    bool bpmFound = false;
    int index = 14;
    for (int track = 0; track < trackCount && index + 8 <= data.size(); ++track) {
        if (data.mid(index, 4) != "MTrk") {
            break;
        }
        index += 4;
        const int length = u32(index);
        index += 4;
        const int end = std::min(index + length, int(data.size()));

        int tick = 0;
        int running = -1;
        while (index < end) {
            tick += readVlq(data, index);
            if (index >= end) {
                break;
            }
            const auto statusByte = static_cast<unsigned char>(data.at(index));
            int status = statusByte;
            if (statusByte < 0x80) {
                status = running;  // running status: статус повторяет предыдущий
            } else {
                ++index;
                running = (status < 0xF0) ? status : -1;
            }

            if (status == 0xFF) {
                if (index >= end) {
                    break;
                }
                const auto meta = static_cast<unsigned char>(data.at(index++));
                const int metaLength = readVlq(data, index);
                if (meta == 0x51 && metaLength == 3 && index + 3 <= data.size() && !bpmFound) {
                    const int microseconds = (static_cast<unsigned char>(data.at(index)) << 16)
                        | (static_cast<unsigned char>(data.at(index + 1)) << 8)
                        | static_cast<unsigned char>(data.at(index + 2));
                    if (microseconds > 0) {
                        *bpm = float(60000000.0 / double(microseconds));
                        bpmFound = true;
                    }
                }
                index += metaLength;
                if (meta == 0x2F) {
                    break;
                }
            } else if (status == 0xF0 || status == 0xF7) {
                index += readVlq(data, index);
            } else if (status < 0) {
                break;  // running status без предыдущего события — дорожка битая
            } else {
                const int type = status & 0xF0;
                if (type == 0x80 || type == 0x90 || type == 0xA0 || type == 0xB0 || type == 0xE0) {
                    if (index + 1 >= data.size()) {
                        break;
                    }
                    const int first = static_cast<unsigned char>(data.at(index++));
                    const int second = static_cast<unsigned char>(data.at(index++));
                    if (type == 0x90 || type == 0x80) {
                        RawEvent event;
                        event.tick = tick;
                        // note-on с нулевой громкостью — это выключение ноты
                        event.on = (type == 0x90 && second != 0);
                        event.pitch = first;
                        raw.append(event);
                    }
                } else if (type == 0xC0 || type == 0xD0) {
                    ++index;
                }
            }
        }
        index = end;
    }

    // Пары включение/выключение по высоте (в порядке прихода)
    QHash<int, QVector<int>> open;
    for (const RawEvent& event : raw) {
        if (event.on) {
            open[event.pitch].append(event.tick);
        } else if (!open[event.pitch].isEmpty()) {
            const int startTick = open[event.pitch].takeFirst();
            if (event.tick > startTick) {
                notes->append({ startTick, event.tick, event.pitch });
            }
        }
    }
    std::stable_sort(notes->begin(), notes->end(), [](const ParsedNote& a, const ParsedNote& b) {
        return a.startTick < b.startTick;
    });
    return true;
}

} // namespace

Result readFile(const QString& path, const Options& options)
{
    Result result;
    result.sourceBpm = 120.0f;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }

    QVector<ParsedNote> parsed;
    if (!parseSmf(file.readAll(), &parsed, &result.ticksPerQuarter, &result.sourceBpm)) {
        result.error = QObject::tr("This is not a MIDI file (no MThd header)");
        return result;
    }
    if (parsed.isEmpty()) {
        result.error = QObject::tr("The MIDI file has no notes");
        return result;
    }

    const int sampleRate = options.sampleRate > 0 ? options.sampleRate : 44100;
    const float projectBpm = options.projectBpm > 0.0f ? options.projectBpm : 120.0f;
    // «Как есть» звучит в темпе файла, остальные режимы — в темпе проекта
    const float bpm = (options.mode == TimingMode::KeepAsIs) ? result.sourceBpm : projectBpm;
    const double samplesPerTick =
        (60.0 / double(bpm > 0.0f ? bpm : 120.0f)) * double(sampleRate) / double(result.ticksPerQuarter);

    for (const ParsedNote& note : parsed) {
        PitchDetector::PitchNote out;
        out.startSample = qint64(double(note.startTick) * samplesPerTick);
        out.endSample = std::max(out.startSample + 1, qint64(double(note.endTick) * samplesPerTick));
        out.midiPitch = float(note.pitch);
        out.detectedPitch = out.midiPitch;
        out.confidence = 1.0f;
        result.notes.append(out);
    }

    if (options.mode == TimingMode::AlignAndFitToBpm && !result.notes.isEmpty()) {
        // Выравнивание: первая нота встаёт на начало тактовой сетки проекта
        const qint64 shift = options.gridStartSample - result.notes.first().startSample;
        for (PitchDetector::PitchNote& note : result.notes) {
            note.startSample = std::max<qint64>(0, note.startSample + shift);
            note.endSample = std::max(note.startSample + 1, note.endSample + shift);
        }
    }

    result.ok = true;
    return result;
}

namespace {

int pitchClassOf(const PitchDetector::PitchNote& note)
{
    return ((int(std::lround(note.midiPitch)) % 12) + 12) % 12;
}

/**
 * Хрома по длительностям звучания на отрезке [rangeStart, rangeEnd):
 * чем дольше звучит класс высоты, тем он весомее. Отрезок без нот даёт
 * пустой вектор. Масштаб — как у хромы из звука (максимум = 1).
 */
QVector<float> chromaForRange(const QVector<PitchDetector::PitchNote>& notes,
                              qint64 rangeStart, qint64 rangeEnd)
{
    QVector<float> chroma(12, 0.0f);
    for (const PitchDetector::PitchNote& note : notes) {
        const qint64 from = std::max(note.startSample, rangeStart);
        const qint64 to = std::min(std::max(note.endSample, note.startSample + 1), rangeEnd);
        if (to <= from) {
            continue;
        }
        chroma[pitchClassOf(note)] += float(to - from);
    }

    float maxValue = 0.0f;
    for (float value : chroma) {
        maxValue = std::max(maxValue, value);
    }
    if (maxValue <= 0.0f) {
        return {};
    }
    for (float& value : chroma) {
        value /= maxValue;
    }
    return chroma;
}

} // namespace

QString detectKey(const QVector<PitchDetector::PitchNote>& notes)
{
    if (notes.isEmpty()) {
        return {};
    }

    qint64 lastSample = 0;
    for (const PitchDetector::PitchNote& note : notes) {
        lastSample = std::max(lastSample, note.endSample);
    }
    const QVector<float> chroma = chromaForRange(notes, 0, lastSample + 1);
    if (chroma.isEmpty()) {
        return {};
    }
    return KeyAnalyzer::detectKeyFromChroma(chroma).keyName;
}

KeyAnalyzer::PerBarKeyResult analyzeKeyPerBar(const QVector<PitchDetector::PitchNote>& notes,
                                              const KeyAnalyzer::BarGrid& grid,
                                              int sampleRate)
{
    if (notes.isEmpty() || sampleRate <= 0) {
        return {};
    }
    const double samplesPerBar = KeyAnalyzer::samplesPerBar(grid, sampleRate);
    if (samplesPerBar < 1.0) {
        return {};
    }

    const qint64 gridStart = std::max<qint64>(0, grid.gridStartSample);
    qint64 firstSample = notes.first().startSample;
    qint64 lastSample = notes.first().endSample;
    for (const PitchDetector::PitchNote& note : notes) {
        firstSample = std::min(firstSample, note.startSample);
        lastSample = std::max(lastSample, note.endSample);
    }
    if (lastSample <= gridStart) {
        return {};  // весь референс лежит левее тактовой сетки
    }

    // Разбираем только такты со звучанием: от такта первой ноты до последней
    const auto barOf = [&](qint64 sample) {
        return std::max<qint64>(0, qint64(std::floor(double(sample - gridStart) / samplesPerBar)));
    };
    const qint64 firstBar = barOf(firstSample);
    const qint64 lastBar = barOf(std::max(firstSample, lastSample - 1));

    QVector<KeyAnalyzer::BarKey> bars;
    bars.reserve(int(lastBar - firstBar) + 1);
    for (qint64 barIndex = firstBar; barIndex <= lastBar; ++barIndex) {
        KeyAnalyzer::BarKey bar;
        bar.barIndex = int(barIndex);
        bar.startSample = gridStart + qint64(std::llround(double(barIndex) * samplesPerBar));
        bar.endSample = gridStart + qint64(std::llround(double(barIndex + 1) * samplesPerBar));

        const QVector<float> chroma = chromaForRange(notes, bar.startSample, bar.endSample);
        if (chroma.isEmpty()) {
            if (bars.isEmpty()) {
                continue;  // тишина до первой ноты — такта в разборе нет
            }
            bar.key = bars.last().key;  // пауза внутри — тональность держится
        } else {
            bar.key = KeyAnalyzer::detectKeyFromChroma(chroma);
        }
        bars.append(bar);
    }

    return KeyAnalyzer::summarizeBarKeys(bars);
}

} // namespace MidiImporter
