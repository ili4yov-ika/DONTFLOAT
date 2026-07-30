#ifndef DONTFLOAT_QT_HOSTING_H
#define DONTFLOAT_QT_HOSTING_H

namespace Dontfloat::Plugins::Ui {

/** Создаёт QApplication при необходимости и задаёт имя приложения продукта. */
void ensureQtApplication(const char* applicationName = "DONTFLOAT");

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_QT_HOSTING_H
