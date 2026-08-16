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
class QTimer;
class QToolButton;
QT_END_NAMESPACE

namespace MiniDaw {

/** Полоска плейбека: время слева/справа, линия трека и красная каретка. */
class PlaybackBar : public QWidget {
    Q_OBJECT

public:
    explicit PlaybackBar(QWidget* parent = nullptr);

    void setDuration(qint64 frames, int sampleRate);
    void setPosition(qint64 frames);
    qint64 position() const { return position_; }

signals:
    /** Пользователь ткнул в полоску — перемотка. */
    void seekRequested(qint64 frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    qint64 frameAtX(int x) const;
    static QString formatTime(qint64 frames, int sampleRate);

    qint64 totalFrames_ = 0;
    qint64 position_ = 0;
    int sampleRate_ = 44100;
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
    void tickPosition();

private:
    void buildUi();
    void applyStyle();
    /** Перезагружает выбранный плагин и встраивает его редактор в панель. */
    void reloadPlugin();
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
    int sampleRate_ = 44100;
};

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_WINDOW_H
