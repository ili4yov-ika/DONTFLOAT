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

#include <memory>

#include "plugin_host_probe.h"

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

    /** Сообщает плагину новый размер области редактора. */
    virtual void resizeEditor(QSize size) = 0;

    /** Прогоняет блок стерео-аудио через плагин (обработка на месте). */
    virtual void process(float* left, float* right, int frames) = 0;

    /** Выгружает редактор и экземпляр плагина. */
    virtual void unload() = 0;

    /** Человекочитаемое имя загруженного плагина (для заголовка окна). */
    virtual QString displayName() const = 0;
};

/** Хост для формата; nullptr — формат не поддержан в этой сборке. */
std::unique_ptr<PluginHost> createPluginHost(PluginFormat format);

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_PLUGIN_HOST_H
