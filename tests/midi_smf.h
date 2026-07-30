#ifndef MIDI_SMF_H
#define MIDI_SMF_H

/**
 * Минимальный разбор SMF для тестовых фикстур tests/midi/*.mid.
 * Дедуплицирует unison (два note-on на одном тике/высоте).
 */

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QIODevice>
#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <cmath>

namespace MidiSmf {

struct Note {
    int startTick = 0;
    int endTick = 0;
    int pitch = 60;   // MIDI
    int velocity = 64;
    int channel = 0;
};

struct Song {
    int format = 1;
    int trackCount = 0;
    int ticksPerQuarter = 480;
    float bpm = 120.0f;
    QVector<Note> notes; // уникальные по (startTick, pitch)
};

inline int readVlq(const QByteArray& data, int& i)
{
    int v = 0;
    while (i < data.size()) {
        const unsigned char c = static_cast<unsigned char>(data.at(i++));
        v = (v << 7) | (c & 0x7f);
        if ((c & 0x80) == 0) {
            break;
        }
    }
    return v;
}

inline Song loadFile(const QString& path)
{
    Song song;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return song;
    }
    const QByteArray data = f.readAll();
    if (data.size() < 14 || !data.startsWith("MThd")) {
        return song;
    }

    const auto u16 = [&](int off) -> int {
        return (static_cast<unsigned char>(data.at(off)) << 8)
            | static_cast<unsigned char>(data.at(off + 1));
    };
    const auto u32 = [&](int off) -> int {
        return (static_cast<unsigned char>(data.at(off)) << 24)
            | (static_cast<unsigned char>(data.at(off + 1)) << 16)
            | (static_cast<unsigned char>(data.at(off + 2)) << 8)
            | static_cast<unsigned char>(data.at(off + 3));
    };

    song.format = u16(8);
    song.trackCount = u16(10);
    song.ticksPerQuarter = u16(12);

    struct RawEvent {
        int tick = 0;
        bool on = false;
        int pitch = 0;
        int velocity = 0;
        int channel = 0;
    };
    QVector<RawEvent> raw;
    bool bpmFound = false;

    int i = 14;
    for (int tr = 0; tr < song.trackCount && i + 8 <= data.size(); ++tr) {
        if (data.mid(i, 4) != "MTrk") {
            break;
        }
        i += 4;
        const int tlen = u32(i);
        i += 4;
        const int end = i + tlen;
        int tick = 0;
        int running = -1;
        while (i < end && i < data.size()) {
            tick += readVlq(data, i);
            if (i >= data.size()) {
                break;
            }
            unsigned char st = static_cast<unsigned char>(data.at(i));
            int status = st;
            if (st < 0x80) {
                status = running;
            } else {
                ++i;
                running = (status < 0xf0) ? status : -1;
            }
            if (status == 0xff) {
                if (i >= data.size()) {
                    break;
                }
                const unsigned char meta = static_cast<unsigned char>(data.at(i++));
                const int ln = readVlq(data, i);
                if (meta == 0x51 && ln == 3 && i + 3 <= data.size() && !bpmFound) {
                    const int us = (static_cast<unsigned char>(data.at(i)) << 16)
                        | (static_cast<unsigned char>(data.at(i + 1)) << 8)
                        | static_cast<unsigned char>(data.at(i + 2));
                    if (us > 0) {
                        song.bpm = float(60'000'000.0 / double(us));
                        bpmFound = true;
                    }
                }
                i += ln;
                if (meta == 0x2f) {
                    break;
                }
            } else if (status == 0xf0 || status == 0xf7) {
                const int ln = readVlq(data, i);
                i += ln;
            } else {
                const int et = status & 0xf0;
                const int ch = status & 0x0f;
                if (et == 0x80 || et == 0x90 || et == 0xa0 || et == 0xb0 || et == 0xe0) {
                    if (i + 1 >= data.size()) {
                        break;
                    }
                    const int a = static_cast<unsigned char>(data.at(i++));
                    const int b = static_cast<unsigned char>(data.at(i++));
                    if (et == 0x90) {
                        RawEvent e;
                        e.tick = tick;
                        e.on = (b != 0);
                        e.pitch = a;
                        e.velocity = b;
                        e.channel = ch;
                        raw.push_back(e);
                    } else if (et == 0x80) {
                        RawEvent e;
                        e.tick = tick;
                        e.on = false;
                        e.pitch = a;
                        e.velocity = b;
                        e.channel = ch;
                        raw.push_back(e);
                    }
                } else if (et == 0xc0 || et == 0xd0) {
                    ++i;
                }
            }
        }
        i = end;
    }

    // Pair note on/off per pitch (FIFO).
    QHash<int, QVector<QPair<int, int>>> open; // pitch -> [(tick, vel)]
    QVector<Note> paired;
    for (const RawEvent& e : raw) {
        if (e.on) {
            open[e.pitch].push_back(qMakePair(e.tick, e.velocity));
        } else if (open.contains(e.pitch) && !open[e.pitch].isEmpty()) {
            const auto st = open[e.pitch].takeFirst();
            Note n;
            n.startTick = st.first;
            n.endTick = e.tick;
            n.pitch = e.pitch;
            n.velocity = st.second;
            n.channel = e.channel;
            if (n.endTick > n.startTick) {
                paired.push_back(n);
            }
        }
    }

    // Deduplicate unison duplicates.
    QSet<QString> seen;
    for (const Note& n : paired) {
        const QString key = QStringLiteral("%1:%2").arg(n.startTick).arg(n.pitch);
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        song.notes.push_back(n);
    }
    return song;
}

inline double tickToSeconds(const Song& song, int tick)
{
    if (song.ticksPerQuarter <= 0 || song.bpm <= 0.0f) {
        return 0.0;
    }
    return double(tick) * (60.0 / double(song.bpm)) / double(song.ticksPerQuarter);
}

inline qint64 tickToSample(const Song& song, int tick, int sampleRate)
{
    return qint64(std::llround(tickToSeconds(song, tick) * double(sampleRate)));
}

} // namespace MidiSmf

#endif // MIDI_SMF_H
