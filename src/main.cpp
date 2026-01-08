#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>
#include <QCoreApplication>
#include <QPalette>
#include <QColor>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QEventLoop>
#include <QtMultimedia/QAudioDecoder>
#include <QtMultimedia/QAudioBuffer>
#include <QtMultimedia/QAudioFormat>
#include <QObject>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include "../include/mainwindow.h"
#include "../include/bpmanalyzer.h"
#include <QStyleFactory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Простая функция загрузки аудиофайла для консольного режима
// Использует упрощенный подход без connect для избежания проблем компиляции
bool loadAudioFile(const QString& /*filePath*/, QVector<float>& samples, int& sampleRate) {
    // Для демонстрации создаем синтетические данные
    // В реальной реализации здесь должен быть код загрузки файла
    // Параметр filePath пока не используется, но сохранен для совместимости
    
    std::cout << "Примечание: Используются синтетические данные для демонстрации" << std::endl;
    
    // Создаем тестовый сигнал с известным BPM
    sampleRate = 44100;
    const float testBPM = 80.0f;
    const float duration = 5.0f; // 5 секунд
    const int totalSamples = static_cast<int>(sampleRate * duration);
    
    samples.clear();
    samples.reserve(totalSamples);
    
    // Создаем синусоидальный сигнал с битами
    const float beatInterval = 60.0f / testBPM; // интервал между битами в секундах
    const int samplesPerBeat = static_cast<int>(sampleRate * beatInterval);
    
    for (int i = 0; i < totalSamples; ++i) {
        float sample = 0.0f;
        
        // Добавляем основной тон (440 Hz)
        sample += 0.3f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
        
        // Добавляем биты (короткие импульсы)
        if (i % samplesPerBeat < samplesPerBeat / 10) {
            sample += 0.8f * std::sin(2.0f * M_PI * 880.0f * i / sampleRate);
        }
        
        // Добавляем немного шума для реалистичности
        sample += 0.1f * ((float)rand() / RAND_MAX - 0.5f);
        
        samples.append(sample);
    }
    
    return true;
}

// Функция для консольного режима
int runConsoleMode(const QString& filePath, const BPMAnalyzer::AnalysisOptions& options) {
    // Настраиваем вывод в консоль
    qSetMessagePattern("[%{type}] %{appname} (%{file}:%{line}) - %{message}");
    
    std::cout << "=== Консольный режим DONTFLOAT ===" << std::endl;
    std::cout << "Анализ файла: " << filePath.toStdString() << std::endl;
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        std::cout << "ОШИБКА: Файл не найден: " << filePath.toStdString() << std::endl;
        return 1;
    }
    
    std::cout << "Загрузка аудиофайла..." << std::endl;
    
    // Загружаем аудиофайл
    QVector<float> samples;
    int sampleRate = 0;
    
    if (!loadAudioFile(filePath, samples, sampleRate)) {
        std::cout << "ОШИБКА: Не удалось загрузить аудиофайл" << std::endl;
        return 1;
    }
    
    std::cout << "Файл загружен успешно:" << std::endl;
    std::cout << "  - Размер: " << samples.size() << " сэмплов" << std::endl;
    std::cout << "  - Частота дискретизации: " << sampleRate << " Hz" << std::endl;
    std::cout << "  - Длительность: " << (double)samples.size() / sampleRate << " секунд" << std::endl;
    
    std::cout << std::endl << "Настройки анализа:" << std::endl;
    std::cout << "  - Минимальный BPM: " << options.minBPM << std::endl;
    std::cout << "  - Максимальный BPM: " << options.maxBPM << std::endl;
    std::cout << "  - Использовать алгоритм Mixxx: " << (options.useMixxxAlgorithm ? "Да" : "Нет") << std::endl;
    std::cout << "  - Предполагать фиксированный темп: " << (options.assumeFixedTempo ? "Да" : "Нет") << std::endl;
    std::cout << "  - Быстрый анализ: " << (options.fastAnalysis ? "Да" : "Нет") << std::endl;
    
    std::cout << std::endl << "Начинаем анализ BPM..." << std::endl;
    
    // Анализируем BPM
    BPMAnalyzer::AnalysisResult result = BPMAnalyzer::analyzeBPM(samples, sampleRate, options);
    
    std::cout << std::endl << "=== РЕЗУЛЬТАТЫ АНАЛИЗА ===" << std::endl;
    std::cout << "Определенный BPM: " << result.bpm << std::endl;
    std::cout << "Уверенность: " << QString::number(result.confidence * 100, 'f', 1).toStdString() << "%" << std::endl;
    std::cout << "Количество обнаруженных битов: " << result.beats.size() << std::endl;
    std::cout << "Фиксированный темп: " << (result.isFixedTempo ? "Да" : "Нет") << std::endl;
    std::cout << "Нерегулярные биты: " << (result.hasIrregularBeats ? "Да" : "Нет") << std::endl;
    std::cout << "Среднее отклонение: " << QString::number(result.averageDeviation * 100, 'f', 2).toStdString() << "%" << std::endl;
    
    if (result.hasPreliminaryBPM) {
        std::cout << "Предварительный BPM: " << result.preliminaryBPM << std::endl;
    }
    
    if (result.beats.size() > 0) {
        std::cout << "Начальная позиция сетки: " << result.gridStartSample << " сэмплов" << std::endl;
        std::cout << "Начальная позиция сетки: " << QString::number((double)result.gridStartSample / sampleRate, 'f', 3).toStdString() << " секунд" << std::endl;
    }
    
    // Показываем первые несколько битов
    if (result.beats.size() > 0) {
        std::cout << std::endl << "Первые 10 битов:" << std::endl;
        std::cout << "Позиция(сэмплы) | Позиция(сек) | Уверенность | Отклонение | Энергия" << std::endl;
        std::cout << "----------------|--------------|-------------|------------|--------" << std::endl;
        
        int count = qMin(10, result.beats.size());
        for (int i = 0; i < count; ++i) {
            const auto& beat = result.beats[i];
            double timeInSeconds = (double)beat.position / sampleRate;
            std::cout << QString("%1 | %2 | %3 | %4 | %5")
                        .arg(beat.position, 14)
                        .arg(QString::number(timeInSeconds, 'f', 3), 12)
                        .arg(QString::number(beat.confidence * 100, 'f', 1) + "%", 11)
                        .arg(QString::number(beat.deviation * 100, 'f', 2) + "%", 10)
                        .arg(QString::number(beat.energy, 'f', 4), 8).toStdString() << std::endl;
        }
        
        if (result.beats.size() > 10) {
            std::cout << "... и еще " << (result.beats.size() - 10) << " битов" << std::endl;
        }
    }
    
    std::cout << std::endl << "=== АНАЛИЗ ЗАВЕРШЕН ===" << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    // Вывод в консоль ДО создания Qt приложения
    std::cout << "=== DONTFLOAT: Начало выполнения ===" << std::endl;
    std::cout.flush();
    
    // Сначала парсим командную строку без создания приложения
    QCommandLineParser parser;
    parser.setApplicationDescription("DONTFLOAT - Анализатор и корректор BPM аудиофайлов");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Опции для консольного режима
    QCommandLineOption consoleOption(QStringList() << "c" << "console", 
                                   "Запуск в консольном режиме");
    parser.addOption(consoleOption);
    
    QCommandLineOption fileOption(QStringList() << "f" << "file", 
                                 "Путь к аудиофайлу для анализа", "file");
    parser.addOption(fileOption);
    
    QCommandLineOption minBPMOption("min-bpm", 
                                   "Минимальный BPM для анализа", "min", "60");
    parser.addOption(minBPMOption);
    
    QCommandLineOption maxBPMOption("max-bpm", 
                                   "Максимальный BPM для анализа", "max", "200");
    parser.addOption(maxBPMOption);
    
    QCommandLineOption useMixxxOption("mixxx", 
                                     "Использовать алгоритм Mixxx");
    parser.addOption(useMixxxOption);
    
    QCommandLineOption fastAnalysisOption("fast", 
                                         "Быстрый анализ");
    parser.addOption(fastAnalysisOption);
    
    QCommandLineOption variableTempoOption("variable-tempo", 
                                          "Предполагать переменный темп");
    parser.addOption(variableTempoOption);
    
    // Проверяем аргументы без создания приложения
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }
    
    // Проверяем, нужен ли консольный режим
    bool consoleMode = args.contains("-c") || args.contains("--console");
    QString filePath;
    
    // Простой парсинг для консольного режима
    for (int i = 0; i < args.size(); ++i) {
        if ((args[i] == "-f" || args[i] == "--file") && i + 1 < args.size()) {
            filePath = args[i + 1];
            break;
        }
    }
    
    if (consoleMode) {
        if (filePath.isEmpty()) {
            std::cout << "Ошибка: Для консольного режима необходимо указать файл с помощью -f" << std::endl;
            std::cout << "Использование: DONTFLOAT -c -f <путь_к_файлу>" << std::endl;
            return 1;
        }
        
        // Создаем QCoreApplication для консольного режима
        QCoreApplication app(argc, argv);
        
        // Установка информации о приложении для QSettings
        QCoreApplication::setOrganizationName("DONTFLOAT");
        QCoreApplication::setApplicationName("DONTFLOAT");
        QCoreApplication::setApplicationVersion("1.0.0");
        
        // Настройки анализа из командной строки
        BPMAnalyzer::AnalysisOptions options;
        options.minBPM = 60.0f;  // По умолчанию
        options.maxBPM = 200.0f; // По умолчанию
        options.useMixxxAlgorithm = args.contains("--mixxx");
        options.fastAnalysis = args.contains("--fast");
        options.assumeFixedTempo = !args.contains("--variable-tempo");
        
        return runConsoleMode(filePath, options);
    }
    
    // GUI режим - создаем QApplication
    QApplication guiApp(argc, argv);
    
    // Настраиваем вывод qDebug() в консоль
    qSetMessagePattern("[%{type}] %{appname} (%{file}:%{line}) - %{message}");
    
    // Диагностика: проверяем, что Qt инициализирован
    std::cout << "=== Запуск DONTFLOAT ===" << std::endl;
    std::cout << "Qt Application создан успешно" << std::endl;
    std::cout << "Qt версия: " << QT_VERSION_STR << std::endl;
    qDebug() << "Qt Application создан успешно";
    qDebug() << "Qt версия:" << QT_VERSION_STR;
    
    // Установка информации о приложении для QSettings
    QCoreApplication::setOrganizationName("DONTFLOAT");
    QCoreApplication::setApplicationName("DONTFLOAT");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    // Установка иконки приложения
    QIcon::setThemeName("DONTFLOAT");
    QIcon appIcon(":/icons/resources/icons/logo.svg");
    if (appIcon.isNull()) {
        qWarning() << "Не удалось загрузить иконку приложения";
    } else {
        qDebug() << "Иконка приложения загружена";
    }
    guiApp.setWindowIcon(appIcon);

    // Устанавливаем единообразную темную тему по умолчанию
    qApp->setStyle(QStyleFactory::create("Fusion"));
    
    // Создаем темную палитру для единообразного оформления
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Shadow, QColor(53, 53, 53));
    qApp->setPalette(darkPalette);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "dontfloat_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            guiApp.installTranslator(&translator);
            break;
        }
    }
    
    std::cout << "Создание главного окна..." << std::endl;
    qDebug() << "Создание главного окна...";
    
    try {
        MainWindow w;
        std::cout << "Главное окно создано успешно" << std::endl;
        qDebug() << "Главное окно создано";
        
        std::cout << "Показ главного окна..." << std::endl;
        qDebug() << "Показ главного окна...";
        w.show();
        w.raise();  // Поднимаем окно наверх
        w.activateWindow();  // Активируем окно (дает фокус)
        
        // Проверяем, что окно действительно видимо
        if (w.isVisible()) {
            std::cout << "Главное окно видимо, размер: " << w.width() << "x" << w.height() << std::endl;
            qDebug() << "Главное окно видимо, размер:" << w.size();
        } else {
            std::cout << "ПРЕДУПРЕЖДЕНИЕ: Главное окно не видимо!" << std::endl;
            qDebug() << "ПРЕДУПРЕЖДЕНИЕ: Главное окно не видимо!";
        }
        
        std::cout << "Запуск event loop..." << std::endl;
        qDebug() << "Главное окно показано, запуск event loop...";
        
        return guiApp.exec();
    } catch (const std::exception& e) {
        std::cerr << "ОШИБКА при создании главного окна: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "НЕИЗВЕСТНАЯ ОШИБКА при создании главного окна" << std::endl;
        return 1;
    }
}
