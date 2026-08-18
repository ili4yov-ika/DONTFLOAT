#ifndef DONTFLOAT_MINI_DAW_WINDOW_H
#define DONTFLOAT_MINI_DAW_WINDOW_H

// Окно мини-DAW по макету MARKDOWN/example_window_minidaw.svg:
// верхняя панель (открыть файл, вид и редакция плагина, воспроизведение, стоп),
// полоска плейбека с кареткой и область плагина в красной рамке.

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <memory>

#include "mini_daw_player.h"
#include "mini_daw_plugin_host.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
class QToolButton;
QT_END_NAMESPACE

namespace MiniDaw {

/**
 * Дорожка плейбека по макету: звуковая волна с тактовой сеткой, проигранная
 * часть залита красным, каретка-треугольник над дорожкой и время под ней.
 */
class PlaybackBar : public QWidget {
    Q_OBJECT

public:
    explicit PlaybackBar(QWidget* parent = nullptr);

    /** Аудио дорожки; пустое — рисуется пустая дорожка. */
    void setTrack(const QVector<float>& left, const QVector<float>& right, int sampleRate);
    void clearTrack();
    void setPosition(qint64 frames);
    /** Тактовая сетка: темп и размер такта из полей панели. */
    void setBeatGrid(float bpm, int beatsPerBar);
    /** Имя трека, подписанное на дорожке. */
    void setTrackName(const QString& name);
    /** Границы клипов (кадры) — рисуются линиями поверх дорожки. */
    void setClipBoundaries(const QVector<qint64>& boundaries);

    qint64 position() const { return position_; }
    qint64 totalFrames() const { return totalFrames_; }

    static constexpr int kTrackHeight = 52;
    static constexpr int kCaretHeight = 8;
    static constexpr int kTimeRowHeight = 15;
    static constexpr int kBarHeight = kCaretHeight + kTrackHeight + kTimeRowHeight;

signals:
    /** Пользователь ткнул в дорожку — перемотка. */
    void seekRequested(qint64 frame);
    /** Клип тащили правой кнопкой — сдвинуть выбранный клип на столько кадров. */
    void clipMoveRequested(qint64 deltaFrames);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect trackRect() const;
    /** Позиция на дорожке по координате X. */
    qint64 frameAtX(int x) const;
    void rebuildEnvelope();
    void drawWaveform(QPainter& painter, const QRect& area) const;
    void drawBeatGrid(QPainter& painter, const QRect& area) const;
    static QString formatTime(qint64 frames, int sampleRate);

    QVector<QVector<float>> channels_;
    QVector<int> envelopeUpper_;   ///< верхняя огибающая, пиксели по Y
    QVector<int> envelopeLower_;
    QString trackName_;
    qint64 totalFrames_ = 0;
    qint64 position_ = 0;
    int sampleRate_ = 44100;
    float bpm_ = 120.0f;
    int beatsPerBar_ = 4;
    /** Границы клипов и состояние перетаскивания правой кнопкой. */
    QVector<qint64> clipBoundaries_;
    int dragStartX_ = 0;
    qint64 dragDeltaFrames_ = 0;
    bool draggingClip_ = false;
};

class Window : public QWidget {
    Q_OBJECT

public:
    explicit Window(QWidget* parent = nullptr);
    ~Window() override;

    /** Загружает трек (путь из командной строки или диалога). */
    bool openAudio(const QString& path);
    /** Выбирает плагин в списках (для запуска с ключами --format/--product). */
    void selectPlugin(PluginFormat format, PluginProduct product);
    /** Запускать транспорт сразу после загрузки трека (ключ --autoplay). */
    void setAutoPlay(bool enabled) { autoPlay_ = enabled; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    /** Гасит колесо над выпадающими списками: случайный скролл менял плагин. */
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onOpenClicked();
    void onPlayClicked();
    void onStopClicked();
    void onSelectionChanged();
    void onSeekRequested(qint64 frame);
    /** Клип перетащили мышью — двигаем выбранный клип на дельту. */
    void onClipMoveRequested(qint64 deltaFrames);
    void onBeatGridChanged();
    void tickPosition();

private:
    /**
     * Клип на дорожке: кусок исходного файла со своей позицией, длиной и
     * коэффициентом растяжения. Так мини-DAW умеет то же, что настоящая DAW —
     * резать, двигать, укорачивать и растягивать, — а плагин видит результат.
     */
    struct Clip {
        qint64 timelineStart = 0;  ///< позиция на дорожке (кадры)
        qint64 sourceStart = 0;    ///< откуда берётся материал
        qint64 sourceLength = 0;   ///< сколько исходных кадров занимает
        double stretch = 1.0;      ///< >1 — растянут во времени (звучит дольше)

        qint64 timelineLength() const { return qint64(double(sourceLength) * stretch); }
        qint64 timelineEnd() const { return timelineStart + timelineLength(); }
    };

    void buildUi();
    void applyStyle();
    /** Перезагружает выбранный плагин и встраивает его редактор в панель. */
    void reloadPlugin();
    /** Сдвиг клипа на \a seconds секунд (Ctrl+←/→). */
    void nudgeClip(int seconds);
    /** Разрез клипа под кареткой на два (клавиша S — как в DAW). */
    void splitClipAtPlayhead();
    /** Сдвиг края клипа: \a startEdge — левый край, иначе правый. */
    void trimSelectedClip(bool startEdge, qint64 deltaFrames);
    /** Растяжение/сжатие клипа во времени (коэффициент умножается). */
    void stretchSelectedClip(double factor);
    /** Индекс клипа под позицией; -1 — там пусто. */
    int clipAt(qint64 frame) const;
    /** Пересобирает дорожку из клипов (растяжение — линейной интерполяцией). */
    void renderTimeline();
    /** Пересборка + прогон через плагин + обновление плеера и дорожки. */
    void applyClipEdit(const QString& statusText);
    /** Прогоняет загруженный трек через плагин: плагин видит трек, мы — выход. */
    void runTrackThroughPlugin();
    void showPluginMessage(const QString& text, bool isError);
    void updateTransportUi();
    void updateWindowTitle();

    PluginFormat currentFormat() const;
    PluginProduct currentProduct() const;

    QToolButton* openButton_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QComboBox* productCombo_ = nullptr;
    QLineEdit* bpmEdit_ = nullptr;      ///< темп хоста
    QComboBox* beatsCombo_ = nullptr;   ///< размер такта
    QToolButton* playButton_ = nullptr;
    QToolButton* stopButton_ = nullptr;
    PlaybackBar* playbackBar_ = nullptr;
    QWidget* pluginFrame_ = nullptr;   ///< красная рамка
    QWidget* pluginSurface_ = nullptr; ///< нативное окно-родитель для редактора
    QLabel* pluginMessage_ = nullptr;
    QTimer* positionTimer_ = nullptr;

    std::unique_ptr<PluginHost> host_;
    Player player_;

    QString audioPath_;
    QVector<float> sourceLeft_;
    QVector<float> sourceRight_;
    /** Дорожка, собранная из клипов: её слышит транспорт и видит плагин. */
    QVector<float> timelineLeft_;
    QVector<float> timelineRight_;
    QVector<Clip> clips_;
    int selectedClip_ = 0;
    int sampleRate_ = 44100;
    bool autoPlay_ = false;
    /** Защита от повторного входа в reloadPlugin из вложенного цикла событий. */
    bool reloadingPlugin_ = false;
};

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_WINDOW_H
