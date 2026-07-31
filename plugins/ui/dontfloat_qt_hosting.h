#ifndef DONTFLOAT_QT_HOSTING_H
#define DONTFLOAT_QT_HOSTING_H

namespace Dontfloat::Plugins::Ui {

/** Создаёт QApplication при необходимости и задаёт имя приложения продукта.
 *  На Windows также ставит native timer, чтобы Qt-события крутились внутри DAW
 *  без QApplication::exec() (иначе VST3/LV2 UI зависает). */
void ensureQtApplication(const char* applicationName = "DONTFLOAT");

/** Короткий processEvents — для idle/timer колбэков хоста. */
void pumpQtEvents(int maxMillis = 8);

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_QT_HOSTING_H
