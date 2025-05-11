#include "Application.h"
#include "UI/MainWindow.h"

#include <QSettings>
#include <QStyleFactory>

Application::Application(int &argc, char **argv)
    : qtApp(argc, argv) {
    qtApp.setApplicationName("DONTFLOAT");
    qtApp.setOrganizationName("OpenDAW");
    qtApp.setApplicationVersion("0.1");

    loadSettings();
    setupStyle();
}

int Application::run() {
    MainWindow mainWindow;
    mainWindow.show();
    return qtApp.exec();
}

void Application::loadSettings() {
    // Заглушка: можно подключить QSettings для загрузки параметров
    QSettings settings;
    // settings.value("someKey", defaultValue);
}

void Application::setupStyle() {
    // Пример: включение Fusion-стиля
    qtApp.setStyle(QStyleFactory::create("Fusion"));
}
