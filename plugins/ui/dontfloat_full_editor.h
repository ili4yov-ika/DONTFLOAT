#ifndef DONTFLOAT_FULL_EDITOR_H
#define DONTFLOAT_FULL_EDITOR_H

#include "../core/dontfloat_plugin_core.h"
#include "dontfloat_editor_content.h"

#include <QWidget>

namespace Dontfloat::Plugins::Ui {

class DontfloatScratchEditor;
class DontfloatPitchEditor;

/**
 * Полная редакция: волна сверху, пианоролл снизу — тот же порядок, что в
 * главном окне (макет `MARKDOWN/example_plugin_dontfloat.svg`).
 * Инструменты волны из шапки уходят в секцию Scratch.
 */
class DontfloatFullEditor final : public QWidget, public DontfloatEditorContent {
    Q_OBJECT

public:
    explicit DontfloatFullEditor(QWidget* parent = nullptr);

    QWidget* widget() override { return this; }
    void bindSession(Dontfloat::PluginCore::TrackToolSession* session) override;
    void notifyHostAudioAppended() override;
    /** Каретка DAW (сэмплы дорожки) — в обе секции. */
    void setHostPlayhead(qint64 samplePosition) override;
    /** Тактовая сетка DAW — в обе секции. */
    void setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample) override;
    /**
     * Привязку к документу ARA получают оба вложенных редактора: волне нужен
     * звук, пианороллу — ноты и разметка. Раньше она не доходила ни до
     * одного: базовая реализация в DontfloatEditorContent пустая.
     */
    void setAraBinding(const void* extension) override;

    bool hasWaveformTools() const override { return true; }
    void shiftBeatGrid(int beats) override;
    void snapMarkersToGrid() override;
    void detectOnsetMarkers() override;
    void setLoopBoundAtPlayhead(bool start) override;
    void setLoopEnabled(bool enabled) override;
    bool loopRegionMs(qint64* startMs, qint64* endMs) const override;

signals:
    /** Текст для статусбара оболочки плагина (из обеих секций). */
    void statusMessage(const QString& text);
    /** Каретку двинули в плагине — DAW должна встать туда же. */
    void seekRequested(qint64 samplePosition);
    /** Плагин пересчитал звук — хосту стоит прогнать дорожку заново. */
    void renderedOutputChanged();

private:
    DontfloatScratchEditor* scratch_ = nullptr;
    DontfloatPitchEditor* pitch_ = nullptr;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_FULL_EDITOR_H
