#include <QtTest/QtTest>
#include <QVector>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <QtMultimedia/QAudioDecoder>
#include <QtMultimedia/QAudioFormat>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include "../include/bpmanalyzer.h"

class BPMAnalyzerTest : public QObject
{
    Q_OBJECT

public:
    BPMAnalyzerTest() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Тест анализа BPM на реальных файлах из source4test
    void testAnalyzeBPMFromSourceFiles();

private:
    QString getTestDataPath(const QString& filename);
    QVector<QVector<float>> loadAudioFile(const QString& filePath, int& sampleRate);
};

void BPMAnalyzerTest::initTestCase()
{
    qDebug() << "Инициализация тестов анализатора BPM";
}

void BPMAnalyzerTest::cleanupTestCase()
{
    qDebug() << "Завершение тестов анализатора BPM";
}

void BPMAnalyzerTest::init()
{
}

void BPMAnalyzerTest::cleanup()
{
}

QString BPMAnalyzerTest::getTestDataPath(const QString& filename)
{
    // Получаем путь к тестовым файлам
    // Рабочая директория установлена в CMakeLists.txt как ${CMAKE_CURRENT_SOURCE_DIR}
    QString testFilePath = QString("tests/source4test/%1").arg(filename);
    if (QFileInfo::exists(testFilePath)) {
        return QFileInfo(testFilePath).absoluteFilePath();
    }
    
    // Пытаемся найти относительно текущей директории
    QDir currentDir = QDir::current();
    testFilePath = currentDir.absoluteFilePath(QString("tests/source4test/%1").arg(filename));
    if (QFileInfo::exists(testFilePath)) {
        return testFilePath;
    }
    
    // Пытаемся найти в родительской директории
    currentDir.cdUp();
    testFilePath = currentDir.absoluteFilePath(QString("tests/source4test/%1").arg(filename));
    if (QFileInfo::exists(testFilePath)) {
        return testFilePath;
    }
    
    qWarning() << "Тестовый файл не найден:" << filename;
    return QString();
}

QVector<QVector<float>> BPMAnalyzerTest::loadAudioFile(const QString& filePath, int& sampleRate)
{
    QVector<QVector<float>> channels;
    sampleRate = 0;
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qWarning() << "Файл не существует:" << filePath;
        return channels;
    }
    
    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(filePath));
    
    // Настраиваем формат аудио
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(format);
    
    QVector<float> leftChannel;
    QVector<float> rightChannel;
    
    QEventLoop loop;
    bool errorOccurred = false;
    bool finished = false;
    
    // Обработка ошибок
    connect(&decoder, static_cast<void(QAudioDecoder::*)(QAudioDecoder::Error)>(&QAudioDecoder::error),
        [&](QAudioDecoder::Error error) {
            if (error != QAudioDecoder::NoError) {
                errorOccurred = true;
                qWarning() << "Ошибка декодирования:" << error;
                loop.quit();
            }
        });
    
    // Обработка готовности буфера
    connect(&decoder, &QAudioDecoder::bufferReady, [&]() {
        QAudioBuffer buffer = decoder.read();
        if (buffer.isValid()) {
            const float* data = buffer.constData<float>();
            int frameCount = buffer.frameCount();
            int channelCount = buffer.format().channelCount();
            
            if (sampleRate == 0) {
                sampleRate = buffer.format().sampleRate();
            }
            
            for (int i = 0; i < frameCount; ++i) {
                if (channelCount >= 1) {
                    leftChannel.append(data[i * channelCount]);
                }
                if (channelCount >= 2) {
                    rightChannel.append(data[i * channelCount + 1]);
                } else if (channelCount == 1) {
                    rightChannel.append(data[i * channelCount]);
                }
            }
        }
    });
    
    // Обработка завершения
    connect(&decoder, &QAudioDecoder::finished, [&]() {
        finished = true;
        loop.quit();
    });
    
    // Таймаут на случай зависания
    QTimer::singleShot(30000, [&]() {
        if (!finished) {
            qWarning() << "Таймаут загрузки файла";
            errorOccurred = true;
            loop.quit();
        }
    });
    
    // Запускаем декодирование
    decoder.start();
    
    if (decoder.error() != QAudioDecoder::NoError) {
        qWarning() << "Не удалось запустить декодер:" << decoder.error();
        return channels;
    }
    
    // Ждем завершения
    loop.exec();
    
    if (errorOccurred) {
        qWarning() << "Ошибка при загрузке файла:" << filePath;
        return channels;
    }
    
    if (leftChannel.isEmpty() && rightChannel.isEmpty()) {
        qWarning() << "Не удалось загрузить аудиоданные из файла:" << filePath;
        return channels;
    }
    
    // Если только один канал, дублируем его
    if (rightChannel.isEmpty() && !leftChannel.isEmpty()) {
        rightChannel = leftChannel;
    }
    if (leftChannel.isEmpty() && !rightChannel.isEmpty()) {
        leftChannel = rightChannel;
    }
    
    channels.append(leftChannel);
    channels.append(rightChannel);
    
    qDebug() << "Загружено аудио:" << filePath;
    qDebug() << "  Каналов:" << channels.size();
    qDebug() << "  Сэмплов на канал:" << (channels.isEmpty() ? 0 : channels[0].size());
    qDebug() << "  Sample rate:" << sampleRate;
    
    return channels;
}

void BPMAnalyzerTest::testAnalyzeBPMFromSourceFiles()
{
    // Список тестовых файлов
    QStringList testFiles = {
        "example_C80BPM.mp3",
        "example_V80BPM.mp3"
    };
    
    for (const QString& filename : testFiles) {
        QString testFile = getTestDataPath(filename);
        
        if (testFile.isEmpty()) {
            qWarning() << "Пропуск файла (не найден):" << filename;
            continue;
        }
        
        qDebug() << "\n=== Тестирование файла:" << filename << "===";
        
        // Загружаем аудиофайл
        int sampleRate = 0;
        QVector<QVector<float>> channels = loadAudioFile(testFile, sampleRate);
        
        if (channels.isEmpty() || sampleRate == 0) {
            QSKIP(QString("Не удалось загрузить файл %1, пропускаем тест").arg(filename).toUtf8().constData());
            continue;
        }
        
        // Создаем моно канал для анализа (усредняем стерео)
        QVector<float> monoChannel;
        int minLength = qMin(channels[0].size(), channels.size() > 1 ? channels[1].size() : channels[0].size());
        monoChannel.reserve(minLength);
        
        for (int i = 0; i < minLength; ++i) {
            float left = channels[0][i];
            float right = (channels.size() > 1) ? channels[1][i] : left;
            monoChannel.append((left + right) / 2.0f);
        }
        
        // Тест 1: Анализ с упрощенным алгоритмом
        {
            qDebug() << "\n--- Тест 1: Упрощенный алгоритм ---";
            BPMAnalyzer::AnalysisOptions options;
            options.useMixxxAlgorithm = false;
            options.assumeFixedTempo = filename.contains("_C"); // C = Constant tempo
            
            BPMAnalyzer::AnalysisResult result = BPMAnalyzer::analyzeBPM(monoChannel, sampleRate, options);
            
            QVERIFY2(result.bpm > 0, QString("BPM должен быть больше 0 для файла %1").arg(filename).toUtf8().constData());
            QVERIFY2(result.bpm < 300, QString("BPM должен быть разумным (< 300) для файла %1, получено %2").arg(filename).arg(result.bpm).toUtf8().constData());
            QVERIFY2(result.beats.size() > 0, QString("Должно быть обнаружено хотя бы несколько битов для файла %1").arg(filename).toUtf8().constData());
            QVERIFY2(result.confidence >= 0.0f && result.confidence <= 1.0f, 
                     QString("Уверенность должна быть в диапазоне [0, 1] для файла %1").arg(filename).toUtf8().constData());
            
            qDebug() << "  Обнаруженный BPM:" << result.bpm;
            qDebug() << "  Количество битов:" << result.beats.size();
            qDebug() << "  Уверенность:" << result.confidence;
            
            // Для файлов с постоянным темпом (C80BPM) проверяем, что BPM близок к 80
            if (filename.contains("_C80BPM")) {
                float expectedBPM = 80.0f;
                float tolerance = expectedBPM * 0.3f; // 30% допуск
                QVERIFY2(qAbs(result.bpm - expectedBPM) <= tolerance || 
                         qAbs(result.bpm - expectedBPM * 2.0f) <= tolerance || // Может быть обнаружена двойная скорость
                         qAbs(result.bpm - expectedBPM / 2.0f) <= tolerance,  // Или половина скорости
                         QString("BPM для файла с постоянным темпом %1: ожидалось около %2, получено %3")
                         .arg(filename).arg(expectedBPM).arg(result.bpm).toUtf8().constData());
            }
        }
        
        // Тест 2: Анализ с алгоритмом Mixxx (если доступен)
        #ifdef USE_MIXXX_QM_DSP
        {
            qDebug() << "\n--- Тест 2: Алгоритм Mixxx ---";
            BPMAnalyzer::AnalysisOptions options;
            options.useMixxxAlgorithm = true;
            options.assumeFixedTempo = filename.contains("_C");
            
            BPMAnalyzer::AnalysisResult result = BPMAnalyzer::analyzeBPM(monoChannel, sampleRate, options);
            
            QVERIFY2(result.bpm > 0, QString("BPM должен быть больше 0 (Mixxx) для файла %1").arg(filename).toUtf8().constData());
            QVERIFY2(result.bpm < 300, QString("BPM должен быть разумным (< 300) (Mixxx) для файла %1, получено %2").arg(filename).arg(result.bpm).toUtf8().constData());
            QVERIFY2(result.beats.size() > 0, QString("Должно быть обнаружено хотя бы несколько битов (Mixxx) для файла %1").arg(filename).toUtf8().constData());
            
            qDebug() << "  Обнаруженный BPM (Mixxx):" << result.bpm;
            qDebug() << "  Количество битов (Mixxx):" << result.beats.size();
            qDebug() << "  Уверенность (Mixxx):" << result.confidence;
            
            // Для файлов с постоянным темпом проверяем точность
            if (filename.contains("_C80BPM")) {
                float expectedBPM = 80.0f;
                float tolerance = expectedBPM * 0.25f; // 25% допуск для Mixxx
                QVERIFY2(qAbs(result.bpm - expectedBPM) <= tolerance || 
                         qAbs(result.bpm - expectedBPM * 2.0f) <= tolerance ||
                         qAbs(result.bpm - expectedBPM / 2.0f) <= tolerance,
                         QString("BPM для файла с постоянным темпом %1 (Mixxx): ожидалось около %2, получено %3")
                         .arg(filename).arg(expectedBPM).arg(result.bpm).toUtf8().constData());
            }
        }
        #else
        {
            qDebug() << "\n--- Тест 2: Алгоритм Mixxx (недоступен) ---";
            qDebug() << "  Mixxx библиотеки не подключены, пропускаем тест";
        }
        #endif
    }
}

QTEST_MAIN(BPMAnalyzerTest)
#include "bpm_analyzer_test.moc"

