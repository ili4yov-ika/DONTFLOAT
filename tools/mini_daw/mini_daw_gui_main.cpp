// DONTFLOAT mini-DAW — GUI-хост плагинов по макету MARKDOWN/example_window_minidaw.svg.
//
// Одно окно на все девять комбинаций: формат и редакция выбираются в
// выпадающих списках, модуль плагина грузится в рантайме (как в настоящей DAW),
// его редактор живёт в панели с красной рамкой, трек прогоняется через
// process() и играет транспортом с кареткой.
//
// `--selftest` прогоняет тот же путь без окна: грузит трек и модуль плагина,
// встраивает редактор в скрытое нативное окно и стримит аудио через process().

#include <QtCore/QCommandLineParser>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <cstdio>

#include "audiofileservice.h"
#include "mini_daw_plugin_host.h"
#include "mini_daw_window.h"

namespace {

using Dontfloat::PluginTester::PluginFormat;
using Dontfloat::PluginTester::PluginProduct;

struct Selection {
    PluginFormat format = PluginFormat::Clap;
    PluginProduct product = PluginProduct::Full;
};

bool parseFormat(const QString& text, PluginFormat* out)
{
    const QString value = text.toLower();
    if (value == QLatin1String("clap")) { *out = PluginFormat::Clap; return true; }
    if (value == QLatin1String("vst3")) { *out = PluginFormat::Vst3; return true; }
    if (value == QLatin1String("lv2"))  { *out = PluginFormat::Lv2;  return true; }
    return false;
}

bool parseProduct(const QString& text, PluginProduct* out)
{
    const QString value = text.toLower();
    if (value == QLatin1String("full"))    { *out = PluginProduct::Full;    return true; }
    if (value == QLatin1String("scratch")) { *out = PluginProduct::Scratch; return true; }
    if (value == QLatin1String("pitcher")) { *out = PluginProduct::Pitcher; return true; }
    return false;
}

double rms(const QVector<float>& samples, int frames)
{
    if (frames <= 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (int i = 0; i < frames; ++i) {
        sum += double(samples[i]) * double(samples[i]);
    }
    return std::sqrt(sum / double(frames));
}

/**
 * Самопроверка без показа окна: декод трека → загрузка модуля → редактор в
 * скрытое нативное окно → прогон блоков через process().
 * @return 0 — всё прошло, иначе код ошибки.
 */
int runSelfTest(const Selection& selection, const QString& input, double maxSeconds)
{
    const QString formatName = Dontfloat::PluginTester::formatLabel(selection.format);
    std::printf("=== mini-DAW selftest: %s / %d ===\n",
                formatName.toLocal8Bit().constData(), int(selection.product));

    const auto decoded = AudioFileService::decode(input);
    if (!decoded.ok || decoded.channels.isEmpty()) {
        std::printf("[FAIL] декод %s: %s\n", input.toLocal8Bit().constData(),
                    decoded.error.toLocal8Bit().constData());
        return 2;
    }
    const int sampleRate = decoded.sampleRate > 0 ? decoded.sampleRate : 44100;
    QVector<float> left = decoded.channels.first();
    QVector<float> right = decoded.channels.size() > 1 ? decoded.channels[1] : left;
    int frames = int(std::min(left.size(), right.size()));
    if (maxSeconds > 0.0) {
        frames = std::min(frames, int(maxSeconds * sampleRate));
    }
    if (frames <= 0) {
        std::printf("[FAIL] в треке нет кадров\n");
        return 2;
    }
    std::printf("[ok] трек: %d Гц, %d кадров (%.2f с), RMS %.4f\n",
                sampleRate, frames, double(frames) / sampleRate, rms(left, frames));

    const QString path =
        Dontfloat::PluginTester::resolvePluginPath(selection.format, selection.product);
    std::printf("[..] модуль: %s\n", QFileInfo(path).absoluteFilePath().toLocal8Bit().constData());

    auto host = MiniDaw::createPluginHost(selection.format);
    if (!host) {
        std::printf("[FAIL] формат не поддержан в этой сборке\n");
        return 3;
    }
    QString error;
    if (!host->load(path, selection.product, sampleRate, 512, &error)) {
        std::printf("[FAIL] загрузка плагина: %s\n", error.toLocal8Bit().constData());
        return 3;
    }
    std::printf("[ok] плагин загружен: %s\n", host->displayName().toLocal8Bit().constData());

    // Редактор встраиваем в скрытое нативное окно — как это делает окно мини-DAW
    QWidget surface;
    surface.setAttribute(Qt::WA_NativeWindow, true);
    surface.resize(960, 640);
    QSize editorSize;
    if (host->embedEditor(surface.winId(), &editorSize, &error)) {
        std::printf("[ok] редактор встроен (%dx%d)\n", editorSize.width(), editorSize.height());
    } else {
        std::printf("[warn] редактор не открылся: %s\n", error.toLocal8Bit().constData());
    }

    for (int pos = 0; pos < frames; pos += 512) {
        const int n = std::min(512, frames - pos);
        host->process(left.data() + pos, right.data() + pos, n);
    }
    std::printf("[ok] прогнано %d кадров через process(), выход RMS %.4f\n",
                frames, rms(left, frames));

    host->unload();
    std::printf("=== selftest OK ===\n");
    std::fflush(stdout);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DONTFLOAT mini-DAW"));
    QApplication::setOrganizationName(QStringLiteral("DONTFLOAT"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("DONTFLOAT mini-DAW"));
    parser.addHelpOption();
    QCommandLineOption inputOption({ QStringLiteral("i"), QStringLiteral("input") },
                                   QStringLiteral("Audio file to load"),
                                   QStringLiteral("path"));
    QCommandLineOption formatOption(QStringLiteral("format"),
                                    QStringLiteral("clap | vst3 | lv2 (selftest)"),
                                    QStringLiteral("name"), QStringLiteral("clap"));
    QCommandLineOption productOption(QStringLiteral("product"),
                                     QStringLiteral("full | scratch | pitcher (selftest)"),
                                     QStringLiteral("name"), QStringLiteral("full"));
    QCommandLineOption secondsOption(QStringLiteral("seconds"),
                                     QStringLiteral("Cap processed length (selftest)"),
                                     QStringLiteral("s"), QStringLiteral("4"));
    QCommandLineOption selfTestOption(QStringLiteral("selftest"),
                                      QStringLiteral("Headless check: load plugin, embed editor, process"));
    parser.addOption(inputOption);
    parser.addOption(formatOption);
    parser.addOption(productOption);
    parser.addOption(secondsOption);
    parser.addOption(selfTestOption);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Audio file to load"));
    parser.process(app);

    QString input = parser.value(inputOption);
    if (input.isEmpty() && !parser.positionalArguments().isEmpty()) {
        input = parser.positionalArguments().first();
    }

    if (parser.isSet(selfTestOption)) {
        Selection selection;
        if (!parseFormat(parser.value(formatOption), &selection.format)
            || !parseProduct(parser.value(productOption), &selection.product)) {
            std::printf("[FAIL] неизвестный формат/редакция\n");
            return 1;
        }
        if (input.isEmpty()) {
            input = QStringLiteral("tests/midi/test_1.wav");
        }
        return runSelfTest(selection, input, parser.value(secondsOption).toDouble());
    }

    MiniDaw::Window window;
    // --format/--product работают и в окне: выбираем плагин до первой загрузки
    Selection selection;
    if (parseFormat(parser.value(formatOption), &selection.format)
        && parseProduct(parser.value(productOption), &selection.product)) {
        window.selectPlugin(selection.format, selection.product);
    }
    window.show();
    if (!input.isEmpty()) {
        // Через очередь событий: сначала поднимется плагин (таймер окна),
        // потом декодируется трек — иначе загрузка идёт во вложенном цикле
        QTimer::singleShot(0, &window, [&window, input]() { window.openAudio(input); });
    }
    return app.exec();
}
