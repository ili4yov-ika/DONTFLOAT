#ifndef DONTFLOAT_MINI_DAW_WINDOW_H
#define DONTFLOAT_MINI_DAW_WINDOW_H

// Окно мини-DAW по макету MARKDOWN/example_window_minidaw.svg:
// верхняя панель (открыть файл, номер дорожки, вид и редакция плагина,
// воспроизведение, стоп), полоска плейбека с двумя дорожками и кареткой,
// область плагина выбранной дорожки в красной рамке.

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <memory>
#include <vector>

#include "mini_daw_clip_model.h"
#include "mini_daw_player.h"
#include "mini_daw_plugin_host.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTimer;
class QToolButton;
QT_END_NAMESPACE

namespace MiniDaw {

/**
 * Дорожки плейбека по макету: звуковая волна с тактовой сеткой, проигранная
 * часть залита цветом дорожки, каретка-треугольник над дорожками и время под
 * ними. Дорожек две — как на макете; выбранная обведена светлой рамкой.
 */
class PlaybackBar : public QWidget {
    Q_OBJECT

public:
    explicit PlaybackBar(QWidget* parent = nullptr);

    /** Сколько дорожек показывает панель (как на макете — две). */
    static constexpr int kTrackCount = 2;

    /** Аудио дорожки \a index; пустое — рисуется пустая дорожка. */
    void setTrack(int index, const QVector<float>& left, const QVector<float>& right,
                  int sampleRate);
    void clearTrack(int index);
    /** Подсветка выбранной дорожки: её редактор показан внизу. */
    void setActiveTrack(int index);
    void setPosition(qint64 frames);
    /** Тактовая сетка: темп и размер такта из полей панели. */
    void setBeatGrid(float bpm, int beatsPerBar);
    /** Имя трека, подписанное на дорожке \a index. */
    void setTrackName(int index, const QString& name);
    /** Границы клипов (кадры) дорожки \a index — линиями поверх волны. */
    void setClipBoundaries(int index, const QVector<qint64>& boundaries);

    qint64 position() const { return position_; }
    qint64 totalFrames() const { return totalFrames_; }
    int activeTrack() const { return activeTrack_; }
    /** Длина дорожки \a index в кадрах (0 — дорожка пуста). */
    qint64 trackFrames(int index) const;

    static constexpr int kTrackHeight = 52;
    static constexpr int kCaretHeight = 8;
    static constexpr int kTimeRowHeight = 15;
    static constexpr int kBarHeight = kCaretHeight + kTrackCount * kTrackHeight + kTimeRowHeight;

signals:
    /** Пользователь ткнул в дорожку — перемотка. */
    void seekRequested(qint64 frame);
    /** Пользователь ткнул в дорожку \a index — сделать её активной. */
    void trackClicked(int index);
    /** Клип тащили правой кнопкой — сдвинуть выбранный клип на столько кадров. */
    void clipMoveRequested(qint64 deltaFrames);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /** Общая область всех дорожек. */
    QRect tracksRect() const;
    /** Область одной дорожки \a index. */
    QRect trackRect(int index) const;
    /** Дорожка под координатой Y; -1 — клик мимо дорожек. */
    int trackAtY(int y) const;
    /** Позиция на дорожке по координате X. */
    qint64 frameAtX(int x) const;
    void rebuildEnvelope(int index);
    void drawWaveform(QPainter& painter, int index, const QRect& area) const;
    void drawBeatGrid(QPainter& painter, const QRect& area) const;
    static QString formatTime(qint64 frames, int sampleRate);

    /** Одна дорожка панели: звук, огибающая, имя и границы клипов. */
    struct Lane {
        QVector<QVector<float>> channels;
        QVector<int> envelopeUpper;   ///< верхняя огибающая, пиксели по Y
        QVector<int> envelopeLower;
        QString name;
        QVector<qint64> clipBoundaries;
        qint64 frames = 0;
    };
    Lane lanes_[kTrackCount];
    int activeTrack_ = 0;
    qint64 totalFrames_ = 0;
    qint64 position_ = 0;
    int sampleRate_ = 44100;
    float bpm_ = 120.0f;
    int beatsPerBar_ = 4;
    /** Состояние перетаскивания клипа правой кнопкой. */
    int dragStartX_ = 0;
    qint64 dragDeltaFrames_ = 0;
    bool draggingClip_ = false;
};

class Window : public QWidget {
    Q_OBJECT

public:
    explicit Window(QWidget* parent = nullptr);
    ~Window() override;

    /** Сколько дорожек в мини-DAW (у каждой свой экземпляр плагина). */
    static constexpr int kTrackCount = PlaybackBar::kTrackCount;

    /** Загружает трек в активную дорожку (путь из командной строки/диалога). */
    bool openAudio(const QString& path);
    /** Загружает трек в дорожку \a trackIndex. */
    bool openAudio(int trackIndex, const QString& path);
    /** Делает дорожку активной: её редактор показан, правки идут в неё. */
    void setActiveTrack(int trackIndex);
    int activeTrack() const { return activeTrack_; }
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
    /** Сменили номер дорожки в выпадающем списке. */
    void onTrackChanged();
    void onSeekRequested(qint64 frame);
    /** Клип перетащили мышью — двигаем выбранный клип на дельту. */
    void onClipMoveRequested(qint64 deltaFrames);
    void onBeatGridChanged();
    void tickPosition();

private:
    /**
     * Одна дорожка мини-DAW со своим файлом, клипами и **своим экземпляром
     * плагина**. Два экземпляра в одном процессе — это и есть та ситуация,
     * ради которой в плагине сделана общая доска нот: соседняя дорожка видит
     * ноты первой как референс.
     */
    struct TrackState {
        QString audioPath;
        QVector<float> sourceLeft;
        QVector<float> sourceRight;
        /** Дорожка, собранная из клипов: её видит плагин. */
        QVector<float> timelineLeft;
        QVector<float> timelineRight;
        /** Что вернул плагин: это слышит транспорт и рисует панель. */
        QVector<float> renderedLeft;
        QVector<float> renderedRight;
        QVector<Clip> clips;
        int selectedClip = 0;
        int sampleRate = 44100;
        PluginFormat format = PluginFormat::Vst3;
        PluginProduct product = PluginProduct::Full;
        std::unique_ptr<PluginHost> host;
        QWidget* surface = nullptr;   ///< нативное окно-родитель для редактора
        QLabel* message = nullptr;
#if defined(DONTFLOAT_WITH_ARA)
        bool araActive = false;
        /** Документ ARA, на котором сидит экземпляр (владеет им окно). */
        Dontfloat::PluginTester::AraHostDocument* araDocument = nullptr;
        /** Индекс дорожки внутри документа; -1 — дорожки в документе нет. */
        int araTrackIndex = -1;
#endif
        bool hasAudio() const { return !sourceLeft.isEmpty(); }
    };

    void buildUi();
    void applyStyle();
    /** Перезагружает плагин дорожки \a trackIndex и встраивает его редактор. */
    void reloadPlugin(int trackIndex);
    /** Сдвиг клипа на \a seconds секунд (Ctrl+←/→). */
    void nudgeClip(int seconds);
    /** Разрез клипа под кареткой на два (клавиша S — как в DAW). */
    void splitClipAtPlayhead();
    /** Сдвиг края клипа: \a startEdge — левый край, иначе правый. */
    void trimSelectedClip(bool startEdge, qint64 deltaFrames);
    /** Растяжение/сжатие клипа во времени (коэффициент умножается). */
    void stretchSelectedClip(double factor);
    /** Индекс клипа под позицией на дорожке \a trackIndex; -1 — там пусто. */
    int clipAt(int trackIndex, qint64 frame) const;
    /** Пересобирает дорожку из клипов (растяжение — линейной интерполяцией). */
    void renderTimeline(int trackIndex);
    /** Пересборка + прогон через плагин + обновление плеера и дорожки. */
    void applyClipEdit(const QString& statusText);
    /** Прогоняет дорожку через её плагин: плагин видит трек, мы — выход. */
    void runTrackThroughPlugin(int trackIndex);
    /** Смешивает выходы всех дорожек и отдаёт транспорту. */
    void updatePlayerMix();
#if defined(DONTFLOAT_WITH_ARA)
    /** Сажает дорожку \a trackIndex в документ ARA и качает обновления. */
    void startAraSessionIfSupported(int trackIndex);
    /** Убирает дорожку из документа ARA (перед выгрузкой её плагина). */
    void releaseAraTrack(int trackIndex);
    /** Документ для фабрики \a factory; поднимает его при первом обращении. */
    Dontfloat::PluginTester::AraHostDocument* araDocumentFor(const void* factory,
                                                            QString* error);
#endif
    void showPluginMessage(const QString& text, bool isError)
    {
        showPluginMessage(activeTrack_, text, isError);
    }
    void showPluginMessage(int trackIndex, const QString& text, bool isError);
    void updateTransportUi();
    void updateWindowTitle();
    /** Ставит в списки формат/редакцию активной дорожки без перезагрузки. */
    void syncPluginCombos();

    TrackState& track() { return tracks_[activeTrack_]; }
    const TrackState& track() const { return tracks_[activeTrack_]; }

    PluginFormat currentFormat() const;
    PluginProduct currentProduct() const;

    QToolButton* openButton_ = nullptr;
    QComboBox* trackCombo_ = nullptr;   ///< «1/2» из макета: выбор дорожки
    QComboBox* formatCombo_ = nullptr;
    QComboBox* productCombo_ = nullptr;
    QLineEdit* bpmEdit_ = nullptr;      ///< темп хоста
    QComboBox* beatsCombo_ = nullptr;   ///< размер такта
    QToolButton* playButton_ = nullptr;
    QToolButton* stopButton_ = nullptr;
    PlaybackBar* playbackBar_ = nullptr;
    QWidget* pluginFrame_ = nullptr;    ///< красная рамка
    QStackedWidget* pluginStack_ = nullptr;  ///< по странице на дорожку
    QTimer* positionTimer_ = nullptr;
#if defined(DONTFLOAT_WITH_ARA)
    /** Качает обновления модели ARA: разбор в плагине идёт в фоне. */
    QTimer* araPumpTimer_ = nullptr;
#endif

    TrackState tracks_[kTrackCount];
    int activeTrack_ = 0;
#if defined(DONTFLOAT_WITH_ARA)
    /** Документ ARA и фабрика (ARA::ARAFactory*), на которой он поднят. */
    struct AraSession {
        const void* factory = nullptr;
        std::unique_ptr<Dontfloat::PluginTester::AraHostDocument> document;
    };
    /**
     * Документы ARA окна. Объявлены **после** tracks_ намеренно: при разрушении
     * окна документы уходят первыми, пока экземпляры плагинов ещё живы, —
     * закрытие документа зовёт плагин.
     */
    std::vector<AraSession> araSessions_;
#endif
    Player player_;

    int sampleRate_ = 44100;
    bool autoPlay_ = false;
    /** Защита от повторного входа в reloadPlugin из вложенного цикла событий. */
    bool reloadingPlugin_ = false;
};

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_WINDOW_H
