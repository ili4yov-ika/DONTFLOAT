"""
Тестовый парсер Avito с исправлениями для работы с div.class
"""
import asyncio
from typing import List
from bs4 import BeautifulSoup


class AvitoParser:
    def __init__(self, page):
        self.page = page
        self.url_mask = {}
    
    async def _get_avito_urls_original(self) -> List[str]:
        """Оригинальный метод с проблемой"""
        print("_get_avito_urls_original")
        urls = []

        await self.page.mouse.wheel(0, 400)
        await asyncio.sleep(1.0)

        html = await self.page.content()
        soup = BeautifulSoup(html, 'html.parser')
        divs = soup.find_all("div")
        print(f"Divs amount: {len(divs)}")
        
        for div in divs:
            if div.get("class") == None:
                continue
            if "styles-item" in div.get("class"):
                print(div.get("class"))
                a_tag = div.find("a")
                url = "https://www.avito.ru/" + a_tag.get("href")
                print(url)
                if self.url_mask.get(url, False) != True:
                    urls.append(url)
                self.url_mask[url] = True
        
        await self.page.mouse.wheel(0, 800)

        return urls

    async def _get_avito_urls_fixed(self) -> List[str]:
        """
        ИСПРАВЛЕННЫЙ метод с несколькими улучшениями:
        1. Ожидание загрузки элементов через page.wait_for_selector
        2. Более надежная проверка классов (class это список в BeautifulSoup!)
        3. Проверка наличия a_tag перед использованием
        4. Отладочная информация
        """
        print("=== _get_avito_urls_fixed START ===")
        urls = []

        # Прокрутка страницы
        await self.page.mouse.wheel(0, 400)
        
        # ВАЖНО: Ждем загрузки элементов с нужным классом
        try:
            # Ждем появления элементов с data-marker="item" (обычный атрибут карточек на Avito)
            await self.page.wait_for_selector('[data-marker="item"]', timeout=5000)
            print("✓ Элементы загружены")
        except Exception as e:
            print(f"⚠ Timeout при ожидании элементов: {e}")
        
        # Дополнительная задержка для полной загрузки
        await asyncio.sleep(1.0)

        html = await self.page.content()
        soup = BeautifulSoup(html, 'html.parser')
        
        # Отладка: выведем первые 5 div с классами
        divs = soup.find_all("div")
        print(f"\nВсего div элементов: {len(divs)}")
        
        divs_with_classes = [div for div in divs if div.get("class")]
        print(f"Div с классами: {len(divs_with_classes)}")
        
        if len(divs_with_classes) > 0:
            print("\nПримеры первых 5 div с классами:")
            for i, div in enumerate(divs_with_classes[:5]):
                classes = div.get("class")
                print(f"  {i+1}. class={classes}")
        
        # ИСПРАВЛЕННАЯ логика поиска
        found_items = 0
        for div in divs:
            # ВАЖНО: В BeautifulSoup class это СПИСОК строк, а не строка!
            class_list = div.get("class")
            
            # Проверка на None
            if class_list is None:
                continue
            
            # Проверка, что это список (должно быть всегда)
            if not isinstance(class_list, list):
                continue
            
            # Ищем "styles-item" в списке классов
            # Можно искать точное совпадение или частичное (startswith)
            has_styles_item = any("styles-item" in cls for cls in class_list)
            
            if has_styles_item:
                found_items += 1
                print(f"\n✓ Найден элемент #{found_items} с классом: {class_list}")
                
                # Ищем тег a внутри div
                a_tag = div.find("a")
                
                if a_tag is None:
                    print("  ⚠ Не найден тег <a> внутри div")
                    continue
                
                href = a_tag.get("href")
                if href is None:
                    print("  ⚠ У тега <a> нет атрибута href")
                    continue
                
                # Формируем полный URL
                if href.startswith("http"):
                    url = href
                else:
                    url = "https://www.avito.ru" + href
                
                print(f"  URL: {url}")
                
                # Добавляем URL если его еще нет
                if url not in self.url_mask:
                    urls.append(url)
                    self.url_mask[url] = True
        
        print(f"\n=== Найдено уникальных URL: {len(urls)} ===")
        
        # Дополнительная прокрутка
        await self.page.mouse.wheel(0, 800)

        return urls

    async def _get_avito_urls_alternative(self) -> List[str]:
        """
        АЛЬТЕРНАТИВНЫЙ метод: используем Playwright селекторы вместо BeautifulSoup
        Это более надежно для динамического контента
        """
        print("=== _get_avito_urls_alternative START ===")
        urls = []

        # Прокрутка страницы
        await self.page.mouse.wheel(0, 400)
        await asyncio.sleep(1.0)
        
        try:
            # Ждем загрузки элементов
            await self.page.wait_for_selector('[data-marker="item"]', timeout=5000)
            
            # Используем Playwright для поиска элементов напрямую
            # Ищем все ссылки внутри элементов с data-marker="item"
            items = await self.page.query_selector_all('[data-marker="item"] a')
            
            print(f"Найдено элементов через Playwright: {len(items)}")
            
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
                    print(f"  + {url}")
            
            print(f"\n=== Найдено уникальных URL: {len(urls)} ===")
            
        except Exception as e:
            print(f"⚠ Ошибка при парсинге: {e}")
        
        # Дополнительная прокрутка
        await self.page.mouse.wheel(0, 800)

        return urls


# Демонстрация проблемы с BeautifulSoup
def demonstrate_class_issue():
    """Демонстрирует, как работает атрибут class в BeautifulSoup"""
    html_sample = """
    <html>
        <div class="styles-item-abc123 item-card">
            <a href="/item/123">Item 1</a>
        </div>
        <div class="another-class">
            <a href="/item/456">Item 2</a>
        </div>
        <div>
            <a href="/item/789">Item 3</a>
        </div>
    </html>
    """
    
    soup = BeautifulSoup(html_sample, 'html.parser')
    divs = soup.find_all("div")
    
    print("\n" + "="*60)
    print("ДЕМОНСТРАЦИЯ: Как работает div.get('class') в BeautifulSoup")
    print("="*60 + "\n")
    
    for i, div in enumerate(divs, 1):
        class_attr = div.get("class")
        print(f"Div #{i}:")
        print(f"  div.get('class') = {class_attr}")
        print(f"  type = {type(class_attr)}")
        
        if class_attr is not None:
            print(f"  isinstance(list) = {isinstance(class_attr, list)}")
            print(f"  'styles-item' in class_attr = {'styles-item' in class_attr}")
            # НЕПРАВИЛЬНАЯ проверка (ваша оригинальная)
            print(f"  'styles-item' in ' '.join(class_attr) = {'styles-item' in ' '.join(class_attr)}")
            # ПРАВИЛЬНАЯ проверка
            has_styles = any('styles-item' in cls for cls in class_attr)
            print(f"  any('styles-item' in cls for cls in class_attr) = {has_styles}")
        print()


if __name__ == "__main__":
    # Запускаем демонстрацию
    demonstrate_class_issue()
    
    print("\n" + "="*60)
    print("РЕЗЮМЕ ПРОБЛЕМЫ")
    print("="*60)
    print("""
1. В BeautifulSoup атрибут 'class' возвращается как СПИСОК строк, а не строка!
   
2. ПРОБЛЕМА в вашем коде:
   if "styles-item" in div.get("class"):
   
   Это проверяет, есть ли строка "styles-item" ТОЧНО в списке.
   Но классы на Avito обычно выглядят как "styles-item-abc123xyz",
   поэтому точного совпадения нет!

3. РЕШЕНИЕ #1: Проверять частичное вхождение
   class_list = div.get("class")
   if class_list and any("styles-item" in cls for cls in class_list):
       # нашли!
   
4. РЕШЕНИЕ #2: Использовать Playwright селекторы вместо BeautifulSoup
   items = await page.query_selector_all('[class*="styles-item"]')
   
5. ВАЖНО: Добавьте ожидание загрузки элементов:
   await page.wait_for_selector('[data-marker="item"]', timeout=5000)
""")
