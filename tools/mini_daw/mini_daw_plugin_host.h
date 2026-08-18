#ifndef DONTFLOAT_MINI_DAW_PLUGIN_HOST_H
#define DONTFLOAT_MINI_DAW_PLUGIN_HOST_H

// Рантайм-хост плагинов DONTFLOAT для мини-DAW.
//
// В отличие от headless mini_daw_* целей (плагин влинкован на этапе сборки),
// здесь модуль выбранного формата и продукта грузится в рантайме — как это
// делает настоящая DAW: LoadLibrary → фабрика → экземпляр → редактор в окно
// хоста → блоки аудио через process().

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/qwindowdefs.h>

#include <functional>
#include <memory>

#include "plugin_host_probe.h"
#if defined(DONTFLOAT_WITH_ARA)
#include "mini_daw_ara_host.h"
#endif

namespace MiniDaw {

using Dontfloat::PluginTester::PluginFormat;
using Dontfloat::PluginTester::PluginProduct;

/** Загруженный экземпляр плагина: DSP + встроенный редактор. */
class PluginHost {
public:
    virtual ~PluginHost() = default;

    /**
     * Грузит модуль плагина и готовит его к обработке.
     * @param path       файл/бандл плагина (см. resolvePluginPath)
     * @param sampleRate частота дискретизации трека
     * @param blockSize  максимальный размер блока обработки
     */
    virtual bool load(const QString& path, PluginProduct product,
                      double sampleRate, int blockSize, QString* error) = 0;

    /** Встраивает редактор плагина в нативное окно \a parent. */
    virtual bool embedEditor(WId parent, QSize* editorSize, QString* error) = 0;

#if defined(DONTFLOAT_WITH_ARA)
    /** Отдаёт ли плагин фабрику ARA 2 (тогда доступен путь через документ ARA). */
    virtual bool supportsAra() const { return false; }
    /**
     * Поднимает документ ARA на дорожке track и привязывает к нему экземпляр:
     * дальше плагин читает звук сам и разбирает его без проигрывания.
     */
    virtual bool startAraSession(const Dontfloat::PluginTester::AraHostTrack& track, QString* error)
    {
        Q_UNUSED(track);
        if (error) {
            *error = QStringLiteral("этот формат не поддерживает ARA");
        }
        return false;
    }
    /** Прокачивает обновления модели ARA (разбор идёт в фоне плагина). */
    virtual void pumpAra() {}
    /** Сколько нот плагин отдал хосту через ARA. */
    virtual int araNoteCount() const { return 0; }
    /** Разбор через ARA завершён. */
    virtual bool araAnalysisCompleted() const { return false; }
#endif

    /** Сообщает плагину новый размер области редактора. */
    virtual void resizeEditor(QSize size) = 0;

    /**
     * Прогоняет блок стерео-аудио через плагин (обработка на месте).
     * @param timelineFrame позиция начала блока на таймлайне; плагин пишет
     *        захват по ней, поэтому сдвиг клипа виден ему как сдвиг
     *        содержимого. Отрицательное значение — позиция неизвестна.
     */
    virtual void process(float* left, float* right, int frames, qint64 timelineFrame = -1) = 0;

    /** Транспорт хоста: темп и размер такта из полей панели. */
    virtual void setTransport(double bpm, int beatsPerBar) = 0;

    /**
     * Позиция каретки транспорта: плагин двигает свою каретку синхронно с DAW.
     * Отправляется пустым блоком process() — аудио в сессию при этом не идёт.
     */
    virtual void setPlayhead(qint64 frame, bool playing) = 0;

    /**
     * Плагин просит переставить каретку DAW (своё расширение CLAP
     * `dontfloat.transport/1`; в VST3/LV2 такого канала нет).
     * @param handler позиция в кадрах дорожки.
     */
    virtual void setSeekRequestHandler(std::function<void(qint64)> handler) { Q_UNUSED(handler); }

    /**
     * Плагин пересчитал звук (коррекция высот, растяжение) — дорожку надо
     * прогнать через него заново, иначе правки не слышны.
     */
    virtual void setRenderChangedHandler(std::function<void()> handler) { Q_UNUSED(handler); }

    /** Выгружает редактор и экземпляр плагина. */
    virtual void unload() = 0;

    /** Человекочитаемое имя загруженного плагина (для заголовка окна). */
    virtual QString displayName() const = 0;
};

/** Хост для формата; nullptr — формат не поддержан в этой сборке. */
std::unique_ptr<PluginHost> createPluginHost(PluginFormat format);

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_PLUGIN_HOST_H
