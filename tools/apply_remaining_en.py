# -*- coding: utf-8 -*-
"""Apply English for all en_US messages where translation still equals Russian source."""
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EN = {
    "WAV файлы (*.wav);;Все файлы (*)": "WAV files (*.wav);;All files (*)",
    "Авто-метки по тактовой сетке": "Auto markers on beat grid",
    "Авто-метки по транзиентам": "Auto markers on transients",
    "Авто-метки по транзиентам (Onset detection)": "Auto markers (onset detection)",
    "Анализ тональности...": "Key analysis...",
    "Аудиоданные не загружены.": "No audio loaded.",
    "Аудиофайлы (*.wav *.mp3)": "Audio files (*.wav *.mp3)",
    "Включить/выключить цикл": "Toggle loop",
    "Волновая форма не инициализирована.": "Waveform not initialized.",
    "Воспроизвести звук большой доли с текущей громкостью для настройки метронома": "Play downbeat at current volume (metronome)",
    "Воспроизвести звук малой доли с текущей громкостью для настройки метронома": "Play upbeat at current volume (metronome)",
    "Выберите пользовательский звук для метронома": "Choose custom metronome sound",
    "Выбрать звук": "Choose sound",
    "Вычисленный BPM (уверенность --%): -- BPM": "Computed BPM (confidence --%): -- BPM",
    "Для применения растяжения необходимо минимум 2 метки.\nИспользуйте клавишу M для добавления меток.": "Time stretch needs at least 2 markers.\nPress M to add markers.",
    "Загрузка аудиофайла...": "Loading audio...",
    "Информация о долях отсутствует": "No beat information",
    "Конец таймлайна": "Timeline end",
    "Конец таймлайна\nВремя: %1\nПозиция: %2 сэмплов": "Timeline end\nTime: %1\nPosition: %2 samples",
    "Коэффициент: %1": "Ratio: %1",
    "Мажорные": "Major",
    "Метка\nВремя: %1\nПозиция: %2 сэмплов%3": "Marker\nTime: %1\nPosition: %2 samples%3",
    "Метроном включен": "Metronome on",
    "Метроном выключен": "Metronome off",
    "Минорные": "Minor",
    "Модуляция": "Modulation",
    "Настройки метронома обновлены": "Metronome settings updated",
    "Начало таймлайна\nВремя: %1\nПозиция: %2 сэмплов": "Timeline start\nTime: %1\nPosition: %2 samples",
    "Не\n                                                                                    определена": "Not\n                                                                                    defined",
    "Не определена": "Undefined",
    "Не удалось обнаружить транзиенты в аудиофайле.": "No transients found.",
    "Не удалось сохранить файл %1:\n%2.": "Could not save %1:\n%2.",
    "Недостаточно меток": "Not enough markers",
    "Некорректная частота дискретизации.": "Invalid sample rate.",
    "Некорректное значение BPM.": "Invalid BPM.",
    "Некорректные параметры BPM или sampleRate": "Invalid BPM or sample rate",
    "Неровные доли не найдены": "No irregular beats found",
    "Нет аудиоданных для анализа": "No audio to analyze",
    "Оставить метки даже в случае пропуска самовыровнения": "Keep markers when skipping alignment",
    "Ошибка декодирования:": "Decode error:",
    "Ошибка загрузки перевода": "Translation load error",
    "Ошибка загрузки файла": "File load error",
    "Ошибка при обработке аудио": "Audio processing error",
    "Ошибка: WaveformView не инициализирован": "Error: WaveformView not initialized",
    "Ошибка: Сначала установите точки A и B для цикла!": "Error: Set loop points A and B first!",
    "Ошибка: некорректные данные в буфере": "Error: invalid buffer data",
    "Ошибка: неподдерживаемый формат аудио": "Error: unsupported audio format",
    "Ошибка: неподдерживаемый формат данных": "Error: unsupported data format",
    "Ошибка: нет данных для сохранения": "Error: nothing to save",
    "Ошибка: нет загруженного аудио": "Error: no audio loaded",
    "Переместить выделенные метки на тактовую сетку": "Snap selected markers to grid",
    "Переместить метку на тактовую сетку": "Snap marker to grid",
    "Подходящих позиций для тактовой сетки не найдено.": "No grid positions found.",
    "Подходящих транзиентов не найдено.": "No suitable transients found.",
    "Позиция: %1\nВремя: %2": "Position: %1\nTime: %2",
    "Размер такта": "Time signature",
    "Размер такта. Перед выравниванием можно изменить.": "Time signature (change before alignment).",
    "Размер такта:": "Time signature:",
    "Растяжение применено успешно. Размер: %1 → %2 сэмплов": "Stretch applied. Length: %1 → %2 samples",
    "Сначала загрузите аудиофайл": "Load an audio file first",
    "Создано %1 авто-меток по тактовой сетке": "Created %1 grid markers",
    "Создано %1 авто-меток по транзиентам": "Created %1 transient markers",
    "Создано %1 меток коррекции для %2 неровных долей": "Created %1 correction markers for %2 irregular beats",
    "Среднее отклонение: --%": "Average deviation: --%",
    "Статус: загрузка...": "Status: loading...",
    "Тема изменена: %1": "Theme: %1",
    "Тональность определена: %1": "Key: %1",
    "Тональность:": "Key:",
    "Точка начала сетки выходит за пределы аудиофайла.": "Grid start is outside the file.",
    "Удалить выделенные метки": "Delete selected markers",
    "Удалить метку": "Delete marker",
    "Установить точку A (начало цикла)": "Set loop start A",
    "Установить точку B (конец цикла)": "Set loop end B",
    "Файл сохранен: %1": "Saved: %1",
    "Цветовая схема изменена: %1": "Color scheme: %1",
    "Цикл включен: %1 - %2": "Loop on: %1 - %2",
    "Цикл выключен": "Loop off",
    "Щелчок (стандартный)": "Click (default)",
    "Язык: Русский": "Language: Russian",
    "неизвестная ошибка": "unknown error",
    "неподдерживаемый формат": "unsupported format",
    "нет доступа к файлу": "no file access",
    "файл не найден или недоступен": "file not found or inaccessible",
}

path = ROOT / "translations" / "en_US.ts"
tree = ET.parse(path)
root = tree.getroot()
n = 0
for msg in root.iter("message"):
    tr = msg.find("translation")
    src_el = msg.find("source")
    if tr is None or src_el is None or tr.get("type") == "vanished":
        continue
    src = (src_el.text or "").strip()
    if src not in EN:
        continue
    tr.text = EN[src]
    tr.attrib.pop("type", None)
    n += 1
tree.write(path, encoding="utf-8", xml_declaration=True)
print("Applied", n)
