#include "../include/midiexporter.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QObject>

#include <algorithm>
#include <cmath>

namespace MidiExporter {
namespace {

void appendUInt16(QByteArray& out, quint16 value)
{
    out.append(char((value >> 8) & 0xFF));
    out.append(char(value & 0xFF));
}

void appendUInt32(QByteArray& out, quint32 value)
{
    out.append(char((value >> 24) & 0xFF));
    out.append(char((value >> 16) & 0xFF));
    out.append(char((value >> 8) & 0xFF));
    out.append(char(value & 0xFF));
}

/** Дельта-время в MIDI пишется переменной длиной (7 бит на байт). */
void appendVlq(QByteArray& out, quint32 value)
{
    quint32 buffer = value & 0x7F;
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    for (;;) {
        out.append(char(buffer & 0xFF));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

struct MidiEvent {
    quint32 tick = 0;
    bool noteOn = false;
    int pitch = 60;
    int velocity = 96;
};

int clampPitch(float midiPitch)
{
    const int rounded = int(std::lround(midiPitch));
    return std::clamp(rounded, 0, 127);
}

} // namespace

QByteArray buildFile(const QVector<PitchDetector::PitchNote>& notes, const Options& options)
{
    const float bpm = options.bpm > 0.0f ? options.bpm : 120.0f;
    const int sampleRate = options.sampleRate > 0 ? options.sampleRate : 44100;
    const int velocity = std::clamp(options.velocity, 1, 127);
    const int channel = std::clamp(options.channel, 0, 15);

    // Сэмплы → тики: сколько сэмплов в четверти, столько же и в kTicksPerQuarter
    const double samplesPerQuarter = (60.0 / double(bpm)) * double(sampleRate);
    const auto toTicks = [&](qint64 sample) {
        const double relative = double(sample - options.startSample);
        const double ticks = (relative / samplesPerQuarter) * double(kTicksPerQuarter);
        return quint32(std::max<long long>(0, std::llround(ticks)));
    };

    QVector<MidiEvent> events;
    events.reserve(notes.size() * 2);
    for (const PitchDetector::PitchNote& note : notes) {
        if (note.endSample <= note.startSample) {
            continue;
        }
        const quint32 startTick = toTicks(note.startSample);
        // Нота не должна схлопываться в ноль после округления
        const quint32 endTick = std::max(startTick + 1, toTicks(note.endSample));
        const int pitch = clampPitch(note.midiPitch);
        events.append({ startTick, true, pitch, velocity });
        events.append({ endTick, false, pitch, 0 });
    }

    // Порядок: по тику, при равенстве сначала note-off (иначе повтор той же
    // высоты гасится собственным выключением)
    std::stable_sort(events.begin(), events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        if (a.tick != b.tick) {
            return a.tick < b.tick;
        }
        return int(a.noteOn) < int(b.noteOn);
    });

    QByteArray track;
    // Темп: микросекунды на четверть
    const quint32 microsecondsPerQuarter = quint32(std::llround(60000000.0 / double(bpm)));
    appendVlq(track, 0);
    track.append(char(0xFF));
    track.append(char(0x51));
    track.append(char(0x03));
    track.append(char((microsecondsPerQuarter >> 16) & 0xFF));
    track.append(char((microsecondsPerQuarter >> 8) & 0xFF));
    track.append(char(microsecondsPerQuarter & 0xFF));

    quint32 previousTick = 0;
    for (const MidiEvent& event : events) {
        appendVlq(track, event.tick - previousTick);
        previousTick = event.tick;
        track.append(char((event.noteOn ? 0x90 : 0x80) | channel));
        track.append(char(event.pitch & 0x7F));
        track.append(char(event.velocity & 0x7F));
    }

    // Конец дорожки
    appendVlq(track, 0);
    track.append(char(0xFF));
    track.append(char(0x2F));
    track.append(char(0x00));

    QByteArray file;
    file.append("MThd", 4);
    appendUInt32(file, 6);
    appendUInt16(file, 0);  // формат 0 — одна дорожка
    appendUInt16(file, 1);
    appendUInt16(file, quint16(kTicksPerQuarter));
    file.append("MTrk", 4);
    appendUInt32(file, quint32(track.size()));
    file.append(track);
    return file;
}

bool writeFile(const QString& path,
               const QVector<PitchDetector::PitchNote>& notes,
               const Options& options,
               QString* error)
{
    if (notes.isEmpty()) {
        if (error) {
            *error = QObject::tr("There are no notes to export");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    const QByteArray data = buildFile(notes, options);
    if (file.write(data) != data.size()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.close();
    return true;
}

} // namespace MidiExporter
