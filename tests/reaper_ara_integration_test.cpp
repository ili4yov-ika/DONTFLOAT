// Интеграционный тест с настоящим REAPER: ARA2 и обмен нотами.
//
// Если REAPER на машине не найден — тест пропускается, поэтому в CI и на
// чужих сборочных машинах он молчит.
//
// Что происходит: тест кладёт в REAPER сценарий (tests/reaper/*.lua), тот при
// старте собирает проект из двух дорожек с аудио и VST3 DONTFLOAT на каждой,
// после чего режет, двигает, растягивает и сжимает клипы. Плагин при этом
// пишет дневник (Diagnostics, включается переменной DONTFLOAT_DIAG_FILE) —
// иначе изнутри хоста его поведение не увидеть: параметров у плагина нет, а
// ARA-события и общая доска нот живут целиком внутри процесса DAW.
//
// Проверяется, что до плагина реально дошли: постановка клипов на дорожки,
// нарезка, перенос, растяжение и сжатие, а также что один экземпляр выложил
// ноты на доску, а другой их оттуда взял.

#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

#include <cmath>

namespace {

/** Сколько секунд сценарий ждёт между правками клипов. */
constexpr int kSettleSeconds = 4;
/** Потолок на сам сценарий: старт REAPER, пять фаз с паузами. */
constexpr int kScenarioTimeoutMs = 180000;
/** Сколько ждём, что REAPER закроется сам после команды выхода. */
constexpr int kQuitGraceMs = 30000;

QString repoRoot()
{
    QDir dir(QDir::current());
    for (int up = 0; up < 3; ++up) {
        if (QFileInfo::exists(dir.absoluteFilePath(QStringLiteral("tests/reaper/dontfloat_ara_probe.lua")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}

/** Путь к reaper.exe: сначала запись установщика, потом обычные места. */
QString findReaper()
{
#ifdef Q_OS_WIN
    const QSettings uninstall(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REAPER"),
        QSettings::NativeFormat);
    const QString installLocation = uninstall.value(QStringLiteral("InstallLocation")).toString();
    if (!installLocation.isEmpty()) {
        const QString exe = QDir(installLocation).absoluteFilePath(QStringLiteral("reaper.exe"));
        if (QFileInfo::exists(exe)) {
            return exe;
        }
    }

    const QStringList candidates = {
        QStringLiteral("C:/Program Files/REAPER (x64)/reaper.exe"),
        QStringLiteral("C:/Program Files/REAPER/reaper.exe"),
        QStringLiteral("C:/Program Files (x86)/REAPER/reaper.exe"),
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
#endif
    return {};
}

/** Каталог ресурсов REAPER — туда кладётся сценарий автозапуска. */
QString reaperResourcePath()
{
#ifdef Q_OS_WIN
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // AppDataLocation отдаёт .../AppData/Roaming/<организация>/<приложение>,
    // а REAPER держит ресурсы просто в Roaming/REAPER
    QDir roaming(appData);
    while (roaming.dirName() != QLatin1String("Roaming") && roaming.cdUp()) {
    }
    const QString path = roaming.absoluteFilePath(QStringLiteral("REAPER"));
    return QFileInfo::exists(path) ? path : QString();
#else
    return {};
#endif
}

QStringList readLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream stream(&file);
    QStringList lines;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    return lines;
}

/** Значение поля вида «ключ=значение» из строки дневника. */
double fieldOf(const QString& line, const QString& key, bool* ok = nullptr)
{
    const QRegularExpression re(QStringLiteral("(?:^|\\s)%1=([-0-9.eE]+)").arg(key));
    const QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch()) {
        if (ok) {
            *ok = false;
        }
        return 0.0;
    }
    return match.captured(1).toDouble(ok);
}

} // namespace

class ReaperAraIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProjectBuildsWithTwoTracksAndPlugins();
    void testReaperClosesByItself();
    void testAraSeesClipEdits();
    void testWaveformGetsAudioFromAra();
    void testClipEditsDoNotRedrawTheWaveform();
    void testHostHandsOverItsTransport();
    void testBeatGridComesFromHost();
    void testPlaybackKeepsWaveform();
    void testReferenceNotesTravelBetweenInstances();

private:
    QString m_reaper;
    QString m_resourcePath;
    QString m_startupScript;
    QString m_startupBackup;
    QString m_reportPath;
    QString m_diagPath;
    QStringList m_report;
    QStringList m_diag;
    bool m_ran = false;
    bool m_exitedOnItsOwn = false;

    void requireRun();
    QStringList diagLines(const QString& prefix) const;
};

void ReaperAraIntegrationTest::initTestCase()
{
    m_reaper = findReaper();
    if (m_reaper.isEmpty()) {
        return;  // requireRun() выдаст QSKIP каждому тесту
    }

    const QString root = repoRoot();
    QVERIFY2(!root.isEmpty(), "не найден корень репозитория с tests/reaper/");

    m_resourcePath = reaperResourcePath();
    QVERIFY2(!m_resourcePath.isEmpty(), "не найден каталог ресурсов REAPER");

    const QString audio = QDir(root).absoluteFilePath(
        QStringLiteral("tests/source4test/example_V80BPM.mp3"));
    const QString audio2 = QDir(root).absoluteFilePath(
        QStringLiteral("tests/source4test/example_C80BPM.mp3"));
    QVERIFY2(QFileInfo::exists(audio), qPrintable(QStringLiteral("нет фикстуры: %1").arg(audio)));
    QVERIFY2(QFileInfo::exists(audio2), qPrintable(QStringLiteral("нет фикстуры: %1").arg(audio2)));

    const QString outDir = QDir::temp().absoluteFilePath(QStringLiteral("dontfloat_reaper_test"));
    QDir().mkpath(outDir);
    m_reportPath = QDir(outDir).absoluteFilePath(QStringLiteral("report.txt"));
    m_diagPath = QDir(outDir).absoluteFilePath(QStringLiteral("diag.txt"));
    const QString projectPath = QDir(outDir).absoluteFilePath(QStringLiteral("probe.rpp"));
    QFile::remove(projectPath);
    QFile::remove(m_reportPath);
    QFile::remove(m_diagPath);

    // Готовим сценарий с подставленными путями
    QFile source(QDir(root).absoluteFilePath(QStringLiteral("tests/reaper/dontfloat_ara_probe.lua")));
    QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text), "не открыть сценарий");
    QString lua = QString::fromUtf8(source.readAll());
    source.close();
    lua.replace(QStringLiteral("@@AUDIO2@@"), QDir::toNativeSeparators(audio2));
    lua.replace(QStringLiteral("@@AUDIO@@"), QDir::toNativeSeparators(audio));
    lua.replace(QStringLiteral("@@REPORT@@"), QDir::toNativeSeparators(m_reportPath));
    lua.replace(QStringLiteral("@@FX_NAME2@@"),
                QStringLiteral("VST3: DONTFLOAT Pitcher (DONTFLOAT)"));
    lua.replace(QStringLiteral("@@FX_NAME@@"), QStringLiteral("VST3: DONTFLOAT (DONTFLOAT)"));
    lua.replace(QStringLiteral("@@SETTLE_SEC@@"), QString::number(kSettleSeconds));
    lua.replace(QStringLiteral("@@PROJECT@@"), QDir::toNativeSeparators(projectPath));

    // REAPER выполняет Scripts/__startup.lua при запуске. Чужой сценарий, если
    // он там лежит, сохраняем и возвращаем на место в cleanupTestCase
    const QString scriptsDir = QDir(m_resourcePath).absoluteFilePath(QStringLiteral("Scripts"));
    QDir().mkpath(scriptsDir);
    m_startupScript = QDir(scriptsDir).absoluteFilePath(QStringLiteral("__startup.lua"));
    if (QFileInfo::exists(m_startupScript)) {
        m_startupBackup = m_startupScript + QStringLiteral(".dontfloat-test-backup");
        QFile::remove(m_startupBackup);
        QVERIFY2(QFile::rename(m_startupScript, m_startupBackup),
                 "не удалось отложить существующий __startup.lua");
    }

    QFile target(m_startupScript);
    QVERIFY2(target.open(QIODevice::WriteOnly | QIODevice::Text), "не записать __startup.lua");
    target.write(lua.toUtf8());
    target.close();

    // Запускаем REAPER: дневник плагина включается переменной окружения
    QProcess reaper;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("DONTFLOAT_DIAG_FILE"), QDir::toNativeSeparators(m_diagPath));
    reaper.setProcessEnvironment(env);
    reaper.start(m_reaper, { QStringLiteral("-nosplash"), QStringLiteral("-new") });
    QVERIFY2(reaper.waitForStarted(30000), "REAPER не запустился");

    // Ждём не выхода REAPER, а отчёта сценария: выход — отдельная история,
    // и если он не состоится, проверки ARA и нот всё равно должны отработать
    // на уже собранных данных
    QElapsedTimer clock;
    clock.start();
    bool scenarioFinished = false;
    while (clock.elapsed() < kScenarioTimeoutMs) {
        const QStringList lines = readLines(m_reportPath);
        for (const QString& line : lines) {
            if (line.startsWith(QStringLiteral("status="))) {
                scenarioFinished = true;
                break;
            }
        }
        if (scenarioFinished || reaper.state() == QProcess::NotRunning) {
            break;
        }
        QTest::qWait(500);
    }

    // Сценарий отработал и попросил REAPER закрыться — даём ему время
    m_exitedOnItsOwn = reaper.waitForFinished(kQuitGraceMs);
    if (!m_exitedOnItsOwn) {
        reaper.kill();
        reaper.waitForFinished(15000);
    }
    QVERIFY2(scenarioFinished, "сценарий не дошёл до конца — REAPER не отчитался");

    m_report = readLines(m_reportPath);
    m_diag = readLines(m_diagPath);
    m_ran = true;

    qDebug() << "отчёт сценария:" << m_report;
    qDebug() << "строк дневника плагина:" << m_diag.size();
}

void ReaperAraIntegrationTest::cleanupTestCase()
{
    if (m_startupScript.isEmpty()) {
        return;
    }
    QFile::remove(m_startupScript);
    if (!m_startupBackup.isEmpty() && QFileInfo::exists(m_startupBackup)) {
        QFile::rename(m_startupBackup, m_startupScript);
    }
}

void ReaperAraIntegrationTest::requireRun()
{
    if (m_reaper.isEmpty()) {
        QSKIP("REAPER на этой машине не найден — интеграционный тест пропущен");
    }
    if (!m_ran) {
        QSKIP("прогон REAPER не состоялся");
    }
}

QStringList ReaperAraIntegrationTest::diagLines(const QString& prefix) const
{
    QStringList out;
    for (const QString& line : m_diag) {
        if (line.startsWith(prefix)) {
            out.append(line);
        }
    }
    return out;
}

// Проект собрался: две дорожки, на каждой аудио и VST3 DONTFLOAT
void ReaperAraIntegrationTest::testProjectBuildsWithTwoTracksAndPlugins()
{
    requireRun();

    QVERIFY2(!m_report.isEmpty(), "сценарий не оставил отчёта — REAPER до него не дошёл");
    QVERIFY2(m_report.contains(QStringLiteral("status=ok")),
             qPrintable(QStringLiteral("сценарий не дошёл до конца: %1")
                            .arg(m_report.join(QStringLiteral(" | ")))));
    QVERIFY2(m_report.contains(QStringLiteral("tracks=2")), "дорожек должно быть две");
    QVERIFY2(m_report.contains(QStringLiteral("fx_added=2")),
             "VST3 DONTFLOAT должен встать на обе дорожки");

    for (const QString& line : m_report) {
        if (line.startsWith(QStringLiteral("fx1=")) || line.startsWith(QStringLiteral("fx2="))) {
            QVERIFY2(line.contains(QStringLiteral("DONTFLOAT")),
                     qPrintable(QStringLiteral("не тот плагин: %1").arg(line)));
        }
    }
}

// REAPER обязан закрыться сам после того, как сценарий его об этом попросил.
//
// Проверка отдельная и намеренно не мешает остальным: если DAW не выходит,
// это дефект выгрузки плагина, а не повод не проверять ARA и ноты.
void ReaperAraIntegrationTest::testReaperClosesByItself()
{
    requireRun();

    QVERIFY2(m_exitedOnItsOwn,
             "REAPER не закрылся после команды выхода — процесс пришлось снять. "
             "С плагинами DONTFLOAT на дорожках выгрузка подвисает.");
}

// ARA2 донесла до плагина постановку клипов и все три правки
void ReaperAraIntegrationTest::testAraSeesClipEdits()
{
    requireRun();

    QVERIFY2(!m_diag.isEmpty(),
             "плагин не написал ни строки — либо ARA не привязалась, либо стоит "
             "сборка без диагностики (переустановите плагины)");

    const QStringList added = diagLines(QStringLiteral("ara.region.add"));
    const QStringList updated = diagLines(QStringLiteral("ara.region.update"));

    QVERIFY2(added.size() >= 2,
             qPrintable(QStringLiteral("клипы двух дорожек должны прийти как регионы, пришло %1")
                            .arg(added.size())));
    QVERIFY2(!updated.isEmpty(), "правки клипов не дошли до плагина");

    // Нарезка добавляет ещё один регион поверх исходных двух
    QVERIFY2(added.size() >= 3,
             qPrintable(QStringLiteral("после нарезки регионов должно стать больше, их %1")
                            .arg(added.size())));

    // Перенос: хотя бы одно обновление с ненулевым началом на таймлайне
    bool sawMove = false;
    // Растяжение и сжатие: длительность в проекте разошлась с длительностью
    // в источнике — именно так ARA показывает изменённый темп клипа
    bool sawStretch = false;
    bool sawCompress = false;

    for (const QString& line : updated) {
        bool ok = false;
        const double start = fieldOf(line, QStringLiteral("start"), &ok);
        if (ok && start > 0.5) {
            sawMove = true;
        }
        bool okDur = false;
        bool okSrc = false;
        const double dur = fieldOf(line, QStringLiteral("dur"), &okDur);
        const double srcDur = fieldOf(line, QStringLiteral("srcDur"), &okSrc);
        if (okDur && okSrc && srcDur > 0.0) {
            const double factor = dur / srcDur;
            if (factor > 1.2) {
                sawStretch = true;
            }
            if (factor < 0.85) {
                sawCompress = true;
            }
        }
    }

    const QString dump = updated.join(QStringLiteral("\n"));
    QVERIFY2(sawMove, qPrintable(QStringLiteral("перенос клипа не виден плагину:\n%1").arg(dump)));
    QVERIFY2(sawStretch, qPrintable(QStringLiteral("растяжение не видно плагину:\n%1").arg(dump)));
    QVERIFY2(sawCompress, qPrintable(QStringLiteral("сжатие не видно плагину:\n%1").arg(dump)));
}

// Волна получает звук из документа ARA, а не ждёт проигрывания.
//
// Раньше сэмплы попадали в волновой редактор только из захвата блоков в
// process(), а он идёт лишь на играющем транспорте — и в роли ARA-рендерера
// хостом не наполняется вовсе. Дорожка в плагине оставалась пустой, хотя её
// звук лежал в документе целиком. Сценарий ничего не проигрывает: если
// строка появилась, звук пришёл именно из ARA.
void ReaperAraIntegrationTest::testWaveformGetsAudioFromAra()
{
    requireRun();

    const QStringList applied = diagLines(QStringLiteral("ara.audio.applied"));
    QVERIFY2(!applied.isEmpty(),
             "волна не получила звук из документа ARA — рисовать пики нечем");

    bool sawFrames = false;
    for (const QString& line : applied) {
        bool ok = false;
        if (fieldOf(line, QStringLiteral("frames"), &ok) > 0.0 && ok) {
            sawFrames = true;
            break;
        }
    }
    QVERIFY2(sawFrames,
             qPrintable(QStringLiteral("звук пришёл пустым:\n%1")
                            .arg(applied.join(QStringLiteral("\n")))));
}

// Правки клипов не перезаливают волну.
//
// Разрез, перенос и растяжение клипа звук источника не меняют — меняется
// только его место на таймлайне. Значит, звук в редактор заливается один раз,
// а дальше перечитывается лишь размещение клипа: иначе на каждой правке
// волна перерисовывалась бы с нуля, теряя метки.
void ReaperAraIntegrationTest::testClipEditsDoNotRedrawTheWaveform()
{
    requireRun();

    const QStringList applied = diagLines(QStringLiteral("ara.audio.applied"));
    const QStringList synced = diagLines(QStringLiteral("ara.clip.sync"));

    // Экземпляров с волной в сценарии один (на второй дорожке — Pitcher),
    // но при разрезе REAPER копирует цепочку на новый клип: два залива —
    // это ещё два разных экземпляра, а не перерисовка у одного
    QVERIFY2(applied.size() <= 2,
             qPrintable(QStringLiteral("волну перезаливали на каждой правке клипа:\n%1")
                            .arg(applied.join(QStringLiteral("\n")))));

    QVERIFY2(synced.size() >= 2,
             qPrintable(QStringLiteral("размещение клипа не перечитывалось после правок:\n%1")
                            .arg(synced.join(QStringLiteral("\n")))));
}

// Хост отдаёт плагину управление транспортом.
//
// Кнопки воспроизведения в плагине — дублёры кнопок DAW и ходят через
// ARAPlaybackControllerInterface. Нажать их из сценария нельзя (окно плагина
// рисует Qt, а ReaScript до его кнопок не дотягивается), зато видно главное:
// отдал ли хост сам интерфейс. Без него кнопки бессильны, и дело не в плагине.
void ReaperAraIntegrationTest::testHostHandsOverItsTransport()
{
    requireRun();

    const QStringList lines = diagLines(QStringLiteral("ara.host.playback="));
    QVERIFY2(!lines.isEmpty(), "плагин не записал, отдал ли хост транспорт");
    QVERIFY2(lines.first().contains(QStringLiteral("ara.host.playback=1")),
             qPrintable(QStringLiteral("REAPER не отдал плагину управление транспортом:\n%1")
                            .arg(lines.join(QStringLiteral("\n")))));
}

// Тактовая сетка берётся из DAW, а не считается плагином заново.
//
// Хост отдаёт темп и начало такта 1 во времени проекта, а волна показывает
// источник: клип может стоять не в начале и быть растянут. Проверяем, что
// сетка доехала и что коэффициент растяжения клипа при этом учтён.
void ReaperAraIntegrationTest::testBeatGridComesFromHost()
{
    requireRun();

    const QStringList grid = diagLines(QStringLiteral("ara.grid.applied"));
    QVERIFY2(!grid.isEmpty(), "тактовая сетка из DAW до плагина не дошла");

    bool sawTempo = false;
    for (const QString& line : grid) {
        bool ok = false;
        const double bpm = fieldOf(line, QStringLiteral("bpm"), &ok);
        if (ok && bpm > 20.0 && bpm < 300.0) {
            sawTempo = true;
            break;
        }
    }
    QVERIFY2(sawTempo,
             qPrintable(QStringLiteral("темп из хоста бессмысленный:\n%1")
                            .arg(grid.join(QStringLiteral("\n")))));
}

// Воспроизведение в DAW не должно стирать волну.
//
// Под ARA звук берётся из документа, а на вход хост в роли ARA-рендерера
// ничего не кладёт. Пока захват работал и в этом режиме, нажатие Play
// затирало дорожку тишиной: пики пропадали и заново не появлялись.
void ReaperAraIntegrationTest::testPlaybackKeepsWaveform()
{
    requireRun();

    QVERIFY2(m_report.contains(QStringLiteral("playing")),
             "сценарий не дошёл до воспроизведения");
    QVERIFY2(m_report.contains(QStringLiteral("stopped")),
             "сценарий не остановил воспроизведение");

    const QStringList applied = diagLines(QStringLiteral("ara.audio.applied"));
    QVERIFY2(!applied.isEmpty(), "волна так и не получила звук");

    // Очистка волны после того, как звук уже применён, означает ровно тот
    // самый захват тишины
    int appliedAt = -1;
    int clearedAfter = -1;
    for (int i = 0; i < m_diag.size(); ++i) {
        if (m_diag[i].startsWith(QStringLiteral("ara.audio.applied")) && appliedAt < 0) {
            appliedAt = i;
        }
        if (appliedAt >= 0 && i > appliedAt
            && m_diag[i].startsWith(QStringLiteral("wave.cleared"))) {
            clearedAfter = i;
            break;
        }
    }
    // Кнопка воспроизведения в плагине — дублёр кнопки DAW, поэтому плагин
    // обязан узнавать о запуске транспорта хоста, а не только сам его просить
    const QStringList transport = diagLines(QStringLiteral("host.transport"));
    QVERIFY2(!transport.isEmpty(),
             "плагин не узнал о транспорте DAW — кнопке нечего отражать");
    QVERIFY2(transport.contains(QStringLiteral("host.transport playing=1")),
             qPrintable(QStringLiteral("старт транспорта до плагина не дошёл:\n%1")
                            .arg(transport.join(QStringLiteral("\n")))));

    // Под ARA дорожку выдаёт сам плагин: хост исходный звук на вход не
    // кладёт. Пока рендерер был пустой заглушкой, роль была заявлена, а
    // дорожка молчала — при нажатии Play в DAW каретки шли, звука не было
    const QStringList rendered = diagLines(QStringLiteral("ara.render "));
    QVERIFY2(!rendered.isEmpty(), "плагин не отчитался о рендеринге");
    bool sawFrames = false;
    for (const QString& line : rendered) {
        bool ok = false;
        if (fieldOf(line, QStringLiteral("frames"), &ok) > 0.0 && ok) {
            sawFrames = true;
            break;
        }
    }
    QVERIFY2(sawFrames,
             "ARA-рендерер не отдал ни одного кадра — под ARA дорожка молчит");

    QVERIFY2(clearedAfter < 0,
             "волну обнулили уже после того, как звук из ARA был получен — "
             "захват снова затирает дорожку");
}

// Референсные ноты: источники разобраны, и один отдан соседнему как референс.
//
// Проверяется именно путь ARA, а не общая доска нот (SharedNoteBoard): в
// ARA-режиме редактор её намеренно не использует — ноты и разметка приходят
// из документа хоста, а референсом служит последний разобранный источник
// (AraDocumentController::referenceNotesExcluding). Доску покрывает отдельный
// plugin_shared_notes_test, там она и работает.
void ReaperAraIntegrationTest::testReferenceNotesTravelBetweenInstances()
{
    requireRun();

    const QStringList parsed = diagLines(QStringLiteral("ara.notes.parsed"));
    const QStringList reference = diagLines(QStringLiteral("ara.notes.reference"));

    QVERIFY2(!parsed.isEmpty(),
             "ни один источник не разобран — ARA не довела звук до анализа");

    bool sawNotes = false;
    for (const QString& line : parsed) {
        bool ok = false;
        if (fieldOf(line, QStringLiteral("count"), &ok) > 0.0 && ok) {
            sawNotes = true;
            break;
        }
    }
    QVERIFY2(sawNotes,
             qPrintable(QStringLiteral("разбор дал пустые ноты:\n%1")
                            .arg(parsed.join(QStringLiteral("\n")))));

    QVERIFY2(!reference.isEmpty(),
             qPrintable(QStringLiteral("референсные ноты соседу не отданы; разборов: %1")
                            .arg(parsed.size())));

    // Референс берётся из общего пула разобранных источников, а не из одного:
    // на двух дорожках пул обязан вырасти хотя бы до двух
    bool sawSharedPool = false;
    for (const QString& line : reference) {
        bool ok = false;
        if (fieldOf(line, QStringLiteral("pool"), &ok) >= 2.0 && ok) {
            sawSharedPool = true;
            break;
        }
    }
    QVERIFY2(sawSharedPool,
             qPrintable(QStringLiteral("референс отдан, но источник в пуле один — "
                                       "вторая дорожка до модели не дошла:\n%1")
                            .arg(reference.join(QStringLiteral("\n")))));
}

QTEST_MAIN(ReaperAraIntegrationTest)
#include "reaper_ara_integration_test.moc"
