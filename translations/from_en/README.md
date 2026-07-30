# Staging / maps for EN-source migration (`from_en/`)

Исторические артефакты миграции RU→EN msgid. Живые каталоги приложения —
`translations/en_US.ts` и `translations/ru_RU.ts` (уже EN-source).

| Файл | Назначение |
|------|------------|
| `en_US.ts` / `ru_RU.ts` | staging-снимки на момент инверсии |
| `source_map.json` | словари `ru_to_en` / `en_to_ru` (helpers + мигратор) |

Установка в live (после правок staging):

```powershell
python tools/install_en_source_translations.py
```

`prepare_en_source_translations.py` — **не запускать** против текущих live `.ts`
(они уже с английским `<source>`).

План: `MARKDOWN/PLAN_I18N_ENGLISH_SOURCE.md`.
