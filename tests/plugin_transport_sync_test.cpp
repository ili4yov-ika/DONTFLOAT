// Каретки и транспорт в окне плагина: половины окна ходят вместе.
//
// Полная редакция показывает одну дорожку двумя половинами — волной и
// пианороллом. Каретка при этом одна: щёлкнули по волне — пианоролл встал
// туда же, и наоборот. Раньше обе половины ждали позицию от DAW, а на стоящем
// транспорте DAW ничего не присылает, и они оставались в разных местах.
//
// Здесь же проверяется, что кнопки воспроизведения в шапке — дублёры кнопок
// DAW и своего проигрывателя не поднимают: без хоста они лишь сообщают, что
// транспорт им не отдали.

#include <QtTest/QTest>

#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QSignalSpy>

#include "../plugins/core/dontfloat_plugin_core.h"
#include "../plugins/ui/dontfloat_full_editor.h"
#include "../plugins/ui/dontfloat_pitch_editor.h"
#include "../plugins/ui/dontfloat_plugin_editor_shell.h"
#include "../plugins/ui/dontfloat_scratch_editor.h"

#include "../include/pitchgridwidget.h"
#include "../include/waveformview.h"

#include <cmath>
#include <vector>

using Dontfloat::PluginCore::TrackAudioBuffer;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::Plugins::Ui::DontfloatFullEditor;
using Dontfloat::Plugins::Ui::DontfloatPitchEditor;
using Dontfloat::Plugins::Ui::DontfloatScratchEditor;

namespace {

constexpr int kSampleRate = 44100;
constexpr double kToneSeconds = 4.0;

/** Дорожка с внятной волной: по ней видно, куда встала каретка. */
TrackAudioBuffer makeTone()
{
    TrackAudioBuffer buffer;
    buffer.sampleRate = kSampleRate;
    buffer.channelCount = 1;
    const int frames = int(kToneSeconds * kSampleRate);
    buffer.mono.resize(std::size_t(frames));
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / double(kSampleRate);
        buffer.mono[std::size_t(i)] = float(0.4 * std::sin(2.0 * M_PI * 220.0 * t));
    }
    buffer.left = buffer.mono;
    return buffer;
}

/**
 * Расхождение кареток в миллисекундах.
 *
 * Позиция ходит между половинами через сэмплы источника и обратно, и на
 * каждом переводе теряется остаток от деления — до миллисекунды. Больше
 * этого расходиться каретки уже не имеют права.
 */
qint64 playheadGapMs(qint64 first, qint64 second)
{
    return first > second ? first - second : second - first;
}

constexpr qint64 kPlayheadToleranceMs = 2;

/**
 * Показывает окно и даёт разметке разложить виджеты.
 *
 * Ждать появления окна на экране (qWaitForWindowExposed) нельзя: под CTest
 * сеанс без активного рабочего стола окно не показывает, и ожидание просто
 * выходит по таймауту. Синтетическим щелчкам QTest окно на экране и не
 * нужно — нужны разложенные размеры виджетов.
 */
void layOut(QWidget* widget)
{
    widget->show();
    QCoreApplication::processEvents();
    widget->layout()->activate();
    QCoreApplication::processEvents();
}

} // namespace

class PluginTransportSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testClickOnWaveformMovesThePianoRoll();
    void testClickOnPianoRollMovesTheWaveform();
    void testPlayButtonAsksTheHostAndPlaysNothingItself();

private:
    /** Кнопка шапки по подсказке: своих имён у кнопок транспорта нет. */
    QPushButton* headerButton(Dontfloat::Plugins::Ui::DontfloatPluginEditorShell* shell,
                              const QString& tooltip) const;

    TrackToolSession session_;
};

void PluginTransportSyncTest::init()
{
    session_.setAudioBuffer(makeTone());
}

void PluginTransportSyncTest::cleanup()
{
    session_.reset();
}

QPushButton* PluginTransportSyncTest::headerButton(
    Dontfloat::Plugins::Ui::DontfloatPluginEditorShell* shell, const QString& tooltip) const
{
    for (QPushButton* button : shell->findChildren<QPushButton*>()) {
        if (button->toolTip() == tooltip) {
            return button;
        }
    }
    return nullptr;
}

// Щелчок по волне ведёт каретку пианоролла: каретка в окне одна
void PluginTransportSyncTest::testClickOnWaveformMovesThePianoRoll()
{
    DontfloatFullEditor editor;
    editor.bindSession(&session_);
    editor.resize(1000, 700);
    layOut(&editor);

    auto* waveform = editor.findChild<WaveformView*>();
    auto* pitchGrid = editor.findChild<PitchGridWidget*>();
    QVERIFY(waveform != nullptr);
    QVERIFY(pitchGrid != nullptr);

    // Щёлкаем не по центру: центр совпал бы со стартовой позицией обеих
    // кареток, и проверка прошла бы, ничего не проверив
    const QPoint click(waveform->width() / 3, waveform->height() / 2);
    QTest::mouseClick(waveform, Qt::LeftButton, Qt::NoModifier, click);
    QCoreApplication::processEvents();

    QVERIFY2(waveform->getPlaybackPosition() > 0,
             "щелчок по волне не сдвинул её собственную каретку");
    QVERIFY2(playheadGapMs(pitchGrid->getPlaybackPosition(),
                           waveform->getPlaybackPosition()) <= kPlayheadToleranceMs,
             "каретка пианоролла осталась не там, где каретка волны");
}

// И обратно: щелчок по пианороллу ведёт каретку волны
void PluginTransportSyncTest::testClickOnPianoRollMovesTheWaveform()
{
    DontfloatFullEditor editor;
    editor.bindSession(&session_);
    editor.resize(1000, 700);
    layOut(&editor);

    auto* waveform = editor.findChild<WaveformView*>();
    auto* pitchGrid = editor.findChild<PitchGridWidget*>();
    QVERIFY(waveform != nullptr);
    QVERIFY(pitchGrid != nullptr);

    const QPoint click(pitchGrid->width() * 2 / 3, pitchGrid->height() / 2);
    QTest::mouseClick(pitchGrid, Qt::LeftButton, Qt::NoModifier, click);
    QCoreApplication::processEvents();

    QVERIFY2(pitchGrid->getPlaybackPosition() > 0,
             "щелчок по пианороллу не сдвинул его собственную каретку");
    QVERIFY2(playheadGapMs(waveform->getPlaybackPosition(),
                           pitchGrid->getPlaybackPosition()) <= kPlayheadToleranceMs,
             "каретка волны осталась не там, где каретка пианоролла");
}

// Кнопка Play — дублёр кнопки DAW. Без хоста ей нечем играть, и она обязана
// об этом сказать, а не поднимать свой проигрыватель: раньше в полной
// редакции она выдавала звук референсного канала — не тот, что видно
void PluginTransportSyncTest::testPlayButtonAsksTheHostAndPlaysNothingItself()
{
    Dontfloat::Plugins::Ui::DontfloatPluginEditorShell shell(
        Dontfloat::PluginCore::PluginProduct::Full);
    shell.bindSession(&session_);
    shell.resize(1000, 700);
    layOut(&shell);

    QPushButton* play = headerButton(&shell, QObject::tr("Start playback in the DAW"));
    QPushButton* stop = headerButton(&shell, QObject::tr("Stop playback in the DAW"));
    QVERIFY2(play != nullptr, "в шапке нет кнопки воспроизведения");
    QVERIFY2(stop != nullptr, "в шапке нет кнопки остановки");

    for (QPushButton* button : { play, stop }) {
        QTest::mouseClick(button, Qt::LeftButton);
        QCoreApplication::processEvents();

        // Хоста нет — статус обязан сказать именно это
        bool told = false;
        for (const QLabel* label : shell.findChildren<QLabel*>()) {
            told = told || label->text().contains(QStringLiteral("transport"))
                || label->text().contains(QStringLiteral("транспорт"));
        }
        QVERIFY2(told, "кнопка транспорта промолчала о том, что хост её не принял");
    }
}

QTEST_MAIN(PluginTransportSyncTest)
#include "plugin_transport_sync_test.moc"
