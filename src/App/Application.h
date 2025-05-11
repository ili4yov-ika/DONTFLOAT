#pragma once

#include <QApplication>

class Application {
public:
    Application(int &argc, char **argv);
    int run();

private:
    QApplication qtApp;

    void loadSettings();
    void setupStyle();
};
