#include "Utils/FileDialogHelper.h"
#include <QFileDialog>
#include <QSettings>
#include <QFileInfo>
#include <QDir>

namespace {
    const QString SETTINGS_KEY = "lastAudioPath";
}

QString FileDialogHelper::openAudioFile(QWidget *parent) {
    QString dir = readLastDir();
    QString filePath = QFileDialog::getOpenFileName(
        parent,
        "Open Audio File",
        dir,
        "Audio Files (*.wav *.mp3 *.flac *.ogg *.aiff);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        writeLastDir(QFileInfo(filePath).absolutePath());
    }

    return filePath;
}

QString FileDialogHelper::saveAudioFile(QWidget *parent) {
    QString dir = readLastDir();
    QString filePath = QFileDialog::getSaveFileName(
        parent,
        "Save Audio File",
        dir,
        "WAV File (*.wav);;FLAC File (*.flac);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        writeLastDir(QFileInfo(filePath).absolutePath());
    }

    return filePath;
}

QString FileDialogHelper::getLastOpenedFilePath() {
    QSettings settings;
    QString lastPath = settings.value(SETTINGS_KEY).toString();
    return QFileInfo(lastPath).exists() ? lastPath : QString();
}

QString FileDialogHelper::readLastDir() {
    QSettings settings;
    return settings.value(SETTINGS_KEY, QDir::homePath()).toString();
}

void FileDialogHelper::writeLastDir(const QString &path) {
    QSettings settings;
    settings.setValue(SETTINGS_KEY, path);
}
