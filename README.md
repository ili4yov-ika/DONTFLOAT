# DONTFLOAT

**DONTFLOAT** — это кроссплатформенное open-source приложение для анализа и выравнивания темпа (BPM) аудио-дорожек. Программа визуализирует звуковую волну, определяет приблизительный BPM и предлагает выровнять темп, если он "плавает".

## Основные функции

- Загрузка аудиофайлов (WAV, MP3, FLAC)
- Визуализация аудиоволны по каналам
- Автоматическое определение BPM
- Обнаружение "плавающего" темпа (неравномерность)
- Предложение выровнять темп
- Кроссплатформенность (Linux, Windows, macOS)
- Открытый исходный код под лицензией **GNU GPL v3**

---

## Скриншот (в будущем)
<!-- ![Screenshot](assets/screenshots/main_ui.png) -->

---

## Сборка

### Зависимости

- CMake 3.16+
- C++17
- Qt 6 (Core, Gui, Widgets, Multimedia)
- (опционально) libsndfile, RubberBand и др. для работы с аудио

### Linux / macOS

```bash
git clone https://github.com/yourname/DONTFLOAT.git
cd DONTFLOAT
mkdir build && cd build
cmake ..
make
./DONTFLOAT
