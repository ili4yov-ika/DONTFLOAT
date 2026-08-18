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
 * Документ ARA поверх одной дорожки мини-DAW.
 * Объект держит модель живой, пока плагин к ней привязан.
 */
class AraHostDocument {
public:
    AraHostDocument();
    ~AraHostDocument();

    AraHostDocument(const AraHostDocument&) = delete;
    AraHostDocument& operator=(const AraHostDocument&) = delete;

    /**
     * Поднимает документ на фабрике плагина и добавляет дорожку.
     * @return false с текстом в \a error, если фабрика не приняла документ.
     */
    bool open(const ARA::ARAFactory* factory, const AraHostTrack& track, QString* error);

    /** Ссылка на document controller — её передают экземпляру плагина. */
    void* documentControllerRef() const;

    /** Роли, которые мини-DAW назначает экземпляру плагина. */
    static unsigned long long knownRoles();
    static unsigned long long assignedRoles();

    /** Прокачивает обновления модели: разбор в плагине идёт в фоне. */
    void pumpModelUpdates();

    /** Разбор дорожки завершён (плагин сообщил об этом хосту). */
    bool analysisCompleted() const;

    /** Сколько нот плагин отдал хосту (через контент-ридер ARA). */
    int readNoteCount() const;

    /** Закрывает документ: сначала объекты модели, потом контроллер. */
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Dontfloat::PluginTester

#endif // MINI_DAW_ARA_HOST_H
