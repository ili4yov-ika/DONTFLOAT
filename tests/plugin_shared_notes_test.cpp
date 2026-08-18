// Общая доска нот: ноты одного экземпляра плагина становятся референсом для
// соседних. Проверяем, что свои ноты не возвращаются, побеждает последняя
// публикация, а уход экземпляра убирает его ноты с доски.

#include <QtTest/QTest>

#include "../plugins/core/dontfloat_shared_notes.h"

using Dontfloat::PluginCore::SharedNoteBoard;
using Dontfloat::PluginCore::TrackPitchNote;

namespace {

/// QString::fromStdString в отладочной сборке идёт в release-Qt6Core.dll и
/// возвращает пустую строку — переводим через UTF-8 сами (как в редакторе).
QString toQString(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::vector<TrackPitchNote> makeNotes(int count, float firstPitch)
{
    std::vector<TrackPitchNote> notes;
    notes.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        TrackPitchNote note;
        note.startSample = std::int64_t(i) * 44100;
        note.endSample = note.startSample + 44100;
        note.midiPitch = firstPitch + float(i);
        note.detectedPitch = note.midiPitch;
        note.confidence = 1.0f;
        notes.push_back(note);
    }
    return notes;
}

} // namespace

class PluginSharedNotesTest : public QObject
{
    Q_OBJECT

private slots:
    void init() { SharedNoteBoard::resetForTests(); }

    void testNotesReachNeighbourButNotSelf();
    void testNewestPublicationWins();
    void testLeavingInstanceTakesItsNotesAway();
    void testEmptyPublicationClearsEntry();
    void testRepeatedPublicationBumpsRevision();
};

// Ноты видит сосед, а сам издатель — нет
void PluginSharedNotesTest::testNotesReachNeighbourButNotSelf()
{
    const auto first = SharedNoteBoard::registerInstance();
    const auto second = SharedNoteBoard::registerInstance();
    QVERIFY(first != 0);
    QVERIFY(first != second);

    SharedNoteBoard::publish(first, "DONTFLOAT Pitcher", 48000, makeNotes(3, 60.0f));

    // Себе свои же ноты референсом не показываем
    QCOMPARE(SharedNoteBoard::latestStamp(first).publisherId, quint64(0));
    QVERIFY(SharedNoteBoard::latestFrom(first).empty());

    const auto stamp = SharedNoteBoard::latestStamp(second);
    QCOMPARE(stamp.publisherId, first);
    QVERIFY(stamp.revision > 0);

    const auto shared = SharedNoteBoard::latestFrom(second);
    QCOMPARE(shared.notes.size(), std::size_t(3));
    QCOMPARE(shared.sampleRate, 48000);
    QCOMPARE(toQString(shared.publisherName), QStringLiteral("DONTFLOAT Pitcher"));
    QCOMPARE(shared.notes[1].midiPitch, 61.0f);
}

// На доске несколько дорожек — референсом идёт последняя публикация
void PluginSharedNotesTest::testNewestPublicationWins()
{
    const auto listener = SharedNoteBoard::registerInstance();
    const auto older = SharedNoteBoard::registerInstance();
    const auto newer = SharedNoteBoard::registerInstance();

    SharedNoteBoard::publish(older, "DONTFLOAT", 44100, makeNotes(2, 48.0f));
    SharedNoteBoard::publish(newer, "DONTFLOAT Pitcher", 44100, makeNotes(4, 72.0f));

    const auto shared = SharedNoteBoard::latestFrom(listener);
    QCOMPARE(shared.stamp.publisherId, newer);
    QCOMPARE(shared.notes.size(), std::size_t(4));

    // Старый экземпляр обновился — теперь референс его
    SharedNoteBoard::publish(older, "DONTFLOAT", 44100, makeNotes(5, 48.0f));
    QCOMPARE(SharedNoteBoard::latestFrom(listener).stamp.publisherId, older);
}

// Плагин сняли с дорожки — его ноты уходят с доски
void PluginSharedNotesTest::testLeavingInstanceTakesItsNotesAway()
{
    const auto listener = SharedNoteBoard::registerInstance();
    const auto publisher = SharedNoteBoard::registerInstance();
    SharedNoteBoard::publish(publisher, "DONTFLOAT", 44100, makeNotes(2, 60.0f));
    QVERIFY(!SharedNoteBoard::latestFrom(listener).empty());

    SharedNoteBoard::unregisterInstance(publisher);
    QVERIFY(SharedNoteBoard::latestFrom(listener).empty());
    QCOMPARE(SharedNoteBoard::latestStamp(listener).revision, quint64(0));
}

// Экземпляр остался без нот (новый файл, сброс анализа) — референс снимается
void PluginSharedNotesTest::testEmptyPublicationClearsEntry()
{
    const auto listener = SharedNoteBoard::registerInstance();
    const auto publisher = SharedNoteBoard::registerInstance();
    SharedNoteBoard::publish(publisher, "DONTFLOAT", 44100, makeNotes(2, 60.0f));
    SharedNoteBoard::publish(publisher, "DONTFLOAT", 44100, {});

    QVERIFY(SharedNoteBoard::latestFrom(listener).empty());
}

// Каждая публикация повышает отметку — по ней получатель понимает, что менять
void PluginSharedNotesTest::testRepeatedPublicationBumpsRevision()
{
    const auto listener = SharedNoteBoard::registerInstance();
    const auto publisher = SharedNoteBoard::registerInstance();

    SharedNoteBoard::publish(publisher, "DONTFLOAT", 44100, makeNotes(2, 60.0f));
    const auto firstRevision = SharedNoteBoard::latestStamp(listener).revision;

    // Те же ноты после правки высоты: содержимое похоже, но отметка новая
    SharedNoteBoard::publish(publisher, "DONTFLOAT", 44100, makeNotes(2, 62.0f));
    const auto secondRevision = SharedNoteBoard::latestStamp(listener).revision;

    QVERIFY(secondRevision > firstRevision);
    QCOMPARE(SharedNoteBoard::latestFrom(listener).notes[0].midiPitch, 62.0f);
}

QTEST_APPLESS_MAIN(PluginSharedNotesTest)
#include "plugin_shared_notes_test.moc"
