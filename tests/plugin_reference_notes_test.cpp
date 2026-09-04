// Ноты соседней дорожки остаются референсом: серыми и неправимыми.
//
// Жалоба, ради которой тест написан: «на первой дорожке DONTFLOAT, на второй
// Pitcher — и в DONTFLOAT правятся ноты второй дорожки, синие вместо серых».
// Синими и правимыми ноты рисуются только тогда, когда попали в рабочий набор
// пианоролла (PitchGridWidget::notes), а туда они попадают ровно из одного
// места: из аудиоисточника, который экземпляр посчитал **своим**.
//
// Здесь поднимается настоящий документ ARA с двумя дорожками разной высоты
// (C4 и C5) и два редактора на нём — как в DAW. Проверяется, что каждый
// редактор правит свои ноты, а ноты соседа лежат отдельным, референсным
// набором. Перепутать их местами тест не даст: высоты разнесены на октаву.

#include <QtTest/QTest>

#include "../plugins/ara/dontfloat_ara_document_controller.h"
#include "../plugins/core/dontfloat_plugin_core.h"
#include "../plugins/ui/dontfloat_pitch_editor.h"
#include "../tools/mini_daw/mini_daw_ara_host.h"

#include "../include/pitchgridwidget.h"

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include <cmath>
#include <memory>

using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginTester::AraHostDocument;
using Dontfloat::PluginTester::AraHostTrack;
using Dontfloat::Plugins::Ui::DontfloatPitchEditor;

namespace {

constexpr double kSampleRate = 44100.0;
constexpr double kToneSeconds = 1.5;
/** C4 и C5: октава между дорожками — по средней высоте видно, чьи это ноты. */
constexpr float kLowerPitch = 60.0f;
constexpr float kUpperPitch = 72.0f;
/** Ноты детектора гуляют на пару полутонов; октава этого запаса не съедает. */
constexpr double kPitchTolerance = 4.0;

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

double averagePitch(const QVector<PitchDetector::PitchNote>& notes)
{
    if (notes.isEmpty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const PitchDetector::PitchNote& note : notes) {
        sum += double(note.midiPitch);
    }
    return sum / double(notes.size());
}

} // namespace

class PluginReferenceNotesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testOwnNotesAreEditableAndNeighbourStaysReference();

private:
    /** Крутит модель и очередь Qt, пока у обоих редакторов не появятся ноты. */
    bool waitForEditors(int timeoutMs = 30000);
    PitchGridWidget* gridOf(int index) const;

    const ARA::ARAFactory* factory_ = nullptr;
    std::unique_ptr<AraHostDocument> document_;
    std::unique_ptr<ARA::PlugIn::PlugInExtension> instances_[2];
    std::unique_ptr<TrackToolSession> sessions_[2];
    std::unique_ptr<DontfloatPitchEditor> editors_[2];
};

void PluginReferenceNotesTest::initTestCase()
{
    factory_ = Dontfloat::Ara::AraDocumentController::getARAFactory();
    QVERIFY(factory_ != nullptr);

    document_ = std::make_unique<AraHostDocument>();
    QString error;
    QVERIFY2(document_->open(factory_, &error), qUtf8Printable(error));

    const AraHostTrack tracks[2] = {
        makeTone(kLowerPitch, QStringLiteral("track A")),
        makeTone(kUpperPitch, QStringLiteral("track B")),
    };

    for (int i = 0; i < 2; ++i) {
        QCOMPARE(document_->addTrack(tracks[i]), i);

        instances_[i] = std::make_unique<ARA::PlugIn::PlugInExtension>();
        const ARA::ARAPlugInExtensionInstance* instance = instances_[i]->bindToARA(
            static_cast<ARA::ARADocumentControllerRef>(document_->documentControllerRef()),
            static_cast<ARA::ARAPlugInInstanceRoleFlags>(AraHostDocument::knownRoles()),
            static_cast<ARA::ARAPlugInInstanceRoleFlags>(AraHostDocument::assignedRoles()));
        QVERIFY(instance != nullptr);
        document_->bindInstance(i, instance);

        // Окно редактора открыто — как в DAW, иначе выбор клипов плагину не виден
        if (auto* view = instances_[i]->getEditorView<Dontfloat::Ara::AraEditorView>()) {
            view->setEditorOpenState(true);
        }

        // Сессия у каждого экземпляра своя: это разные окна плагина
        sessions_[i] = std::make_unique<TrackToolSession>();
        editors_[i] = std::make_unique<DontfloatPitchEditor>();
        editors_[i]->bindSession(sessions_[i].get());
        editors_[i]->setAraBinding(instances_[i].get());
    }

    QVERIFY2(waitForEditors(), "редакторы не получили ноты из документа ARA");
}

void PluginReferenceNotesTest::cleanupTestCase()
{
    // Порядок разрушения задан ARA: редакторы → клипы с ролей → экземпляры →
    // документ. Редактор держит указатель на расширение, поэтому уходит первым
    for (int i = 0; i < 2; ++i) {
        if (editors_[i]) {
            editors_[i]->setAraBinding(nullptr);
        }
        editors_[i].reset();
        sessions_[i].reset();
    }
    for (int i = 0; i < 2; ++i) {
        document_->unbindInstance(i);
        instances_[i].reset();
    }
    document_.reset();
}

PitchGridWidget* PluginReferenceNotesTest::gridOf(int index) const
{
    return editors_[index] ? editors_[index]->findChild<PitchGridWidget*>() : nullptr;
}

bool PluginReferenceNotesTest::waitForEditors(int timeoutMs)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < timeoutMs) {
        document_->pumpModelUpdates();
        QTest::qWait(50);

        const PitchGridWidget* first = gridOf(0);
        const PitchGridWidget* second = gridOf(1);
        if (first && second && !first->notes().isEmpty() && !second->notes().isEmpty()
            && !first->referenceNotes().isEmpty() && !second->referenceNotes().isEmpty()) {
            return true;
        }
    }
    return false;
}

// Свои ноты правятся, соседские — только фон
void PluginReferenceNotesTest::testOwnNotesAreEditableAndNeighbourStaysReference()
{
    PitchGridWidget* first = gridOf(0);
    PitchGridWidget* second = gridOf(1);
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);

    // Рабочий набор — свои ноты: у первой дорожки C4, у второй C5
    QVERIFY2(std::fabs(averagePitch(first->notes()) - double(kLowerPitch)) < kPitchTolerance,
             qPrintable(QStringLiteral("первый редактор правит чужие ноты: средняя высота %1")
                            .arg(averagePitch(first->notes()))));
    QVERIFY2(std::fabs(averagePitch(second->notes()) - double(kUpperPitch)) < kPitchTolerance,
             qPrintable(QStringLiteral("второй редактор правит чужие ноты: средняя высота %1")
                            .arg(averagePitch(second->notes()))));

    // Референс — ноты соседа, и лежат они отдельным набором (рисуются серым)
    QVERIFY2(std::fabs(averagePitch(first->referenceNotes()) - double(kUpperPitch))
                 < kPitchTolerance,
             qPrintable(QStringLiteral("первому редактору достался не тот референс: %1")
                            .arg(averagePitch(first->referenceNotes()))));
    QVERIFY2(std::fabs(averagePitch(second->referenceNotes()) - double(kLowerPitch))
                 < kPitchTolerance,
             qPrintable(QStringLiteral("второму редактору достался не тот референс: %1")
                            .arg(averagePitch(second->referenceNotes()))));

    // И главное: наборы не смешались. Октава между дорожками не оставляет
    // толкования — если бы соседские ноты попали в рабочий набор, средние
    // высоты сошлись бы
    for (PitchGridWidget* grid : { first, second }) {
        QVERIFY2(std::fabs(averagePitch(grid->notes()) - averagePitch(grid->referenceNotes()))
                     > double(kUpperPitch - kLowerPitch) / 2.0,
                 "рабочие ноты и референс совпали — соседская дорожка стала своей");
    }
}

QTEST_MAIN(PluginReferenceNotesTest)
#include "plugin_reference_notes_test.moc"
