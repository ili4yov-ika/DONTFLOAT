#ifndef DONTFLOAT_PLUGIN_EDITOR_SHELL_H
#define DONTFLOAT_PLUGIN_EDITOR_SHELL_H

/**
 * Оболочка редактора плагина: шапка и статусбар «как в главном окне».
 *
 * Разметка — макет `MARKDOWN/example_plugin_dontfloat.svg`: слева имя
 * редакции, справа те же кнопки, что на панели главного окна (OD / < / BG / >
 * и транспорт ▶ ■ метроном A B ↺), под ними содержимое редакции, внизу
 * статусбар. Оформление берётся из `dontfloat_plugin_theme.h`.
 */

#include "../core/plugin_product.h"

#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
class QLabel;
class QScrollArea;
class QPushButton;
QT_END_NAMESPACE

class MetronomeController;
class NotePreviewPlayer;

namespace Dontfloat::Plugins::Ui {

class DontfloatEditorContent;

class DontfloatPluginEditorShell final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatPluginEditorShell(Dontfloat::PluginCore::PluginProduct product,
                                        QWidget* parent = nullptr);

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void notifyHostAudioAppended();
    /** Позиция каретки DAW (секунды проекта) — двигает каретку редактора. */
    void setHostPlayheadSeconds(double projectSeconds);
    /** Тактовая сетка DAW: темп, доли в такте и граница такта в сэмплах. */
    /** Проброс привязки ARA в содержимое редактора (см. DontfloatEditorContent). */
    void setAraBinding(const void* extension);
    /**
     * К какому экземпляру привязано это окно.
     *
     * По ней обёртка формата решает, чьи позиции каретки окну показывать:
     * экземпляров в проекте несколько, и раньше окно получало их вперемешку.
     */
    const void* araBinding() const { return araBinding_; }
    /**
     * Транспорт DAW пошёл или встал.
     *
     * Кнопка воспроизведения в плагине — дублёр кнопки хоста, поэтому она
     * показывает его состояние, а не своё: нажали Play в DAW — кнопка здесь
     * тоже становится «играет».
     */
    void setHostTransportPlaying(bool playing);

    void setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample);
    /**
     * Обработчик «переставить каретку DAW»: редактор зовёт его, когда каретку
     * двигают внутри плагина. Обёртка формата передаёт запрос хосту.
     */
    void setHostSeekHandler(std::function<void(qint64)> handler)
    {
        hostSeekHandler_ = std::move(handler);
    }
    /**
     * Обработчик «плагин пересчитал звук»: хост должен прогнать дорожку
     * заново, иначе правки не будут слышны.
     */
    void setHostRenderChangedHandler(std::function<void()> handler)
    {
        hostRenderChangedHandler_ = std::move(handler);
    }

private slots:
    /** Локальное прослушивание дорожки, захваченной из DAW. */
    void onPreviewPlayClicked();
    void onPreviewStopClicked();
    void onMetronomeToggled(bool enabled);
    void onLoopToggled(bool enabled);

private:
    QWidget* buildHeader();
    /** Кнопка панели: размеры и вид как в главном окне (32×32). */
    QPushButton* makeToolButton(QWidget* parent, const QString& text, const QString& iconPath,
                                const QString& tooltip, bool checkable = false);
    void showStatus(const QString& text);
    /** Моно-микс дорожки сессии для прослушивания и метронома. */
    QVector<float> sessionMonoMix(int* sampleRateOut) const;
    /** Мультимедиа поднимаем лениво: при создании редактора это чревато
     *  вложенным циклом событий у хоста. */
    NotePreviewPlayer* ensurePreviewPlayer();
    MetronomeController* ensureMetronome();

    Dontfloat::PluginCore::PluginProduct product_;
    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    QWidget* contentWidget_ = nullptr;
    /** Содержимое прокручивается, когда хост сжал окно (см. конструктор). */
    QScrollArea* contentScroll_ = nullptr;
    DontfloatEditorContent* content_ = nullptr;

    QLabel* statusBar_ = nullptr;
    QPushButton* playButton_ = nullptr;
    /** Играет ли сейчас транспорт DAW — кнопка показывает именно его. */
    bool hostPlaying_ = false;
    const void* araBinding_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* metronomeButton_ = nullptr;
    QPushButton* loopButton_ = nullptr;
    QWidget* gridToolsGroup_ = nullptr;
    QWidget* loopToolsGroup_ = nullptr;

    std::function<void(qint64)> hostSeekHandler_;
    std::function<void()> hostRenderChangedHandler_;
    NotePreviewPlayer* previewPlayer_ = nullptr;
    MetronomeController* metronome_ = nullptr;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PLUGIN_EDITOR_SHELL_H
