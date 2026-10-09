# 🐾 kt-Modules — Каталог модулей Kotogram от @Diezdd

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Android%20arm64--v8a-brightgreen?style=for-the-badge&logo=android" alt="Platform" />
  <img src="https://img.shields.io/badge/ABI-Native%20ABI%203-blue?style=for-the-badge" alt="ABI" />
  <img src="https://img.shields.io/badge/Kotogram-v0.1.1+-orange?style=for-the-badge" alt="Kotogram Version" />
  <img src="https://img.shields.io/badge/Type-Proprietary-red?style=for-the-badge" alt="Type" />
</p>

Каталог пользовательских нативных модулей для клиента и юзербота **Kotogram / KoteLoader**.  
Все модули скомпилированы под архитектуру **Android arm64-v8a** и готовы к установке в 1 клик прямо внутри приложения или через Telegram.

---

## 📦 Каталог модулей

| Модуль | Тип | Версия | Команды | Описание |
| :--- | :---: | :---: | :--- | :--- |
| **✍️ [autosig](modules/autosig.so)** | `Native .so` | `v1.0.0` | `.autosig` `[on\|off]`<br>`.setsig <текст>`<br>`.getsig`<br>`.sigpos <start\|end\|both>`<br>`.sigchat` | **Автоматическая подпись сообщений**: автоматическое прикрепление подписи к исходящим сообщениям с поддержкой Markdown, ссылок, кастомных эмодзи и фильтрами чатов (ЛС, группы, каналы). |
| **🎵 [music_dl](modules/music_dl.so)** | `Native .so` | `v1.0.0` | `.find <запрос>`<br>`.fc [next\|prev]`<br>`dlf <номер>`<br>`.download <запрос>`<br>`.like`<br>`.favs` | **Музыкальный поисковик и загрузчик**: поиск и отправка аудиофайлов из SoundCloud с автоматическим самовосстановлением Client ID, пагинацией и списком «Избранное». |
| **🎧 [reswaga](modules/reswaga.so)** | `Native .so` | `v1.0.0` | `.now` / `.np`<br>`.npcard`<br>`.setlastfm <user>`<br>`.setstatsfm <user>`<br>`.npplatform` | **Карточка текущего трека (Now Playing)**: стильная карточка играющей музыки для Last.fm (любые плееры), Spotify / Stats.fm с таймлайном и мультиссылками SongLink. |

---

## 🚀 Как установить модули

### Способ 1: Через приложение Kotogram (В 1 клик)
1. Откройте **Kotogram** на Android и перейдите во вкладку **Модули** → **Репозиторий** (или нажмите иконку добавления каталога).
2. Вставьте прямую ссылку на индекс репозитория:
   ```text
   https://raw.githubusercontent.com/Skwiziii/kt-Modules/main/index.json
   ```
3. Выберите нужный модуль из списка и нажмите **«Установить»**. Модуль мгновенно загрузится и активируется без перезагрузки приложения!

### Способ 2: Через Telegram (команда `.install`)
1. Скачайте файл `.so` из папки [`modules/`](modules/) (например, [`autosig.so`](modules/autosig.so), [`music_dl.so`](modules/music_dl.so) или [`reswaga.so`](modules/reswaga.so)).
2. Отправьте файл себе в «Избранное» (Saved Messages) или в любой чат как документ.
3. Ответьте на сообщение с файлом командой:
   ```text
   .install
   ```
4. Юзербот подтвердит установку, и команды сразу появятся в `.help` и `.modules`.

---

## 🛠 Подробности модулей

### ✍️ Автоподпись (`autosig.so`)
* Перехватывает исходящие сообщения через хук `on_watch` (`KOTO_WATCH_OUTGOING`).
* Защита от порчи команд юзербота: сообщения, начинающиеся с `.`, пропускаются без изменений.
* Персональные подписи для конкретных диалогов: `.sigchat here <текст>` задает индивидуальную подпись для чата.
* Интерактивные тумблеры в меню `.cfg`: раздельное включение для ЛС, обычных групп и каналов.

### 🎵 Загрузчик музыки (`music_dl.so`)
* **Автоматическое обновление Client ID SoundCloud**: при истечении ключа модуль динамически считывает актуальные JS-бандлы с `soundcloud.com` и обновляет ключ в локальной SQLite базе данных без сбоев.
* Скачивает поток и отправляет нативный аудиофайл через `c_send_file` с автоматической очисткой временных файлов.
* Полноценная пагинация `.fc next` / `.fc prev` и сохранение сессии поиска.
* Локальное хранилище избранных треков (`.like` / `.favs`).

### 🎧 Карточка играющего трека (`reswaga.so`)
* **Last.fm интеграция**: универсально работает с любыми плеерами и сервисами (Яндекс.Музыка, Spotify, Apple Music, VK, плееры на Android и ПК). Достаточно указать никнейм: `.setlastfm <username>`.
* **Stats.fm (Spotify)**: прямое получение текущего стрима по имени пользователя: `.setstatsfm <username>`.
* **SongLink**: формирует мультиссылку `https://song.link/...`, позволяющую слушателям открыть найденный трек в любом своём стриминге в 1 клик.
* **Обложки альбомов (`.npcard`)**: загрузка оригинальной обложки альбома в высоком качестве и отправка с форматированной подписью.

---

**© 2026 @Diezdd. Разработано для Kotogram / KoteLoader.**

