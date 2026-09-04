// Мини-DAW с двумя дорожками: один документ ARA, два экземпляра плагина.
//
// Тест играет роль окна мини-DAW: поднимает AraHostDocument на фабрике нашего
// ARA-плагина, кладёт в него две дорожки с разным звуком и сажает на каждую
// свой экземпляр. Проверяется то, ради чего документ сделан общим:
//   * каждый экземпляр читает сэмплы **своей** дорожки (а не соседней);
//   * каждый экземпляр адресует свой аудиоисточник;
//   * ноты соседней дорожки видны как референс — тот самый механизм, который
//     в редакторе рисует серые ноты под своими.

#include <QtTest/QTest>

#include "../plugins/ara/dontfloat_ara_document_controller.h"
#include "../tools/mini_daw/mini_daw_ara_host.h"

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr double kSampleRate = 44100.0;
constexpr double kToneSeconds = 1.5;

using Dontfloat::PluginTester::AraHostDocument;
using Dontfloat::PluginTester::AraHostTrack;

/** Дорожка мини-DAW: пила заданной высоты — детектору нужен богатый спектр. */
AraHostTrack makeTone(float midiPitch, const QString& name)
{
    AraHostTrack track;
    const int frames = int(kToneSeconds * kSampleRate);
    const double frequency = 440.0 * std::pow(2.0, (double(midiPitch) - 69.0) / 12.0);
    track.left.resize(frames);
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / kSampleRate;
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 6; ++harmonic) {
            value += std::sin(2.0 * M_PI * frequency * double(harmonic) * t) / double(harmonic);
        }
        track.left[i] = float(0.5 * value);
    }
    track.sampleRate = kSampleRate;
    track.tempoBpm = 120.0;
    track.beatsPerBar = 4;
    track.name = name;
    return track;
}

/** Средняя высота набора нот — по ней видно, чьи это ноты. */
double averagePitch(const std::vector<Dontfloat::PluginCore::TrackPitchNote>& notes)
{
    if (notes.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const auto& note : notes) {
        sum += double(note.midiPitch);
    }
    return sum / double(notes.size());
}

} // namespace

class MiniDawTwoTracksTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testEachTrackGetsItsOwnAudioAndNotes();
    void testForeignSelectionDoesNotStealTheTrack();
    void testOwnClipIsFoundForEachInstance();
    void testTransportRequestsReachTheHost();
    void testNeighbourNotesAreAvailableAsReference();
    void testRemovedTrackDisappearsFromDocument();

private:
    /** Крутит модель, пока плагин не разберёт обе дорожки. */
    bool waitForAnalysis(int timeoutMs = 20000);

    const ARA::ARAFactory* factory_ = nullptr;
    std::unique_ptr<AraHostDocument> document_;
    /** Экземпляры плагина: по одному на дорожку, как в окне мини-DAW. */
    std::unique_ptr<ARA::PlugIn::PlugInExtension> instances_[2];
    int trackIndexes_[2] = { -1, -1 };
    AraHostTrack tracks_[2];
};

void MiniDawTwoTracksTest::initTestCase()
{
    factory_ = Dontfloat::Ara::AraDocumentController::getARAFactory();
    QVERIFY(factory_ != nullptr);

    tracks_[0] = makeTone(60.0f, QStringLiteral("track A"));   // C4 — дорожка 1
    tracks_[1] = makeTone(72.0f, QStringLiteral("track B"));   // C5 — дорожка 2

    document_ = std::make_unique<AraHostDocument>();
    QString error;
    QVERIFY2(document_->open(factory_, &error), qUtf8Printable(error));

    for (int i = 0; i < 2; ++i) {
        trackIndexes_[i] = document_->addTrack(tracks_[i]);
        QVERIFY(trackIndexes_[i] == i);

        // Привязка экземпляра — то же, что делает обёртка формата в мини-DAW
        instances_[i] = std::make_unique<ARA::PlugIn::PlugInExtension>();
        const ARA::ARAPlugInExtensionInstance* instance = instances_[i]->bindToARA(
            static_cast<ARA::ARADocumentControllerRef>(document_->documentControllerRef()),
            static_cast<ARA::ARAPlugInInstanceRoleFlags>(AraHostDocument::knownRoles()),
            static_cast<ARA::ARAPlugInInstanceRoleFlags>(AraHostDocument::assignedRoles()));
        QVERIFY(instance != nullptr);
        document_->bindInstance(trackIndexes_[i], instance);

        // Окно редактора открыто — как в DAW, где выбор клипов плагину виден
        // только при открытом окне. Без этого шага проверка выбора чужого
        // клипа ничего бы не проверяла
        if (auto* view = instances_[i]->getEditorView<Dontfloat::Ara::AraEditorView>()) {
            view->setEditorOpenState(true);
        }
    }
    QCOMPARE(document_->trackCount(), 2);
    QVERIFY2(waitForAnalysis(), "плагин не разобрал дорожки за отведённое время");
}

void MiniDawTwoTracksTest::cleanupTestCase()
{
    // Порядок разрушения задан ARA: клипы с ролей → экземпляры → документ
    for (int i = 0; i < 2; ++i) {
        document_->unbindInstance(i);
        instances_[i].reset();
    }
    document_.reset();
}

bool MiniDawTwoTracksTest::waitForAnalysis(int timeoutMs)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < timeoutMs) {
        document_->pumpModelUpdates();
        if (document_->readNoteCount(0) > 0 && document_->readNoteCount(1) > 0) {
            return true;
        }
        QTest::qWait(20);
    }
    return false;
}

// Каждая дорожка получила свой звук: хост читает сэмплы по источнику, а не
// «первую попавшуюся» дорожку документа
void MiniDawTwoTracksTest::testEachTrackGetsItsOwnAudioAndNotes()
{
    auto* controller = instances_[0]->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    QVERIFY(controller != nullptr);
    QCOMPARE(instances_[1]->getDocumentController<Dontfloat::Ara::AraDocumentController>(),
             controller);  // документ у дорожек один

    Dontfloat::Ara::AraAudioSource* first =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[0]);
    Dontfloat::Ara::AraAudioSource* second =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[1]);
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY2(first != second, "оба экземпляра сели на один источник");

    // Звук у источников — именно тот, что положили в его дорожку
    for (int i = 0; i < 2; ++i) {
        Dontfloat::Ara::AraAudioSource* source = i == 0 ? first : second;
        QCOMPARE(int(source->monoSamples().size()), tracks_[i].left.size());
        double maxDelta = 0.0;
        for (int frame = 0; frame < tracks_[i].left.size(); frame += 97) {
            maxDelta = std::max<double>(
                maxDelta, std::fabs(double(source->monoSamples()[std::size_t(frame)])
                                    - double(tracks_[i].left[frame])));
        }
        QVERIFY2(maxDelta < 1e-5, "источник дорожки читает чужие сэмплы");
        QVERIFY(source->hasNotes());
    }

    // Разный звук — разные ноты: C4 внизу, C5 наверху
    QVERIFY(averagePitch(second->noteSet().notes) > averagePitch(first->noteSet().notes) + 6.0);
}

// Выделение чужого клипа в DAW не уводит экземпляр с его дорожки.
//
// Это ровно та жалоба, с которой всё началось: DONTFLOAT на первой дорожке
// показывал ноты второй. Выбор клипов в DAW общий на проект, и раньше он
// стоял в поиске источника **первым** — экземпляр уходил на выделенный клип
// соседа. Свои клипы экземпляра теперь важнее.
void MiniDawTwoTracksTest::testForeignSelectionDoesNotStealTheTrack()
{
    Dontfloat::Ara::AraAudioSource* own =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[0]);
    Dontfloat::Ara::AraAudioSource* neighbour =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[1]);
    QVERIFY(own != nullptr);
    QVERIFY(neighbour != nullptr);

    // Человек выделил в DAW клип второй дорожки, окно первой открыто
    document_->selectClipOfTrack(trackIndexes_[0], trackIndexes_[1]);
    document_->pumpModelUpdates();

    QCOMPARE(Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[0]),
             own);

    // Возвращаем выделение на место, чтобы не мешать соседним проверкам
    document_->selectClipOfTrack(trackIndexes_[0], trackIndexes_[0]);
    document_->pumpModelUpdates();
}

// Каждый экземпляр находит **свой** клип: по нему считаются тактовая сетка и
// каретка, и обе половины окна обязаны брать один и тот же
void MiniDawTwoTracksTest::testOwnClipIsFoundForEachInstance()
{
    Dontfloat::Ara::AraClipPlacement first;
    Dontfloat::Ara::AraClipPlacement second;
    QVERIFY(Dontfloat::Ara::AraDocumentController::clipForInstance(*instances_[0], &first));
    QVERIFY(Dontfloat::Ara::AraDocumentController::clipForInstance(*instances_[1], &second));

    // Клип покрывает всю дорожку и не растянут — так его положил мини-DAW
    for (const Dontfloat::Ara::AraClipPlacement& clip : { first, second }) {
        QCOMPARE(clip.startInPlaybackSeconds, 0.0);
        QVERIFY(clip.durationInPlaybackSeconds > 0.0);
        QVERIFY(std::fabs(clip.stretchFactor() - 1.0) < 1e-6);
    }
}

// Кнопки воспроизведения плагина — дублёры кнопок DAW: нажатие обязано
// дойти до хоста, своего проигрывателя у плагина под ARA нет
void MiniDawTwoTracksTest::testTransportRequestsReachTheHost()
{
    auto* controller =
        instances_[0]->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    QVERIFY(controller != nullptr);

    const int startsBefore = document_->transportStartRequests();
    const int stopsBefore = document_->transportStopRequests();

    QVERIFY2(controller->requestHostPlayback(true),
             "хост не отдал плагину управление транспортом");
    QCOMPARE(document_->transportStartRequests(), startsBefore + 1);

    QVERIFY(controller->requestHostPlayback(false));
    QCOMPARE(document_->transportStopRequests(), stopsBefore + 1);

    // Клик по волне или пианороллу переставляет каретку DAW тем же путём
    QVERIFY(controller->requestHostPlaybackPosition(1.25));
    QVERIFY(std::fabs(document_->lastRequestedPlaybackPosition() - 1.25) < 1e-9);
}

// Ноты соседней дорожки доступны как референс — это и рисует редактор серым
void MiniDawTwoTracksTest::testNeighbourNotesAreAvailableAsReference()
{
    auto* controller = instances_[0]->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    QVERIFY(controller != nullptr);

    Dontfloat::Ara::AraAudioSource* first =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[0]);
    Dontfloat::Ara::AraAudioSource* second =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[1]);
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);

    const Dontfloat::Ara::AraNoteSet referenceForFirst = controller->referenceNotesExcluding(first);
    const Dontfloat::Ara::AraNoteSet referenceForSecond =
        controller->referenceNotesExcluding(second);
    QVERIFY2(referenceForFirst.valid && !referenceForFirst.notes.empty(),
             "первая дорожка не получила референс со второй");
    QVERIFY2(referenceForSecond.valid && !referenceForSecond.notes.empty(),
             "вторая дорожка не получила референс с первой");

    // Референс — чужие ноты, а не свои: сверху C5, снизу C4
    QVERIFY2(averagePitch(referenceForFirst.notes)
                 > averagePitch(first->noteSet().notes) + 6.0,
             "первой дорожке подсунули её же ноты вместо соседних");
    QVERIFY2(averagePitch(referenceForSecond.notes)
                 < averagePitch(second->noteSet().notes) - 6.0,
             "второй дорожке подсунули её же ноты вместо соседних");
}

// Дорожку убрали из документа — её источник ушёл, соседняя жива
void MiniDawTwoTracksTest::testRemovedTrackDisappearsFromDocument()
{
    auto* controller = instances_[0]->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    QVERIFY(controller != nullptr);
    QCOMPARE(int(controller->getDocument()->getAudioSources().size()), 2);

    // Порядок как в окне мини-DAW (releaseAraTrack → unload): сначала дорожка
    // уходит из документа, пока экземпляр жив и может отпустить клип, и только
    // потом сам экземпляр
    document_->removeTrack(trackIndexes_[1]);
    trackIndexes_[1] = -1;
    instances_[1].reset();

    QCOMPARE(int(controller->getDocument()->getAudioSources().size()), 1);
    QCOMPARE(document_->readNoteCount(1), 0);
    QVERIFY2(document_->readNoteCount(0) > 0, "первая дорожка пострадала от удаления второй");

    // Референса больше нет: соседей в документе не осталось
    Dontfloat::Ara::AraAudioSource* first =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*instances_[0]);
    QVERIFY(first != nullptr);
    const Dontfloat::Ara::AraNoteSet reference = controller->referenceNotesExcluding(first);
    QVERIFY(reference.notes.empty());
}

QTEST_MAIN(MiniDawTwoTracksTest)
#include "mini_daw_two_tracks_test.moc"
