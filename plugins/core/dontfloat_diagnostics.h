#ifndef DONTFLOAT_DIAGNOSTICS_H
#define DONTFLOAT_DIAGNOSTICS_H

/**
 * Дневник событий плагина для интеграционных тестов с настоящей DAW.
 *
 * Изнутри хоста плагин почти ничего о себе не рассказывает: параметров у него
 * нет, а ARA-события, обмен нотами и порядок выгрузки живут целиком внутри
 * процесса DAW. Чтобы тест мог их увидеть, плагин пишет их сюда — построчно,
 * в файл.
 *
 * По умолчанию выключен и не стоит ничего: путь берётся из переменной
 * окружения DONTFLOAT_DIAG_FILE, и если её нет, каждый вызов сразу выходит.
 * В обычной работе DAW переменная не задана.
 *
 * Заголовочный целиком и намеренно: писать сюда нужно из мест, которые
 * собираются в разные цели — приложение, плагины, тесты, — и отдельный .cpp
 * пришлось бы добавлять в каждую из них по отдельности.
 */

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>

namespace Dontfloat::PluginCore {

class Diagnostics {
public:
    /** Задана ли DONTFLOAT_DIAG_FILE (проверяется один раз за процесс). */
    static bool enabled() { return !path().empty(); }

    /** Пишет строку событием: «имя ключ=значение ключ=значение». */
    static void log(const std::string& line)
    {
        if (!enabled()) {
            return;
        }
        // Дописываем и закрываем на каждой строке: DAW может закрыться в любой
        // момент, а тест читает файл уже после её выхода — недописанный буфер
        // означал бы потерянные события
        const std::lock_guard<std::mutex> guard(mutex());
        std::ofstream out(path(), std::ios::app);
        if (out) {
            out << line << '\n';
        }
    }

private:
    static const std::string& path()
    {
        static const std::string value = []() -> std::string {
#ifdef _WIN32
            // getenv на MSVC считается небезопасным, а _dupenv_s отдаёт копию,
            // которую нужно освободить
            char* raw = nullptr;
            std::size_t size = 0;
            if (_dupenv_s(&raw, &size, "DONTFLOAT_DIAG_FILE") != 0 || raw == nullptr) {
                return {};
            }
            std::string result(raw);
            std::free(raw);
            return result;
#else
            const char* raw = std::getenv("DONTFLOAT_DIAG_FILE");
            return raw ? std::string(raw) : std::string();
#endif
        }();
        return value;
    }

    static std::mutex& mutex()
    {
        static std::mutex instance;
        return instance;
    }
};

} // namespace Dontfloat::PluginCore

#endif // DONTFLOAT_DIAGNOSTICS_H
