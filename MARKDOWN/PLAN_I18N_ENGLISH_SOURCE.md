# План: исходный язык UI — английский (EN → RU и др.)

## Зачем

Сейчас строки в `tr()` / `.ui` на **английском**, а `translations/ru_RU.ts` переводит EN→RU;
`en_US.ts` — identity. Это стандарт Qt Linguist и удобно для новых языков.

Исторически msgid был русским; миграция выполнена (см. этапы ниже).

## Подготовлено (этап 0)

| Артефакт | Описание |
|----------|----------|
| `translations/from_en/en_US.ts` | EN→EN (staging) |
| `translations/from_en/ru_RU.ts` | EN→RU (staging) |
| `translations/from_en/source_map.json` | карты RU↔EN |
| `tools/prepare_en_source_translations.py` | исторический инвертор (из старых RU-source live `.ts`) |
| `tools/migrate_tr_to_english.py` | автозамена `tr()` / `.ui` |
| `tools/install_en_source_translations.py` | копирование staging → live + EXTRA |

---

## Этапы переноса

### 1. Инвентаризация и заморозка строк

- [x] Прогнать `lupdate -no-obsolete` на текущем коде; убедиться, что live `.ts` полные.
- [x] Пересобрать `from_en/` (`python tools/prepare_en_source_translations.py`).
- [x] Зафиксировать `source_map.json` (baseline + EXTRA).
- [x] Стили/CSS в `tr()` не трогать (passthrough).

### 2. Автозамена строк в коде (RU → EN)

- [x] `src/**`, `include/**`, `ui/*.ui`, `plugins/ui/**`
- [x] Довод вручную: `QCoreApplication::translate`, смежные литералы в `tr()`
- [x] Whitelist: автоним `Русский`

### 3. Политика инвариантов (сохранить)

| Элемент | Правило после миграции |
|---------|-------------------------|
| Подменю `Language` | source = `Language`, во всех локалях translation = `Language` |
| Пункт «Русский» | source = автоним `Русский` |
| Пункт `English` | source = `English` |
| Status bar | source `Language: English` / `Language: Russian`; в RU для Russian — `Язык: Русский` |
| Меню `&File` / `&Edit` | accelerator в EN; в RU — `&Файл` / `&Правка` |

Обновлены `.cursor/commands/init.md`, `MARKDOWN/INIT.MD`.

### 4. Подмена каталогов переводов

- [x] Скопировать `translations/from_en/{en_US,ru_RU}.ts` → `translations/`.
- [x] `lupdate -no-obsolete` против англоязычного кода.
- [x] Helpers переписаны под EN-source (`fix_translations.py`, `finalize_translations.py`, `apply_remaining_en.py`).
- [x] `lrelease` (411 finished / 0 unfinished).

### 5. Поведение приложения

- код EN;
- при `en_US` — identity qm (или можно не грузить);
- при `ru_RU` — `ru_RU.qm`.

### 6. Новые языки

Шаблон:

```text
lupdate ... -ts translations/de_DE.ts
# переводчики заполняют EN→DE
lrelease translations/de_DE.ts
```

Добавить пункт в меню Language + qm в CMake `qt_add_lrelease`.

### 7. Документация и CI

- [x] `docs/features.md`, `MARKDOWN/DEVELOPMENT_GUIDE.md` — «исходный язык = English».
- [x] CI: `tools/check_tr_english_source.py` + job `i18n-guard` в `.github/workflows/ci.yml`.
- [x] Changelog в `MARKDOWN/DOCUMENTATION_CHANGELOG.md`.

---

## Критерий готовности

- [x] В `src/` и `ui/` строки UI в `tr()` на английском (кроме whitelist).
- [x] `ru_RU.ts`: source EN, translation RU; `.qm` собраны без unfinished.
- [x] Переключение Language: EN не требует `.qm` (msgid = English); RU требует `ru_RU.qm`.
- [x] `lupdate` не заливает сотни unfinished.
- [x] Документация и `fix_translations.py` согласованы с EN-source.
