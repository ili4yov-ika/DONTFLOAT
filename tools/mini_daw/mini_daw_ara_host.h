#ifndef MINI_DAW_ARA_HOST_H
#define MINI_DAW_ARA_HOST_H

/**
 * Мини-DAW в роли ARA-хоста.
 *
 * Обычный (не-ARA) путь отдаёт плагину звук блоками во время проигрывания.
 * По ARA хост вместо этого заводит документ и отдаёт плагину **весь файл**
 * дорожки в произвольный доступ: плагин разбирает его сразу, а разметку
 * (ноты, темп, тактовую сетку) хост и плагин видят в общей модели.
 *
 * Здесь реализованы пять интерфейсов хоста ARA и сборка минимального
 * документа: musical context (темп и размер такта из панели мини-DAW),
 * region sequence, audio source (загруженный WAV), audio modification и
 * playback region на всю длину дорожки.
 */

#include <QtCore/QString>
#include <QtCore/QVector>

#include <functional>
#include <memory>
#include <vector>

namespace ARA {
struct ARAFactory;
}

namespace Dontfloat::PluginTester {

/** Дорожка хоста, которую видит плагин через ARA. */
struct AraHostTrack {
    QVector<float> left;
    QVector<float> right;
    double sampleRate = 44100.0;
    double tempoBpm = 120.0;
    int beatsPerBar = 4;
    QString name;

    qint64 frameCount() const { return left.size(); }
    int channelCount() const { return right.isEmpty() ? 1 : 2; }
};

/**
 * Документ ARA поверх дорожек мини-DAW.
 *
 * Документ один на все дорожки — так же, как в настоящей DAW: у каждой дорожки
 * свои region sequence, audio source, modification и playback region, а
 * musical context (темп и размер такта) общий. Именно из-за общего документа
 * экземпляр плагина на второй дорожке видит ноты первой как референс.
 *
 * Объект держит модель живой, пока плагины к ней привязаны.
 */
class AraHostDocument {
public:
    AraHostDocument();
    ~AraHostDocument();

    AraHostDocument(const AraHostDocument&) = delete;
    AraHostDocument& operator=(const AraHostDocument&) = delete;

    /**
     * Поднимает пустой документ на фабрике плагина (musical context уже есть).
     * @return false с текстом в \a error, если фабрика не приняла документ.
     */
    bool open(const ARA::ARAFactory* factory, QString* error);

    /** Поднимает документ и сразу кладёт в него одну дорожку. */
    bool open(const ARA::ARAFactory* factory, const AraHostTrack& track, QString* error);

    /**
     * Добавляет дорожку в **уже открытый** документ и просит её разбор.
     *
     * Документ у дорожек мини-DAW один, как и в настоящей DAW: только так
     * экземпляр плагина на второй дорожке видит ноты первой (референс).
     * @return индекс дорожки в документе или -1
     */
    int addTrack(const AraHostTrack& track);

    /**
     * Убирает дорожку из документа (индексы остальных не меняются).
     *
     * Звать **до** уничтожения экземпляра плагина этой дорожки: клип
     * снимается с его роли воспроизведения, а это вызов в плагин.
     */
    void removeTrack(int index);

    /** Сколько дорожек заведено в документе. */
    int trackCount() const;

    /** Ссылка на document controller — её передают экземпляру плагина. */
    void* documentControllerRef() const;

    /** Роли, которые мини-DAW назначает экземпляру плагина. */
    static unsigned long long knownRoles();
    static unsigned long long assignedRoles();

    /**
     * Привязывает клип дорожки \a trackIndex к ролям экземпляра плагина.
     * Без этого шага плагин не знает, с каким аудиоисточником он работает:
     * в ARA связь «экземпляр → дорожка» задаётся именно через playback region.
     * @param plugInExtensionInstance ARAPlugInExtensionInstance от плагина
     */
    void bindInstance(int trackIndex, const void* plugInExtensionInstance);

    /**
     * Снимает клип дорожки \a trackIndex с ролей её экземпляра плагина.
     *
     * Обязательный шаг перед уничтожением самого экземпляра: пока клип
     * висит в его роли, хост при закрытии документа полезет в уже мёртвый
     * объект. Повторный вызов безвреден.
     */
    void unbindInstance(int trackIndex);

    /** Прокачивает обновления модели: разбор в плагине идёт в фоне. */
    void pumpModelUpdates();

    /** Разбор дорожки завершён (плагин сообщил об этом хосту). */
    bool analysisCompleted() const;

    /** Сколько нот плагин отдал хосту по дорожке \a trackIndex. */
    int readNoteCount(int trackIndex = 0) const;

    /**
     * Выделяет в редакторе экземпляра \a viewTrackIndex клип **чужой**
     * дорожки \a selectedTrackIndex.
     *
     * Так ведёт себя настоящая DAW: выбор клипов общий на проект, и окно
     * плагина первой дорожки получает выделение, сделанное на второй.
     * Экземпляр обязан остаться при своей дорожке.
     */
    void selectClipOfTrack(int viewTrackIndex, int selectedTrackIndex);

    /** Сколько раз плагин просил хост начать воспроизведение. */
    int transportStartRequests() const;
    /** Сколько раз плагин просил хост остановиться. */
    int transportStopRequests() const;
    /** Последняя позиция (секунды проекта), о которой просил плагин. */
    double lastRequestedPlaybackPosition() const;

    /** Закрывает документ: сначала объекты модели, потом контроллер. */
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Dontfloat::PluginTester

#endif // MINI_DAW_ARA_HOST_H
