#include "dontfloat_qt_hosting.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QString>

#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#else
#include <dlfcn.h>
#endif

namespace Dontfloat::Plugins::Ui {
namespace {

#if defined(_WIN32)
UINT_PTR g_qtPumpTimerId = 0;

void CALLBACK qtPumpTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
    if (QApplication* app = qApp) {
        app->sendPostedEvents();
        app->processEvents(QEventLoop::AllEvents, 8);
        app->sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

void ensureWin32QtEventPump()
{
    if (g_qtPumpTimerId != 0) {
        return;
    }
    // Hosts (REAPER, etc.) do not run QApplication::exec(). Without a native
    // timer, Qt timers/futures never fire and attached() can deadlock waiting
    // for a message pump the host will not run until we return.
    g_qtPumpTimerId = SetTimer(nullptr, 0, 16, qtPumpTimerProc);
}
#endif

/** Directory of the shared library that contains ensureQtApplication (the
 *  *.impl.dll / plugin module). Qt looks for platforms/ relative to the host
 *  EXE by default — for in-process plugins we must point it at our own dir. */
QString pluginModuleDirectory()
{
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&pluginModuleDirectory),
                            &module)
        || !module) {
        return {};
    }
    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return {};
    }
    return QFileInfo(QString::fromWCharArray(path)).absolutePath();
#else
    Dl_info info {};
    if (dladdr(reinterpret_cast<const void*>(&pluginModuleDirectory), &info) == 0
        || !info.dli_fname) {
        return {};
    }
    return QFileInfo(QString::fromUtf8(info.dli_fname)).absolutePath();
#endif
}

void configureQtPluginSearchPaths()
{
    const QString moduleDir = pluginModuleDirectory();
    if (moduleDir.isEmpty()) {
        return;
    }

    // Must be set before the first QGuiApplication/QApplication is created.
    if (!QCoreApplication::instance()) {
        const QByteArray dirUtf8 = QDir::toNativeSeparators(moduleDir).toUtf8();
        const QByteArray platformsUtf8 =
            QDir::toNativeSeparators(moduleDir + QStringLiteral("/platforms")).toUtf8();
        if (qgetenv("QT_PLUGIN_PATH").isEmpty()) {
            qputenv("QT_PLUGIN_PATH", dirUtf8);
        }
        if (qgetenv("QT_QPA_PLATFORM_PLUGIN_PATH").isEmpty()) {
            qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", platformsUtf8);
        }
    }

    if (!QCoreApplication::libraryPaths().contains(moduleDir)) {
        QCoreApplication::addLibraryPath(moduleDir);
    }
}

} // namespace

void ensureQtApplication(const char* applicationName)
{
    configureQtPluginSearchPaths();

    if (QCoreApplication::instance()) {
        if (applicationName && applicationName[0] != '\0') {
            QCoreApplication::setApplicationName(QString::fromUtf8(applicationName));
        }
#if defined(_WIN32)
        ensureWin32QtEventPump();
#endif
        return;
    }

    static int argc = 1;
    static char appNameStorage[96] = "DONTFLOAT";
    if (applicationName && applicationName[0] != '\0') {
        std::strncpy(appNameStorage, applicationName, sizeof(appNameStorage) - 1);
        appNameStorage[sizeof(appNameStorage) - 1] = '\0';
    }
    static char* argv[] = {appNameStorage, nullptr};
    auto* app = new QApplication(argc, argv);
    // Re-add after construction (QApplication may reset library paths from the EXE).
    configureQtPluginSearchPaths();
    app->setApplicationName(QString::fromUtf8(appNameStorage));
    app->setOrganizationName(QStringLiteral("DONTFLOAT"));
#if defined(_WIN32)
    ensureWin32QtEventPump();
#endif
}

void pumpQtEvents(int maxMillis)
{
    if (QApplication* app = qApp) {
        app->sendPostedEvents();
        app->processEvents(QEventLoop::AllEvents, maxMillis);
        app->sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

} // namespace Dontfloat::Plugins::Ui
