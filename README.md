# EncTags — Notepad++ Plugin

Шифрование фрагментов текста внутри файлов с помощью тегов `^^...^^`

## Сборка

### Требования
- Visual Studio 2022 (Community подойдёт)
- Windows SDK 10.0
- Компонент "Разработка классических приложений C++"

### Шаги
1. Открыть `EncTags.sln` в Visual Studio
2. Выбрать конфигурацию **Release | x64**
3. Build → Build Solution (Ctrl+Shift+B)
4. Готовый файл: `x64\Release\EncTags.dll`

### Установка в Notepad++
1. Создать папку: `<путь к Notepad++>\plugins\EncTags\`
2. Скопировать `EncTags.dll` в эту папку
3. Перезапустить Notepad++
4. Меню: Plugins → EncTags

## Использование

### Шифрование
1. Выделить текст
2. `Ctrl+Shift+E` или Plugins → EncTags → Зашифровать выделение
3. Ввести пароль
4. Выделенный текст заменяется на `^^L1:base64data^^`

### Расшифровка
1. `Ctrl+Shift+D` или Plugins → EncTags → Расшифровать всё
2. Ввести пароль
3. Все теги `^^...^^` заменяются расшифрованным текстом
4. Расшифрованные фрагменты подсвечиваются зелёным

### Сохранение
- При `Ctrl+S` плагин **автоматически** шифрует все расшифрованные фрагменты обратно
- После сохранения — снова показывает расшифрованный текст

### Блокировка
- `Ctrl+Shift+L` — вручную зашифровать всё обратно без сохранения файла

### Снятие шифрования
- Поставить курсор внутрь расшифрованного фрагмента
- Plugins → EncTags → Снять шифрование
- При сохранении текст останется как есть (без шифрования)

## Формат тега

```
^^L1:<base64_blob>^^
```

Blob содержит (в бинарном виде, закодированном в base64):
- Salt: 16 байт (PBKDF2)
- Nonce: 12 байт (AES-GCM)
- Ciphertext: переменная длина
- Auth Tag: 16 байт (AES-GCM)

## Криптография

- **KDF:** PBKDF2-SHA256, 100 000 итераций
- **Шифрование:** AES-256-GCM
- **Реализация:** Windows BCrypt API (нулевые внешние зависимости)

## Структура проекта

```
enctags-npp/
├── EncTags.sln              — Visual Studio solution
├── EncTags.vcxproj          — Visual Studio project
├── include/
│   ├── PluginInterface.h    — Notepad++ plugin API
│   └── Scintilla.h          — Scintilla messages
├── src/
│   ├── PluginMain.cpp       — DLL entry, menu, notifications
│   ├── EncTagsEngine.h/cpp  — AES-256-GCM + PBKDF2
│   ├── TagParser.h/cpp      — поиск ^^...^^ в тексте
│   ├── FragmentRegistry.h/cpp — реестр расшифрованных фрагментов
│   ├── resource.h           — ID ресурсов
│   ├── EncTags.rc           — диалог пароля
│   └── EncTags.def          — DLL exports
└── README.md
```

## Лицензия

MIT
