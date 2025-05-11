#pragma once

#include <QString>
#include <QWidget>

class FileDialogHelper {
public:
    static QString openAudioFile(QWidget *parent = nullptr);
    static QString saveAudioFile(QWidget *parent = nullptr);
    static QString getLastOpenedFilePath();

private:
    static QString readLastDir();
    static void writeLastDir(const QString &path);
};
