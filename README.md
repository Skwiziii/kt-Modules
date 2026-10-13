# 🐾 kt-Modules — Каталог модулей Kotogram от @Diezdd

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Android%20arm64--v8a-brightgreen?style=for-the-badge&logo=android" alt="Platform" />
  <img src="https://img.shields.io/badge/ABI-Native%20ABI%203-blue?style=for-the-badge" alt="ABI" />
  <img src="https://img.shields.io/badge/Kotogram-v0.3.6+-orange?style=for-the-badge" alt="Kotogram Version" />
  <img src="https://img.shields.io/badge/Type-Proprietary-red?style=for-the-badge" alt="Type" />
</p>

Каталог пользовательских нативных модулей для клиента и юзербота **Kotogram / KoteLoader**.  
Все модули скомпилированы под архитектуру **Android arm64-v8a** (с поддержкой 16KB/64KB страниц для Android 15/16) и готовы к установке в 1 клик прямо внутри приложения или через Telegram.

---

## 📦 Каталог модулей

| Модуль | Тип | Версия | Команды | Описание |
| :--- | :---: | :---: | :--- | :--- |
| **🎭 [mimic](modules/mimic.so)** | `Native .so` | `v1.0.0` | `.clone` `[время] [@user]`<br>`.profiles`<br>`.loadprofile <слот>`<br>`.saveprofile <слот>`<br>`.newprofile ...`<br>`.ghost`<br>`.panic` | **Клонирование и менеджер профилей**: копирование чужого профиля по reply/@username (имя, био, аватарка), слоты личностей для быстрого переключения в 1 клик, режим невидимки (Ghost) и авто-возврат оригинала по таймеру. |
| **🎧 [nowplay](modules/nowplay.so)** | `Native .so` | `v1.6.5` | `.np` `[photo|bot|text]`<br>`.npmode <photo|bot|text>`<br>`.npbio [on|off|now]`<br>`.npformat <16:9|...>`<br>`.npplatform` | **Карточка текущего трека (Now Playing)**: карточка играющей музыки (Яндекс Музыка, Spotify, VK, SoundCloud, Last.fm) с обходом блокировок ссылок (режимы фото/бот/текст), прогресс-баром и трансляцией в Bio. |
| **🎵 [music_dl](modules/music_dl.so)** | `Native .so` | `v1.6.4` | `.find <запрос>`<br>`.fc [next|prev]`<br>`dlf <номер>`<br>`.download <запрос>`<br>`.musiclog`<br>`.favs` | **Музыкальный поисковик и загрузчик**: поиск и отправка аудиофайлов из SoundCloud с автоматическим самовосстановлением Client ID, интерактивными кнопками, многоуровневым скачиванием и встроенными логами. |
| **🎨 [love_canvas](modules/love_canvas.so)** | `Native .so` | `v1.0.6` | `.canvas`<br>`.canvas on / off`<br>`.canvas user <@user>`<br>`.canvas draw`<br>`.canvas test`<br>`.canvas clear` | **Плавающий оверлей-холст**: прозрачное окно поверх экрана (SYSTEM_ALERT_WINDOW, pass-through). При отправке белого холста от второй половинки автоматически удаляет белый фон и рисует арт поверх экрана. |
| **✍️ [autosig](modules/autosig.so)** | `Native .so` | `v1.0.0` | `.autosig` `[on|off]`<br>`.setsig <текст>`<br>`.getsig`<br>`.sigpos <start|end|both>`<br>`.sigchat` | **Автоматическая подпись сообщений**: автоматическое прикрепление подписи к исходящим сообщениям с поддержкой Markdown, ссылок, кастомных эмодзи и фильтрами чатов. |

---

## 🚀 Как установить модули

### Способ 1: Через приложение Kotogram (В 1 клик)
1. Откройте **Kotogram** на Android и перейдите во вкладку **Модули** → **Репозиторий** (или нажмите иконку добавления каталога).
2. Вставьте прямую ссылку на индекс репозитория:
   ```text
   https://raw.githubusercontent.com/Skwiziii/kt-Modules/main/index.json
   ```
3. Выберите нужный модуль из списка и нажмите **«Установить»**. Модуль мгновенно загрузится и активируется без перезагрузки приложения!

### Способ 2: Через команду `.dlm` в Telegram
Отправьте в любой чат команду со ссылкой на модуль:
```text
.dlm https://raw.githubusercontent.com/Skwiziii/kt-Modules/main/modules/mimic.so
```
или для карточки Now Play:
```text
.dlm https://raw.githubusercontent.com/Skwiziii/kt-Modules/main/modules/nowplay.so
```

---

## 🛠 Подробности модулей

### 🎭 Клонирование профиля (`mimic.so` v1.0.0)
* **Клонирование по Reply**: в ответ на любое сообщение копирует имя собеседника. Если ответ на фото — загружает его и ставит новой аватаркой!
* **Авто-бэкап оригинала**: при первом запуске автоматически сохраняет ваши настоящие имя и био в слот `original`.
* **Кнопка тревоги (`.panic` / `.resetprofile`)**: мгновенно возвращает ваш родной профиль в 1 клик.
* **Таймер авто-возврата**: команды вида `.clone 15m` или `.loadprofile troll 1h` возвращают ваш настоящий профиль автоматически, когда время истекает.
* **Режим Призрака (`.ghost`)**: скрывает имя невидимым символом (U+3164), очищает Bio и удаляет аватарку.

### 🎧 Карточка играющего трека (`nowplay.so` v1.6.5)
* **Мульти-режимы отправки (`.npmode`)**:
  - `photo` (по умолчанию) — отправка сгенерированной карточки как фото с описанием без кнопок и бота. **Обходит ошибку 403 `CHAT_SEND_WEBPAGE_FORBIDDEN` в любых группах!**
  - `bot` — отправка через инлайн-бота с баннером и интерактивными кнопками `[🔄 Обновить]` / `[❌ Закрыть]`.
  - `text` — компактный текстовый режим.
* **Поддержка платформ**: Яндекс Музыка, Spotify (через Stats.fm), VK Музыка, SoundCloud, Last.fm.
* **SongLink**: генерация мультиссылки на трек во всех плеерах.

---

**© 2026 @Diezdd. Разработано для Kotogram / KoteLoader.**
