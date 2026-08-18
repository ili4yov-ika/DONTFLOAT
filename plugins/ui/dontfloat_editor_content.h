#ifndef DONTFLOAT_EDITOR_CONTENT_H
#define DONTFLOAT_EDITOR_CONTENT_H

/**
 * Что оболочка плагина умеет спросить у своего содержимого.
 *
 * Шапка редактора повторяет панель главного окна (макет
 * `MARKDOWN/example_plugin_dontfloat.svg`), а нажатия уходят в редакцию:
 * Full и Scratch работают с волной и метками, Pitcher — нет, поэтому у
 * инструментов волны есть пустая реализация по умолчанию, а кнопки для
 * такой редакции просто скрыты (`hasWaveformTools`).
 */

#include <QtCore/QtGlobal>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Dontfloat::PluginCore {
class TrackToolSession;
}

namespace Dontfloat::Plugins::Ui {

class DontfloatEditorContent {
public:
    virtual ~DontfloatEditorContent() = default;

    virtual QWidget* widget() = 0;
    virtual void bindSession(Dontfloat::PluginCore::TrackToolSession* session) = 0;
    virtual void notifyHostAudioAppended() = 0;
    /** Каретка DAW в сэмплах дорожки. */
    virtual void setHostPlayhead(qint64 samplePosition) = 0;

    /**
     * Тактовая сетка DAW: темп, доли в такте и позиция ближайшей границы такта
     * (сэмплы дорожки). По ней сетка плагина совпадает с сеткой хоста.
     */
    virtual void setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample)
    {
        Q_UNUSED(bpm);
        Q_UNUSED(beatsPerBar);
        Q_UNUSED(barStartSample);
    }

#if defined(DONTFLOAT_WITH_ARA)
    /**
     * Экземпляр привязали к документу ARA: дальше ноты, тактовая сетка и
     * референс с соседних дорожек берутся из общей модели, а не из захвата
     * блоками.  extension — ARA::PlugIn::PlugInExtension этого экземпляра
     * (тип скрыт за void*, чтобы заголовок не тянул ARA в сборки без неё).
     */
    virtual void setAraBinding(const void* extension) { Q_UNUSED(extension); }
#endif

    /** Есть ли волна с тактовой сеткой и метками (кнопки OD / < / BG / > / A / B). */
    virtual bool hasWaveformTools() const { return false; }
    /** Сдвиг тактовой сетки на \a beats долей (знак — направление). */
    virtual void shiftBeatGrid(int beats) { Q_UNUSED(beats); }
    /** Привязка всех меток к тактовой сетке. */
    virtual void snapMarkersToGrid() {}
    /** Метки по транзиентам (общий алгоритм MarkerUtils::detectOnsetSamples). */
    virtual void detectOnsetMarkers() {}
    /** Точка цикла A (\a start = true) или B по позиции каретки. */
    virtual void setLoopBoundAtPlayhead(bool start) { Q_UNUSED(start); }
    /** Показ области цикла. */
    virtual void setLoopEnabled(bool enabled) { Q_UNUSED(enabled); }
    /**
     * Границы включённого цикла в миллисекундах — по ним оболочка
     * прослушивает кусок дорожки, а не весь трек.
     * @return false, если цикл выключен или точки не заданы.
     */
    virtual bool loopRegionMs(qint64* startMs, qint64* endMs) const
    {
        Q_UNUSED(startMs);
        Q_UNUSED(endMs);
        return false;
    }
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_EDITOR_CONTENT_H
