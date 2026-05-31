#include <QApplication>
#include <QIcon>
#include <QCoreApplication>
#include <QPalette>
#include <QColor>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLoggingCategory>
#include <QFileInfo>
#include <iostream>
#include "../include/mainwindow.h"
#include "../include/bpmanalyzer.h"
#include "../include/audiofileservice.h"
#include <QStyleFactory>

Q_LOGGING_CATEGORY(lcStartup, "dontfloat.startup")

#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>

// Приложение собрано как GUI (subsystem:windows). При запуске из cmd с перенаправлением
// (> file | pipe) стандартные потоки уже подключены и работают. А вот при запуске из
// терминала без перенаправления у GUI-процесса нет своей консоли — тогда подключаемся к
// консоли родителя (cmd/powershell), чтобы консольный режим (-c) печатал результат.
static void initConsoleIO()
{
    auto isConnected = [](DWORD nStd) {
        const DWORD type = GetFileType(GetStdHandle(nStd));
        return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE || type == FILE_TYPE_CHAR;
    };

    const bool outConnected = isConnected(STD_OUTPUT_HANDLE);
    const bool errConnected = isConnected(STD_ERROR_HANDLE);
    if (outConnected && errConnected)
        return;

    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;

    FILE* f = nullptr;
    if (!outConnected) freopen_s(&f, "CONOUT$", "w", stdout);
    if (!errConnected) freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    std::ios::sync_with_stdio(true);
}
#endif

namespace {

void configureStartupLogging(bool verbose)
{
    if (verbose) {
        QLoggingCategory::setFilterRules(QStringLiteral("dontfloat.startup.debug=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral("dontfloat.startup.debug=false"));
    }
}

// Декодирует аудиофайл в моно-сигнал для анализа BPM (консольный режим).
bool loadAudioFile(const QString& filePath, QVector<float>& samples, int& sampleRate)
{
    const AudioFileService::DecodeResult res = AudioFileService::decode(filePath);
    if (!res.ok) {
        if (!res.error.isEmpty())
            std::cout << "ОШИБКА декодирования: " << res.error.toStdString() << std::endl;
        return false;
    }
    samples = AudioFileService::toMono(res.channels);
    sampleRate = res.sampleRate;
    return !samples.isEmpty();
}

int runConsoleMode(const QString& filePath, const BPMAnalyzer::AnalysisOptions& options)
{
    std::cout << "=== Консольный режим DONTFLOAT ===" << std::endl;
    std::cout << "Анализ файла: " << filePath.toStdString() << std::endl;

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        std::cout << "ОШИБКА: Файл не найден: " << filePath.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Загрузка аудиофайла..." << std::endl;

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

    const BPMAnalyzer::AnalysisResult result = BPMAnalyzer::analyzeBPM(samples, sampleRate, options);

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

    if (!result.beats.isEmpty()) {
        std::cout << "Начальная позиция сетки: " << result.gridStartSample << " сэмплов" << std::endl;
        std::cout << "Начальная позиция сетки: "
                  << QString::number((double)result.gridStartSample / sampleRate, 'f', 3).toStdString()
                  << " секунд" << std::endl;
    }

    if (!result.beats.isEmpty()) {
        std::cout << std::endl << "Первые 10 битов:" << std::endl;
        std::cout << "Позиция(сэмплы) | Позиция(сек) | Уверенность | Отклонение | Энергия" << std::endl;
        std::cout << "----------------|--------------|-------------|------------|--------" << std::endl;

        const int count = qMin(10, result.beats.size());
        for (int i = 0; i < count; ++i) {
            const auto& beat = result.beats[i];
            const double timeInSeconds = (double)beat.position / sampleRate;
            std::cout << QString("%1 | %2 | %3 | %4 | %5")
                             .arg(beat.position, 14)
                             .arg(QString::number(timeInSeconds, 'f', 3), 12)
                             .arg(QString::number(beat.confidence * 100, 'f', 1) + "%", 11)
                             .arg(QString::number(beat.deviation * 100, 'f', 2) + "%", 10)
                             .arg(QString::number(beat.energy, 'f', 4), 8)
                             .toStdString()
                      << std::endl;
        }

        if (result.beats.size() > 10) {
            std::cout << "... и еще " << (result.beats.size() - 10) << " битов" << std::endl;
        }
    }

    std::cout << std::endl << "=== АНАЛИЗ ЗАВЕРШЕН ===" << std::endl;
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }

    const bool consoleMode = args.contains("-c") || args.contains("--console");
    const bool verboseStartup = args.contains("--verbose") || args.contains("-v");

    QString filePath;
    for (int i = 0; i < args.size(); ++i) {
        if ((args[i] == "-f" || args[i] == "--file") && i + 1 < args.size()) {
            filePath = args[i + 1];
            break;
        }
    }

    if (consoleMode) {
#ifdef Q_OS_WIN
        initConsoleIO();
#endif
        if (filePath.isEmpty()) {
            std::cout << "Ошибка: Для консольного режима необходимо указать файл с помощью -f" << std::endl;
            std::cout << "Использование: DONTFLOAT -c -f <путь_к_файлу>" << std::endl;
            return 1;
        }

        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName("DONTFLOAT");
        QCoreApplication::setApplicationName("DONTFLOAT");
        QCoreApplication::setApplicationVersion("0.0.0.1");

        BPMAnalyzer::AnalysisOptions options;
        options.minBPM = 60.0f;
        options.maxBPM = 200.0f;
        options.useMixxxAlgorithm = !args.contains("--simple");
        options.fastAnalysis = args.contains("--fast");
        options.assumeFixedTempo = !args.contains("--variable-tempo");

        return runConsoleMode(filePath, options);
    }

    QApplication guiApp(argc, argv);
    configureStartupLogging(verboseStartup);

    QCoreApplication::setOrganizationName("DONTFLOAT");
    QCoreApplication::setApplicationName("DONTFLOAT");
    QCoreApplication::setApplicationVersion("0.0.0.1");

    qCDebug(lcStartup) << "Qt Application создан, версия" << QT_VERSION_STR;

    QIcon::setThemeName("DONTFLOAT");
    QIcon appIcon(":/icons/resources/icons/logo.svg");
    if (appIcon.isNull()) {
        qWarning() << "Не удалось загрузить иконку приложения";
    } else {
        qCDebug(lcStartup) << "Иконка приложения загружена";
    }
    guiApp.setWindowIcon(appIcon);

    qApp->setStyle(QStyleFactory::create("Fusion"));

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

    try {
        qCDebug(lcStartup) << "Создание главного окна...";
        MainWindow w;
        w.show();
        w.raise();
        w.activateWindow();

        if (w.isVisible()) {
            qCDebug(lcStartup) << "Главное окно видимо, размер:" << w.size();
        } else {
            qCWarning(lcStartup) << "Главное окно не видимо после show()";
        }

        qCDebug(lcStartup) << "Запуск event loop";
        return guiApp.exec();
    } catch (const std::exception& e) {
        qCritical() << "Ошибка при создании главного окна:" << e.what();
        return 1;
    } catch (...) {
        qCritical() << "Неизвестная ошибка при создании главного окна";
        return 1;
    }
}
