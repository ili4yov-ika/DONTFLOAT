// ARA 2: плагин как ARA-плагин глазами хоста.
//
// Тест играет роль DAW: поднимает фабрику ARA нашего плагина, заводит документ
// с двумя аудиоисточниками (двумя «дорожками»), даёт плагину произвольный
// доступ к их сэмплам, дожидается разбора и читает ноты обратно через
// контент-ридер ARA — как это делает настоящий хост. Заодно проверяется
// механизм, на котором построен референс с соседней дорожки: один document
// controller видит все источники документа.

#include <QtTest/QTest>

#include "../plugins/ara/dontfloat_ara_document_controller.h"

#include "../include/midiimporter.h"

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr double kSampleRate = 44100.0;
constexpr double kToneSeconds = 1.5;

/** Дорожка хоста: синус заданной высоты, как аудиофайл на дорожке DAW. */
struct HostAudioTrack {
    std::vector<float> samples;
    double sampleRate = kSampleRate;
    int channelCount = 1;

    static HostAudioTrack makeTone(float midiPitch, double seconds = kToneSeconds)
    {
        HostAudioTrack track;
        const auto frames = static_cast<std::size_t>(seconds * kSampleRate);
        const double frequency = 440.0 * std::pow(2.0, (double(midiPitch) - 69.0) / 12.0);
        track.samples.resize(frames, 0.0f);
        for (std::size_t i = 0; i < frames; ++i) {
            const double t = double(i) / kSampleRate;
            // Пила из нескольких гармоник: детектору нот нужен богатый спектр
            double value = 0.0;
            for (int harmonic = 1; harmonic <= 6; ++harmonic) {
                value += std::sin(2.0 * M_PI * frequency * double(harmonic) * t) / double(harmonic);
            }
            track.samples[i] = float(0.5 * value);
        }
        return track;
    }
};

// Ссылки хоста на объект дорожки: и как источник, и как читатель сэмплов
ARA_MAP_HOST_REF(HostAudioTrack, ARA::ARAAudioSourceHostRef, ARA::ARAAudioReaderHostRef)

/** Доступ к сэмплам: то, ради чего ARA и существует. */
class TestAudioAccessController : public ARA::Host::AudioAccessControllerInterface {
public:
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef audioSourceHostRef,
                                                          bool /*use64BitSamples*/) noexcept override
    {
        return toHostRef(fromHostRef(audioSourceHostRef));
    }

    bool readAudioSamples(ARA::ARAAudioReaderHostRef audioReaderHostRef,
                          ARA::ARASamplePosition samplePosition,
                          ARA::ARASampleCount samplesPerChannel,
                          void* const buffers[]) noexcept override
    {
        auto* track = fromHostRef(audioReaderHostRef);
        if (!track || !buffers || !buffers[0]) {
            return false;
        }
        auto* out = static_cast<float*>(buffers[0]);
        for (ARA::ARASampleCount i = 0; i < samplesPerChannel; ++i) {
            const auto index = static_cast<std::size_t>(samplePosition + i);
            out[i] = index < track->samples.size() ? track->samples[index] : 0.0f;
        }
        return true;
    }

    void destroyAudioReader(ARA::ARAAudioReaderHostRef /*audioReaderHostRef*/) noexcept override {}
};

/**
 * Архив в памяти. Хосты именно так делают undo/redo для ARA: состояние
 * плагина пишется в архив и позже накатывается обратно.
 */
class TestArchivingController : public ARA::Host::ArchivingControllerInterface {
public:
    ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef) noexcept override
    {
        return bytes.size();
    }
    bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef, ARA::ARASize position,
                              ARA::ARASize length, ARA::ARAByte buffer[]) noexcept override
    {
        if (position + length > bytes.size()) {
            return false;
        }
        std::memcpy(buffer, bytes.data() + position, length);
        return true;
    }
    bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef, ARA::ARASize position,
                             ARA::ARASize length, const ARA::ARAByte buffer[]) noexcept override
    {
        if (bytes.size() < position + length) {
            bytes.resize(position + length, 0);
        }
        std::memcpy(bytes.data() + position, buffer, length);
        return true;
    }
    void notifyDocumentArchivingProgress(float) noexcept override {}
    void notifyDocumentUnarchivingProgress(float) noexcept override {}
    ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef) noexcept override
    {
        // Хост обязан сказать, чей это архив — берём id из фабрики плагина
        return "org.dontfloat.ara.archive.v1";
    }

    std::vector<ARA::ARAByte> bytes;
};

/** Хост своей разметки не отдаёт — плагин разбирает звук сам. */
class TestContentAccessController : public ARA::Host::ContentAccessControllerInterface {
public:
    bool isMusicalContextContentAvailable(ARA::ARAMusicalContextHostRef,
                                          ARA::ARAContentType) noexcept override
    {
        return false;
    }
    ARA::ARAContentGrade getMusicalContextContentGrade(ARA::ARAMusicalContextHostRef,
                                                       ARA::ARAContentType) noexcept override
    {
        return ARA::kARAContentGradeInitial;
    }
    ARA::ARAContentReaderHostRef createMusicalContextContentReader(
        ARA::ARAMusicalContextHostRef, ARA::ARAContentType,
        const ARA::ARAContentTimeRange*) noexcept override
    {
        return nullptr;
    }
    bool isAudioSourceContentAvailable(ARA::ARAAudioSourceHostRef,
                                       ARA::ARAContentType) noexcept override
    {
        return false;
    }
    ARA::ARAContentGrade getAudioSourceContentGrade(ARA::ARAAudioSourceHostRef,
                                                    ARA::ARAContentType) noexcept override
    {
        return ARA::kARAContentGradeInitial;
    }
    ARA::ARAContentReaderHostRef createAudioSourceContentReader(
        ARA::ARAAudioSourceHostRef, ARA::ARAContentType,
        const ARA::ARAContentTimeRange*) noexcept override
    {
        return nullptr;
    }
    ARA::ARAInt32 getContentReaderEventCount(ARA::ARAContentReaderHostRef) noexcept override
    {
        return 0;
    }
    const void* getContentReaderDataForEvent(ARA::ARAContentReaderHostRef,
                                             ARA::ARAInt32) noexcept override
    {
        return nullptr;
    }
    void destroyContentReader(ARA::ARAContentReaderHostRef) noexcept override {}
};

/** Сюда плагин сообщает о ходе разбора и о готовой разметке. */
class TestModelUpdateController : public ARA::Host::ModelUpdateControllerInterface {
public:
    void notifyAudioSourceAnalysisProgress(ARA::ARAAudioSourceHostRef audioSourceHostRef,
                                           ARA::ARAAnalysisProgressState state,
                                           float /*value*/) noexcept override
    {
        if (state == ARA::kARAAnalysisProgressCompleted) {
            ++completedAnalyses[audioSourceHostRef];
        }
    }
    void notifyAudioSourceContentChanged(ARA::ARAAudioSourceHostRef audioSourceHostRef,
                                         const ARA::ARAContentTimeRange*,
                                         ARA::ContentUpdateScopes) noexcept override
    {
        ++contentUpdates[audioSourceHostRef];
    }
    void notifyAudioModificationContentChanged(ARA::ARAAudioModificationHostRef,
                                               const ARA::ARAContentTimeRange*,
                                               ARA::ContentUpdateScopes) noexcept override
    {
    }
    void notifyPlaybackRegionContentChanged(ARA::ARAPlaybackRegionHostRef,
                                            const ARA::ARAContentTimeRange*,
                                            ARA::ContentUpdateScopes) noexcept override
    {
    }

    void notifyDocumentDataChanged() noexcept override {}

    std::map<ARA::ARAAudioSourceHostRef, int> completedAnalyses;
    std::map<ARA::ARAAudioSourceHostRef, int> contentUpdates;
};

/** Транспорт хоста в тесте не участвует. */
class TestPlaybackController : public ARA::Host::PlaybackControllerInterface {
public:
    void requestStartPlayback() noexcept override {}
    void requestStopPlayback() noexcept override {}
    void requestSetPlaybackPosition(ARA::ARATimePosition) noexcept override {}
    void requestSetCycleRange(ARA::ARATimePosition, ARA::ARATimeDuration) noexcept override {}
    void requestEnableCycle(bool) noexcept override {}
};

} // namespace

class AraDocumentControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFactoryDescribesPlugIn();
    void testHostReadsAnalyzedNotes();
    void testOtherTrackNotesAreAvailableAsReference();
    void testNewClipIsAnalyzedWithoutRequest();
    void testClipMoveAndStretchKeepNotes();
    void testArchiveRoundTripRestoresNotes();
    void testReferenceNotesFromNeighbourGiveKeyRegions();
    void testAudioAndProgressAvailableWithoutPlayback();
    void testInstancesOnDifferentTracksExchangeReferenceNotes();

private:
    /** Заводит источник в документе и дожидается разбора. */
    ARA::ARAAudioSourceRef addAnalyzedSource(HostAudioTrack* track, const char* persistentId);
    /** Крутит notifyModelUpdates, пока разбор не завершится. */
    bool waitForAnalysis(ARA::ARAAudioSourceHostRef hostRef, int timeoutMs = 20000);

    const ARA::ARAFactory* factory_ = nullptr;
    /** Хост обязан снести свои объекты раньше document controller. */
    std::vector<ARA::ARAAudioSourceRef> audioSourceRefs_;
    /** Общие для теста контекст и дорожка: на них вешаются клипы. */
    ARA::ARAMusicalContextRef musicalContextRef_ = nullptr;
    ARA::ARARegionSequenceRef regionSequenceRef_ = nullptr;

    /** Сколько нот плагин отдаёт по источнику прямо сейчас. */
    int noteCountOf(ARA::ARAAudioSourceRef sourceRef);
    /** Наш document controller — тесты смотрят модель напрямую. */
    ARA::PlugIn::DocumentController* araDocumentController() const { return araController_; }

    ARA::PlugIn::DocumentController* araController_ = nullptr;
    std::unique_ptr<ARA::Host::DocumentController> documentController_;
    TestAudioAccessController audioAccess_;
    TestArchivingController archiving_;
    TestContentAccessController contentAccess_;
    TestModelUpdateController modelUpdates_;
    TestPlaybackController playback_;
    std::unique_ptr<ARA::Host::DocumentControllerHostInstance> hostInstance_;
};

void AraDocumentControllerTest::initTestCase()
{
    factory_ = Dontfloat::Ara::AraDocumentController::getARAFactory();
    QVERIFY(factory_ != nullptr);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAInterfaceConfiguration, assertFunctionAddress)>
        interfaceConfig { ARA::kARAAPIGeneration_2_0_Final, nullptr };
    factory_->initializeARAWithConfiguration(&interfaceConfig);

    hostInstance_ = std::make_unique<ARA::Host::DocumentControllerHostInstance>(
        &audioAccess_, &archiving_, &contentAccess_, &modelUpdates_, &playback_);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARADocumentProperties, name)> documentProperties {
        "DONTFLOAT ARA test"
    };
    const ARA::ARADocumentControllerInstance* instance =
        factory_->createDocumentControllerWithDocument(hostInstance_.get(),
                                                       &documentProperties);
    QVERIFY(instance != nullptr);
    documentController_ = std::make_unique<ARA::Host::DocumentController>(instance);
    // Ссылка на нашу реализацию: часть проверок смотрит модель изнутри
    araController_ = static_cast<ARA::PlugIn::DocumentController*>(
        ARA::PlugIn::fromRef<ARA::PlugIn::DocumentControllerInterface>(
            instance->documentControllerRef));

    // Контекст и дорожка живут весь тест: клипы вешаются на них
    documentController_->beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAMusicalContextProperties, color)>
        musicalContextProperties { "test timeline", 0, nullptr };
    musicalContextRef_ = documentController_->createMusicalContext(
        reinterpret_cast<ARA::ARAMusicalContextHostRef>(this), &musicalContextProperties);
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARARegionSequenceProperties, color)>
        regionSequenceProperties { "test track", 0, musicalContextRef_, nullptr };
    regionSequenceRef_ = documentController_->createRegionSequence(
        reinterpret_cast<ARA::ARARegionSequenceHostRef>(this), &regionSequenceProperties);
    documentController_->endEditing();
}

int AraDocumentControllerTest::noteCountOf(ARA::ARAAudioSourceRef sourceRef)
{
    if (!documentController_->isAudioSourceContentAvailable(sourceRef,
                                                            ARA::kARAContentTypeNotes)) {
        return 0;
    }
    const ARA::ARAContentReaderRef reader = documentController_->createAudioSourceContentReader(
        sourceRef, ARA::kARAContentTypeNotes, nullptr);
    if (!reader) {
        return 0;
    }
    const int count = int(documentController_->getContentReaderEventCount(reader));
    documentController_->destroyContentReader(reader);
    return count;
}

void AraDocumentControllerTest::cleanupTestCase()
{
    if (documentController_) {
        // Порядок разрушения задан ARA: сначала объекты модели, потом контроллер
        for (const ARA::ARAAudioSourceRef sourceRef : audioSourceRefs_) {
            documentController_->enableAudioSourceSamplesAccess(sourceRef, false);
        }
        documentController_->beginEditing();
        for (const ARA::ARAAudioSourceRef sourceRef : audioSourceRefs_) {
            documentController_->destroyAudioSource(sourceRef);
        }
        documentController_->endEditing();
        audioSourceRefs_.clear();

        documentController_->beginEditing();
        if (regionSequenceRef_) {
            documentController_->destroyRegionSequence(regionSequenceRef_);
            regionSequenceRef_ = nullptr;
        }
        if (musicalContextRef_) {
            documentController_->destroyMusicalContext(musicalContextRef_);
            musicalContextRef_ = nullptr;
        }
        documentController_->endEditing();

        documentController_->destroyDocumentController();
        documentController_.reset();
    }
    if (factory_) {
        factory_->uninitializeARA();
    }
}

ARA::ARAAudioSourceRef AraDocumentControllerTest::addAnalyzedSource(HostAudioTrack* track,
                                                                   const char* persistentId)
{
    const ARA::ARAAudioSourceHostRef hostRef = toHostRef(track);

    // Ссылка хоста — адрес дорожки на стеке теста, и у дорожек из разных
    // проверок он совпадает. Старый счётчик разборов по этому адресу заставлял
    // waitForAnalysis вернуться сразу, до того как разбор действительно прошёл:
    // ноты у источника ещё пусты, а тест уже идёт дальше
    modelUpdates_.completedAnalyses.erase(hostRef);

    documentController_->beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioSourceProperties, merits64BitSamples)>
        sourceProperties {
            "track",
            persistentId,
            static_cast<ARA::ARASampleCount>(track->samples.size()),
            track->sampleRate,
            track->channelCount,
            ARA::kARAFalse,
        };
    const ARA::ARAAudioSourceRef sourceRef =
        documentController_->createAudioSource(hostRef, &sourceProperties);
    documentController_->endEditing();

    // Доступ к сэмплам включает хост — с этого момента плагин может читать файл
    documentController_->enableAudioSourceSamplesAccess(sourceRef, true);
    documentController_->requestAudioSourceContentAnalysis(sourceRef, 1,
                                                           &Dontfloat::Ara::kNoteContentType);
    audioSourceRefs_.push_back(sourceRef);
    return sourceRef;
}

bool AraDocumentControllerTest::waitForAnalysis(ARA::ARAAudioSourceHostRef hostRef, int timeoutMs)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < timeoutMs) {
        documentController_->notifyModelUpdates();
        if (modelUpdates_.completedAnalyses[hostRef] > 0) {
            return true;
        }
        QTest::qWait(20);
    }
    return false;
}

// Фабрика описывает плагин так, чтобы хост его принял
void AraDocumentControllerTest::testFactoryDescribesPlugIn()
{
    QCOMPARE(QString::fromUtf8(factory_->factoryID), QStringLiteral("org.dontfloat.ara.factory"));
    QVERIFY(factory_->highestSupportedApiGeneration >= ARA::kARAAPIGeneration_2_0_Final);
    QCOMPARE(factory_->analyzeableContentTypesCount, ARA::ARASize(1));
    QCOMPARE(factory_->analyzeableContentTypes[0], ARA::kARAContentTypeNotes);
    QVERIFY(factory_->documentArchiveID != nullptr);
}

// Хост даёт доступ к сэмплам, плагин разбирает и отдаёт ноты обратно
void AraDocumentControllerTest::testHostReadsAnalyzedNotes()
{
    HostAudioTrack track = HostAudioTrack::makeTone(69.0f);  // A4
    const ARA::ARAAudioSourceRef sourceRef = addAnalyzedSource(&track, "source-a4");
    QVERIFY2(waitForAnalysis(toHostRef(&track)), "плагин не сообщил о завершении разбора");

    QVERIFY(documentController_->isAudioSourceContentAvailable(sourceRef,
                                                              ARA::kARAContentTypeNotes));

    const ARA::ARAContentReaderRef reader =
        documentController_->createAudioSourceContentReader(sourceRef, ARA::kARAContentTypeNotes,
                                                            nullptr);
    QVERIFY(reader != nullptr);
    const ARA::ARAInt32 eventCount = documentController_->getContentReaderEventCount(reader);
    QVERIFY2(eventCount > 0, "хост не получил ни одной ноты");

    const auto* note = static_cast<const ARA::ARAContentNote*>(
        documentController_->getContentReaderDataForEvent(reader, 0));
    QVERIFY(note != nullptr);
    // Тон A4: допускаем полутон в обе стороны
    QVERIFY2(std::abs(note->pitchNumber - 69) <= 1,
             qPrintable(QStringLiteral("pitchNumber=%1").arg(note->pitchNumber)));
    QVERIFY(note->noteDuration > 0.0);
    documentController_->destroyContentReader(reader);
}

// Второй источник в том же документе виден как референс для первого
void AraDocumentControllerTest::testOtherTrackNotesAreAvailableAsReference()
{
    HostAudioTrack first = HostAudioTrack::makeTone(60.0f);   // C4 — «дорожка 1»
    HostAudioTrack second = HostAudioTrack::makeTone(67.0f);  // G4 — «дорожка 2»

    const ARA::ARAAudioSourceRef firstRef = addAnalyzedSource(&first, "source-c4");
    QVERIFY(waitForAnalysis(toHostRef(&first)));
    const ARA::ARAAudioSourceRef secondRef = addAnalyzedSource(&second, "source-g4");
    QVERIFY(waitForAnalysis(toHostRef(&second)));

    // Обе дорожки разобраны — их ноты доступны через один document controller
    QVERIFY(documentController_->isAudioSourceContentAvailable(firstRef, ARA::kARAContentTypeNotes));
    QVERIFY(documentController_->isAudioSourceContentAvailable(secondRef, ARA::kARAContentTypeNotes));

    const ARA::ARAContentReaderRef firstReader =
        documentController_->createAudioSourceContentReader(firstRef, ARA::kARAContentTypeNotes,
                                                            nullptr);
    const ARA::ARAContentReaderRef secondReader =
        documentController_->createAudioSourceContentReader(secondRef, ARA::kARAContentTypeNotes,
                                                            nullptr);
    QVERIFY(documentController_->getContentReaderEventCount(firstReader) > 0);
    QVERIFY(documentController_->getContentReaderEventCount(secondReader) > 0);

    const auto* firstNote = static_cast<const ARA::ARAContentNote*>(
        documentController_->getContentReaderDataForEvent(firstReader, 0));
    const auto* secondNote = static_cast<const ARA::ARAContentNote*>(
        documentController_->getContentReaderDataForEvent(secondReader, 0));
    QVERIFY(firstNote != nullptr);
    QVERIFY(secondNote != nullptr);
    // Дорожки звучат по-разному — значит это действительно разные источники
    QVERIFY2(firstNote->pitchNumber != secondNote->pitchNumber,
             "ноты соседних дорожек совпали — источники перепутаны");

    documentController_->destroyContentReader(firstReader);
    documentController_->destroyContentReader(secondReader);
}

// Новый клип на дорожке разбирается сам, без просьбы хоста
void AraDocumentControllerTest::testNewClipIsAnalyzedWithoutRequest()
{
    HostAudioTrack clip = HostAudioTrack::makeTone(64.0f);  // E4 — «новый клип»
    const auto hostRef = toHostRef(&clip);

    documentController_->beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioSourceProperties, merits64BitSamples)>
        sourceProperties {
            "new clip",
            "source-new-clip",
            static_cast<ARA::ARASampleCount>(clip.samples.size()),
            clip.sampleRate,
            clip.channelCount,
            ARA::kARAFalse,
        };
    const ARA::ARAAudioSourceRef sourceRef =
        documentController_->createAudioSource(hostRef, &sourceProperties);
    documentController_->endEditing();
    audioSourceRefs_.push_back(sourceRef);

    // Разбор не заказываем: плагин должен взяться за клип сам, как только
    // хост открыл доступ к сэмплам
    documentController_->enableAudioSourceSamplesAccess(sourceRef, true);
    QVERIFY2(waitForAnalysis(hostRef), "новый клип не разобрался без явного запроса");
    QVERIFY(documentController_->isAudioSourceContentAvailable(sourceRef,
                                                              ARA::kARAContentTypeNotes));
}

// Перенос и растяжение клипа не роняют разметку и не запускают разбор заново
void AraDocumentControllerTest::testClipMoveAndStretchKeepNotes()
{
    HostAudioTrack track = HostAudioTrack::makeTone(62.0f);  // D4
    const auto hostRef = toHostRef(&track);
    const ARA::ARAAudioSourceRef sourceRef = addAnalyzedSource(&track, "source-clip-edits");
    QVERIFY(waitForAnalysis(hostRef));

    const double duration = double(track.samples.size()) / track.sampleRate;

    documentController_->beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioModificationProperties, persistentID)>
        modificationProperties { "clip", "modification-clip-edits" };
    const ARA::ARAAudioModificationRef modificationRef =
        documentController_->createAudioModification(
            sourceRef, reinterpret_cast<ARA::ARAAudioModificationHostRef>(&track),
            &modificationProperties);
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAPlaybackRegionProperties, color)>
        regionProperties {
            ARA::kARAPlaybackTransformationNoChanges,
            0.0, duration, 0.0, duration,
            musicalContextRef_, regionSequenceRef_, "clip", nullptr,
        };
    const ARA::ARAPlaybackRegionRef regionRef = documentController_->createPlaybackRegion(
        modificationRef, reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&track),
        &regionProperties);
    documentController_->endEditing();

    const int notesBefore = noteCountOf(sourceRef);
    QVERIFY(notesBefore > 0);

    // Разрез + перенос + растяжение разом: клип встал на 4-ю секунду, играет
    // половину исходника вдвое дольше
    documentController_->beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAPlaybackRegionProperties, color)>
        movedProperties {
            ARA::kARAPlaybackTransformationTimestretch,
            4.0, duration, duration * 0.25, duration * 0.5,
            musicalContextRef_, regionSequenceRef_, "clip", nullptr,
        };
    documentController_->updatePlaybackRegionProperties(regionRef, &movedProperties);
    documentController_->endEditing();
    documentController_->notifyModelUpdates();

    // Ноты никуда не делись: сам звук не менялся, менялось место клипа
    QCOMPARE(noteCountOf(sourceRef), notesBefore);

    // Разметка клипа читается по новой позиции: контент-ридер клипа отдаёт
    // ноты в координатах проекта
    const ARA::ARAContentReaderRef regionReader =
        documentController_->createPlaybackRegionContentReader(
            regionRef, ARA::kARAContentTypeNotes, nullptr);
    if (regionReader) {
        const ARA::ARAInt32 count = documentController_->getContentReaderEventCount(regionReader);
        QVERIFY(count >= 0);
        documentController_->destroyContentReader(regionReader);
    }

    documentController_->beginEditing();
    documentController_->destroyPlaybackRegion(regionRef);
    documentController_->destroyAudioModification(modificationRef);
    documentController_->endEditing();
}

// Undo/redo хоста: состояние уходит в архив и накатывается обратно
void AraDocumentControllerTest::testArchiveRoundTripRestoresNotes()
{
    HostAudioTrack track = HostAudioTrack::makeTone(65.0f);  // F4
    const auto hostRef = toHostRef(&track);
    const ARA::ARAAudioSourceRef sourceRef = addAnalyzedSource(&track, "source-archive");
    QVERIFY(waitForAnalysis(hostRef));
    const int notesBefore = noteCountOf(sourceRef);
    QVERIFY(notesBefore > 0);

    // Сохраняем состояние плагина, как это делает хост перед правкой
    archiving_.bytes.clear();
    const ARA::ARAAudioSourceRef sourcesToStore[] = { sourceRef };
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAStoreObjectsFilter, audioModificationRefs)>
        storeFilter { ARA::kARAFalse, ARA::ARASize { 1 }, sourcesToStore,
                      ARA::ARASize { 0 }, nullptr };
    QVERIFY(documentController_->storeObjectsToArchive(
        reinterpret_cast<ARA::ARAArchiveWriterHostRef>(&archiving_), &storeFilter));
    QVERIFY2(!archiving_.bytes.empty(), "плагин ничего не записал в архив");

    // Накатываем архив обратно — плагин должен принять разметку как свою
    documentController_->beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARARestoreObjectsFilter, audioModificationCurrentIDs)>
        restoreFilter { ARA::kARAFalse, ARA::ARASize { 0 }, nullptr, nullptr,
                        ARA::ARASize { 0 }, nullptr, nullptr };
    QVERIFY(documentController_->restoreObjectsFromArchive(
        reinterpret_cast<ARA::ARAArchiveReaderHostRef>(&archiving_), &restoreFilter));
    documentController_->endEditing();
    documentController_->notifyModelUpdates();

    QCOMPARE(noteCountOf(sourceRef), notesBefore);
}

// Механизм референса целиком: ноты соседа → потактовые тональности,
// как их показывает редактор плагина
void AraDocumentControllerTest::testReferenceNotesFromNeighbourGiveKeyRegions()
{
    HostAudioTrack own = HostAudioTrack::makeTone(60.0f);        // своя дорожка
    HostAudioTrack neighbour = HostAudioTrack::makeTone(67.0f);  // соседняя

    const ARA::ARAAudioSourceRef ownRef = addAnalyzedSource(&own, "source-own");
    QVERIFY(waitForAnalysis(toHostRef(&own)));
    const ARA::ARAAudioSourceRef neighbourRef = addAnalyzedSource(&neighbour, "source-neighbour");
    QVERIFY(waitForAnalysis(toHostRef(&neighbour)));
    Q_UNUSED(ownRef);
    Q_UNUSED(neighbourRef);

    // Тот же вызов, которым редактор берёт референс с соседней дорожки
    auto* controller = static_cast<Dontfloat::Ara::AraDocumentController*>(
        araDocumentController());
    QVERIFY(controller != nullptr);

    Dontfloat::Ara::AraAudioSource* ownSource = nullptr;
    for (ARA::PlugIn::AudioSource* source : controller->getDocument()->getAudioSources()) {
        auto* candidate = static_cast<Dontfloat::Ara::AraAudioSource*>(source);
        if (candidate->getPersistentID() == std::string { "source-own" }) {
            ownSource = candidate;
        }
    }
    QVERIFY(ownSource != nullptr);

    const Dontfloat::Ara::AraNoteSet reference = controller->referenceNotesExcluding(ownSource);
    QVERIFY2(reference.valid && !reference.notes.empty(), "референс соседней дорожки пуст");

    // Ноты референса → потактовые тональности (полоса под пианороллом)
    QVector<PitchDetector::PitchNote> notes;
    for (const auto& note : reference.notes) {
        PitchDetector::PitchNote out;
        out.startSample = note.startSample;
        out.endSample = note.endSample;
        out.midiPitch = note.midiPitch;
        out.detectedPitch = note.detectedPitch;
        out.confidence = note.confidence;
        notes.append(out);
    }
    KeyAnalyzer::BarGrid grid;
    grid.bpm = 120.0f;
    grid.beatsPerBar = 4;
    grid.gridStartSample = 0;
    const KeyAnalyzer::PerBarKeyResult keys =
        MidiImporter::analyzeKeyPerBar(notes, grid, int(reference.sampleRate));
    QVERIFY2(!keys.regions.isEmpty(), "по нотам референса не собрались тональности");
    QVERIFY(!keys.primaryKey.keyName.isEmpty());
}

// Звук и прогресс разбора доступны плагину без единого блока process()
void AraDocumentControllerTest::testAudioAndProgressAvailableWithoutPlayback()
{
    HostAudioTrack track = HostAudioTrack::makeTone(64.0f);
    const ARA::ARAAudioSourceRef sourceRef = addAnalyzedSource(&track, "source-no-playback");
    QVERIFY(waitForAnalysis(toHostRef(&track)));
    Q_UNUSED(sourceRef);

    auto* controller = static_cast<Dontfloat::Ara::AraDocumentController*>(
        araDocumentController());
    QVERIFY(controller != nullptr);

    Dontfloat::Ara::AraAudioSource* source = nullptr;
    for (ARA::PlugIn::AudioSource* candidate : controller->getDocument()->getAudioSources()) {
        auto* araSource = static_cast<Dontfloat::Ara::AraAudioSource*>(candidate);
        if (araSource->getPersistentID() == std::string { "source-no-playback" }) {
            source = araSource;
        }
    }
    QVERIFY(source != nullptr);

    // Весь звук дорожки уже у плагина — редактор рисует волну без проигрывания
    QCOMPARE(source->monoSamples().size(), track.samples.size());
    QCOMPARE(source->analysisProgress(), 100);
    QVERIFY(!source->analysisRunning());
    QVERIFY(source->hasNotes());
}

// Два экземпляра плагина на разных дорожках одного документа
//
// Так плагин и стоит в DAW: на каждой дорожке свой экземпляр, у каждого свой
// клип и свой аудиоисточник, а документ ARA общий. Проверяется и адресация
// («какой источник мой»), и сам механизм референса — ноты соседней дорожки.
void AraDocumentControllerTest::testInstancesOnDifferentTracksExchangeReferenceNotes()
{
    HostAudioTrack lower = HostAudioTrack::makeTone(60.0f);  // C4 — дорожка 1
    HostAudioTrack upper = HostAudioTrack::makeTone(72.0f);  // C5 — дорожка 2

    const ARA::ARAAudioSourceRef lowerSourceRef = addAnalyzedSource(&lower, "source-lane-1");
    QVERIFY(waitForAnalysis(toHostRef(&lower)));
    const ARA::ARAAudioSourceRef upperSourceRef = addAnalyzedSource(&upper, "source-lane-2");
    QVERIFY(waitForAnalysis(toHostRef(&upper)));

    /** Дорожка глазами хоста: клип на своей region sequence и свой экземпляр. */
    struct Lane {
        ARA::ARARegionSequenceRef sequence = nullptr;
        ARA::ARAAudioModificationRef modification = nullptr;
        ARA::ARAPlaybackRegionRef region = nullptr;
        std::unique_ptr<ARA::PlugIn::PlugInExtension> extension;
        std::unique_ptr<ARA::Host::PlaybackRenderer> renderer;
    };
    Lane lanes[2];

    const auto buildLane = [&](Lane& lane, ARA::ARAAudioSourceRef sourceRef, HostAudioTrack* track,
                               const char* name, const char* modificationId, int order) {
        const double duration = double(track->samples.size()) / track->sampleRate;
        documentController_->beginEditing();
        const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARARegionSequenceProperties, color)>
            sequenceProperties { name, order, musicalContextRef_, nullptr };
        lane.sequence = documentController_->createRegionSequence(
            reinterpret_cast<ARA::ARARegionSequenceHostRef>(track), &sequenceProperties);
        const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioModificationProperties, persistentID)>
            modificationProperties { name, modificationId };
        lane.modification = documentController_->createAudioModification(
            sourceRef, reinterpret_cast<ARA::ARAAudioModificationHostRef>(track),
            &modificationProperties);
        const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAPlaybackRegionProperties, color)>
            regionProperties {
                ARA::kARAPlaybackTransformationNoChanges,
                0.0, duration, 0.0, duration,
                musicalContextRef_, lane.sequence, name, nullptr,
            };
        lane.region = documentController_->createPlaybackRegion(
            lane.modification, reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(track),
            &regionProperties);
        documentController_->endEditing();

        // Привязка экземпляра к документу — то же, что делает обёртка формата
        lane.extension = std::make_unique<ARA::PlugIn::PlugInExtension>();
        const ARA::ARAPlugInExtensionInstance* instance = lane.extension->bindToARA(
            documentController_->getRef(),
            ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole,
            ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole);
        QVERIFY(instance != nullptr);
        // Клип уходит в роль экземпляра: по нему плагин и находит свой источник
        lane.renderer = std::make_unique<ARA::Host::PlaybackRenderer>(instance);
        lane.renderer->addPlaybackRegion(lane.region);
    };

    buildLane(lanes[0], lowerSourceRef, &lower, "lane 1", "modification-lane-1", 0);
    buildLane(lanes[1], upperSourceRef, &upper, "lane 2", "modification-lane-2", 1);

    auto* controller = static_cast<Dontfloat::Ara::AraDocumentController*>(araDocumentController());
    QVERIFY(controller != nullptr);

    // 1. Каждый экземпляр адресует свою дорожку, а не соседнюю
    Dontfloat::Ara::AraAudioSource* ownOfFirst =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*lanes[0].extension);
    Dontfloat::Ara::AraAudioSource* ownOfSecond =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*lanes[1].extension);
    QVERIFY(ownOfFirst != nullptr);
    QVERIFY(ownOfSecond != nullptr);
    // Сравниваем как std::string: QString::fromStdString в Debug против
    // релизных Qt-DLL падает (разные аллокаторы STL)
    QVERIFY2(ownOfFirst->getPersistentID() == std::string { "source-lane-1" },
             "первый экземпляр сел не на свою дорожку");
    QVERIFY2(ownOfSecond->getPersistentID() == std::string { "source-lane-2" },
             "второй экземпляр сел не на свою дорожку");
    QVERIFY(ownOfFirst->hasNotes());
    QVERIFY(ownOfSecond->hasNotes());

    // 2. Референс: каждому экземпляру видны ноты соседней дорожки
    const Dontfloat::Ara::AraNoteSet referenceForFirst =
        controller->referenceNotesExcluding(ownOfFirst);
    const Dontfloat::Ara::AraNoteSet referenceForSecond =
        controller->referenceNotesExcluding(ownOfSecond);
    QVERIFY2(referenceForFirst.valid && !referenceForFirst.notes.empty(),
             "первая дорожка не получила референс со второй");
    QVERIFY2(referenceForSecond.valid && !referenceForSecond.notes.empty(),
             "вторая дорожка не получила референс с первой");

    // 3. Референс — именно чужие ноты: снизу C4, сверху C5, а не наоборот
    const auto averagePitch = [](const std::vector<Dontfloat::PluginCore::TrackPitchNote>& notes) {
        if (notes.empty()) {
            return 0.0;
        }
        double sum = 0.0;
        for (const auto& note : notes) {
            sum += double(note.midiPitch);
        }
        return sum / double(notes.size());
    };
    const double ownPitchOfFirst = averagePitch(ownOfFirst->noteSet().notes);
    const double ownPitchOfSecond = averagePitch(ownOfSecond->noteSet().notes);
    QVERIFY2(averagePitch(referenceForFirst.notes) > ownPitchOfFirst + 6.0,
             "первой дорожке подсунули её же ноты вместо соседних");
    QVERIFY2(averagePitch(referenceForSecond.notes) < ownPitchOfSecond - 6.0,
             "второй дорожке подсунули её же ноты вместо соседних");

    // Разрушение — в порядке ARA: сначала экземпляры, потом объекты модели
    for (Lane& lane : lanes) {
        if (lane.renderer && lane.region) {
            lane.renderer->removePlaybackRegion(lane.region);
        }
        lane.renderer.reset();
        lane.extension.reset();
    }
    documentController_->beginEditing();
    for (Lane& lane : lanes) {
        documentController_->destroyPlaybackRegion(lane.region);
        documentController_->destroyAudioModification(lane.modification);
        documentController_->destroyRegionSequence(lane.sequence);
    }
    documentController_->endEditing();
}

QTEST_MAIN(AraDocumentControllerTest)
#include "ara_document_controller_test.moc"
