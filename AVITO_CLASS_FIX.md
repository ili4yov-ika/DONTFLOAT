# Решение проблемы: div.get("class") возвращает None

## Проблема

При парсинге Avito с использованием BeautifulSoup метод `div.get("class")` возвращает `None` для большинства элементов, либо проверка `if "styles-item" in div.get("class")` не находит нужные элементы.

## Причины

### 1. **Атрибут `class` в BeautifulSoup — это список, а не строка!**

```python
# HTML: <div class="styles-item-abc123 item-card">
div.get("class")  # Вернет: ['styles-item-abc123', 'item-card']
                  # НЕ строку: "styles-item-abc123 item-card"
```

### 2. **Проверка точного совпадения не работает**

```python
# ❌ НЕПРАВИЛЬНО - ищет точное совпадение строки в списке
if "styles-item" in div.get("class"):
    # Никогда не найдет, потому что в списке ['styles-item-abc123', 'item-card']
    # нет элемента равного точно "styles-item"
    pass

# ✅ ПРАВИЛЬНО - ищет частичное вхождение
class_list = div.get("class")
if class_list and any("styles-item" in cls for cls in class_list):
    # Найдет, потому что "styles-item" содержится в "styles-item-abc123"
    pass
```

### 3. **Динамическая загрузка контента**

Avito загружает контент через JavaScript. Нужно ждать загрузки элементов:

```python
# ❌ НЕДОСТАТОЧНО
await asyncio.sleep(1.0)

# ✅ ПРАВИЛЬНО
await page.wait_for_selector('[data-marker="item"]', timeout=5000)
await asyncio.sleep(1.0)
```

## Решения

### Решение 1: Исправленная проверка классов (BeautifulSoup)

```python
async def _get_avito_urls(self) -> List[str]:
    """Исправленная версия с правильной проверкой классов"""
    urls = []
    
    await self.page.mouse.wheel(0, 400)
    
    # Ждем загрузки элементов
    try:
        await self.page.wait_for_selector('[data-marker="item"]', timeout=5000)
    except:
        pass
    
    await asyncio.sleep(1.0)
    
    html = await self.page.content()
    soup = BeautifulSoup(html, 'html.parser')
    divs = soup.find_all("div")
    
    for div in divs:
        # Получаем список классов
        class_list = div.get("class")
        
        # Проверяем, что class не None и это список
        if class_list is None:
            continue
        
        # Ищем частичное совпадение "styles-item" в любом из классов
        has_styles_item = any("styles-item" in cls for cls in class_list)
        
        if has_styles_item:
            a_tag = div.find("a")
            
            if a_tag is None:
                continue
            
            href = a_tag.get("href")
            if href is None:
                continue
            
            # Формируем полный URL
            if href.startswith("http"):
                url = href
            else:
                url = "https://www.avito.ru" + href
            
            # Добавляем URL если его еще нет
            if url not in self.url_mask:
                urls.append(url)
                self.url_mask[url] = True
    
    await self.page.mouse.wheel(0, 800)
    return urls
```

### Решение 2: Использование Playwright вместо BeautifulSoup

Более надежный способ для динамического контента:

```python
async def _get_avito_urls(self) -> List[str]:
    """Версия с использованием Playwright селекторов"""
    urls = []
    
    await self.page.mouse.wheel(0, 400)
    await asyncio.sleep(1.0)
    
    try:
        # Ждем загрузки элементов
        await self.page.wait_for_selector('[data-marker="item"]', timeout=5000)
        
        # Используем Playwright для поиска элементов напрямую
        items = await self.page.query_selector_all('[data-marker="item"] a')
        
        for item in items:
            href = await item.get_attribute('href')
            
            if href is None:
                continue
            
            # Формируем полный URL
            if href.startswith("http"):
                url = href
            else:
                url = "https://www.avito.ru" + href
            
            # Добавляем URL если его еще нет
            if url not in self.url_mask:
                urls.append(url)
                self.url_mask[url] = True
    
    except Exception as e:
        print(f"Ошибка при парсинге: {e}")
    
    await self.page.mouse.wheel(0, 800)
    return urls
```

### Решение 3: CSS селекторы BeautifulSoup

Можно использовать CSS селекторы для более точного поиска:

```python
# Поиск всех div с классом, содержащим "styles-item"
items = soup.select('div[class*="styles-item"]')

# Или более конкретно:
items = soup.select('div[class*="styles-item"] a')
```

## Отладка

Для отладки добавьте вывод информации о найденных элементах:

```python
divs = soup.find_all("div")
print(f"Всего div: {len(divs)}")

divs_with_classes = [d for d in divs if d.get("class")]
print(f"Div с классами: {len(divs_with_classes)}")

# Выведем первые 5 примеров классов
for div in divs_with_classes[:5]:
    print(f"Классы: {div.get('class')}")
```

## Рекомендации

1. **Используйте `page.wait_for_selector()`** для ожидания загрузки элементов
2. **Предпочитайте Playwright селекторы** для динамического контента
3. **Помните, что `class` в BeautifulSoup — это список**
4. **Используйте `any()` для проверки частичного вхождения** в список классов
5. **Всегда проверяйте на `None`** перед использованием результатов

## Проверка

Запустите демонстрацию:

```bash
python3 avito_parser_test.py
```

Это покажет, как работает атрибут `class` в BeautifulSoup и разницу между правильной и неправильной проверкой.

## Дополнительные селекторы Avito

На Avito можно использовать следующие селекторы:

- `[data-marker="item"]` — карточка товара
- `[data-marker="item-title"]` — заголовок
- `[itemprop="url"]` — ссылка на товар
- `div[class*="iva-item"]` — другой вариант карточки

Пример:

```python
# Playwright
items = await page.query_selector_all('[data-marker="item"] [itemprop="url"]')

# BeautifulSoup
items = soup.select('[data-marker="item"] [itemprop="url"]')
```
