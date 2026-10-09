// reswaga.cpp
// Модуль отправки карточки текущего играющего трека (Now Playing) для Kotogram (C-ABI 3).
// Перенесено из reswag.plugin.
// Поддерживаемые сервисы: Last.fm (любые плееры, Spotify, Yandex, Apple Music), Stats.fm (Spotify), Yandex Music.
// Автор: @Diezdd

#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include "module_abi.h"
#include "koto_utils.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace koto_utils;

static const char* DEFAULT_LASTFM_KEY = "19e0b83fb81f7043c02e4f070848a57a";

struct NowTrack {
    bool active = false;
    std::string title;
    std::string artist;
    std::string album;
    std::string image_url;
    std::string track_url;
    long long duration_sec = 0;
    long long progress_sec = 0;
    std::string platform = "Last.fm";
};

// ─── Получение трека из Last.fm ──────────────────────────────────────────────
static NowTrack fetch_lastfm(const koto_api* api, koto_ctx* ctx) {
    NowTrack t;
    std::string user = db_get_str(api, ctx, "lastfm_user", "");
    if (user.empty()) return t;

    std::string api_key = db_get_str(api, ctx, "lastfm_key", DEFAULT_LASTFM_KEY);
    std::string url = "http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&user=" +
                      url_encode(user) + "&api_key=" + api_key + "&format=json&limit=1";

    std::string body = http_get(url, {}, 6);
    if (body.empty()) return t;

    try {
        json j = json::parse(body);
        if (j.contains("recenttracks") && j["recenttracks"].contains("track")) {
            auto tracks = j["recenttracks"]["track"];
            json track;
            if (tracks.is_array() && !tracks.empty()) {
                track = tracks[0];
            } else if (tracks.is_object()) {
                track = tracks;
            } else {
                return t;
            }

            t.title = track.value("name", "");
            if (track.contains("artist")) {
                if (track["artist"].is_object()) {
                    t.artist = track["artist"].value("#text", "");
                } else if (track["artist"].is_string()) {
                    t.artist = track["artist"].get<std::string>();
                }
            }
            if (track.contains("album")) {
                if (track["album"].is_object()) {
                    t.album = track["album"].value("#text", "");
                } else if (track["album"].is_string()) {
                    t.album = track["album"].get<std::string>();
                }
            }

            t.track_url = track.value("url", "");

            if (track.contains("image") && track["image"].is_array()) {
                for (auto it = track["image"].rbegin(); it != track["image"].rend(); ++it) {
                    std::string img = it->value("#text", "");
                    if (!img.empty()) {
                        t.image_url = img;
                        break;
                    }
                }
            }

            // Проверяем, играет ли трек прямо сейчас
            if (track.contains("@attr") && track["@attr"].value("nowplaying", "") == "true") {
                t.active = true;
            } else {
                // Если нет флага nowplaying, трек был проигран недавно
                t.active = true;
            }
            t.platform = "Last.fm";
        }
    } catch (...) {}

    return t;
}

// ─── Получение трека из Spotify / Stats.fm ───────────────────────────────────
static NowTrack fetch_statsfm(const koto_api* api, koto_ctx* ctx) {
    NowTrack t;
    std::string user = db_get_str(api, ctx, "statsfm_user", "");
    if (user.empty()) return t;

    std::string url = "https://api.stats.fm/api/v1/users/" + url_encode(user) + "/streams/current";
    std::vector<std::string> headers = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Accept: application/json"
    };

    std::string body = http_get(url, headers, 6);
    if (body.empty()) return t;

    try {
        json j = json::parse(body);
        if (j.contains("item") && j["item"].is_object()) {
            auto item = j["item"];
            if (item.contains("track") && item["track"].is_object()) {
                auto tr = item["track"];
                t.title = tr.value("name", "");

                if (tr.contains("artists") && tr["artists"].is_array()) {
                    std::string artists;
                    for (const auto& a : tr["artists"]) {
                        if (!artists.empty()) artists += ", ";
                        artists += a.value("name", "");
                    }
                    t.artist = artists;
                }

                if (tr.contains("albums") && tr["albums"].is_array() && !tr["albums"].empty()) {
                    t.album = tr["albums"][0].value("name", "");
                    t.image_url = tr["albums"][0].value("image", "");
                }

                t.duration_sec = tr.value("durationMs", 0LL) / 1000;
                t.progress_sec = item.value("progressMs", 0LL) / 1000;

                std::string sp_id = "";
                if (tr.contains("externalIds") && tr["externalIds"].contains("spotify") && tr["externalIds"]["spotify"].is_array()) {
                    if (!tr["externalIds"]["spotify"].empty()) {
                        sp_id = tr["externalIds"]["spotify"][0].get<std::string>();
                    }
                }
                if (!sp_id.empty()) {
                    t.track_url = "https://open.spotify.com/track/" + sp_id;
                }

                t.active = true;
                t.platform = "Spotify";
            }
        }
    } catch (...) {}

    return t;
}

// ─── Главная функция определения текущего трека ────────────────────────────
static NowTrack get_current_track(const koto_api* api, koto_ctx* ctx) {
    std::string platform = db_get_str(api, ctx, "reswag_platform", "lastfm");
    if (platform == "spotify" || platform == "statsfm") {
        NowTrack t = fetch_statsfm(api, ctx);
        if (t.active) return t;
    }
    // Фолбэк на Last.fm
    return fetch_lastfm(api, ctx);
}

// ─── Команда .now / .np ──────────────────────────────────────────────────────
static void cmd_now(koto_ctx* ctx, const koto_api* api) {
    NowTrack track = get_current_track(api, ctx);

    if (!track.active || track.title.empty()) {
        std::string user_lfm = db_get_str(api, ctx, "lastfm_user", "");
        std::string user_sp = db_get_str(api, ctx, "statsfm_user", "");

        api->c_reset(ctx);
        if (user_lfm.empty() && user_sp.empty()) {
            api->c_fmt(ctx, "⚙️ Модуль reSwaga не настроен!\n", KOTO_ENT_BOLD);
            api->c_text(ctx, "Привяжите ваш аккаунт одной из команд:\n");
            api->c_fmt(ctx, "• .setlastfm <username>", KOTO_ENT_CODE);
            api->c_text(ctx, " (Last.fm — поддерживает любые плееры)\n");
            api->c_fmt(ctx, "• .setstatsfm <username>", KOTO_ENT_CODE);
            api->c_text(ctx, " (Spotify / Stats.fm)\n");
        } else {
            api->c_fmt(ctx, "💤 Сейчас ничего не воспроизводится.", KOTO_ENT_ITALIC);
        }
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    api->c_reset(ctx);
    api->c_emoji(ctx, "🎵", 5188621441926438751LL);
    api->c_fmt(ctx, " Сейчас играет: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, track.title.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, " — ");
    api->c_fmt(ctx, track.artist.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, "\n");

    if (!track.album.empty()) {
        api->c_text(ctx, "💿 Альбом: ");
        api->c_fmt(ctx, track.album.c_str(), KOTO_ENT_ITALIC);
        api->c_text(ctx, "\n");
    }

    // Если есть данные о таймлайне
    if (track.duration_sec > 0) {
        std::string bar = progress_bar(track.progress_sec, track.duration_sec, 10);
        std::string times = " [" + format_duration(track.progress_sec) + " / " +
                            format_duration(track.duration_sec) + "]\n";
        api->c_text(ctx, "▶️ ");
        api->c_fmt(ctx, bar.c_str(), KOTO_ENT_CODE);
        api->c_text(ctx, times.c_str());
    }

    // Ссылки на прослушивание
    api->c_text(ctx, "🔗 ");
    if (!track.track_url.empty()) {
        api->c_url(ctx, track.platform.c_str(), track.track_url.c_str());
        api->c_text(ctx, " • ");
    }

    // Мультиссылка SongLink
    std::string songlink_url = "https://song.link/s/" + url_encode(track.artist + " " + track.title);
    api->c_url(ctx, "SongLink", songlink_url.c_str());

    api->c_fmt(ctx, "\n\n🎧 reSwaga | @Diezdd", KOTO_ENT_ITALIC);

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .npcard (отправка с обложкой) ──────────────────────────────────
static void cmd_npcard(koto_ctx* ctx, const koto_api* api) {
    NowTrack track = get_current_track(api, ctx);

    if (!track.active || track.title.empty()) {
        cmd_now(ctx, api);
        return;
    }

    if (track.image_url.empty()) {
        // Если нет обложки, отправляем красивую текстовую карточку
        cmd_now(ctx, api);
        return;
    }

    std::string tmp_cover = "/tmp/np_cover_" + std::to_string(getpid()) + ".jpg";
    bool downloaded = download_file(track.image_url, tmp_cover, {}, 10);
    if (!downloaded) {
        cmd_now(ctx, api);
        return;
    }

    long long chat_id = api->chat_id(ctx);

    api->c_reset(ctx);
    api->c_emoji(ctx, "🎶", 5188705588925702510LL);
    api->c_fmt(ctx, " ", KOTO_ENT_NONE);
    api->c_fmt(ctx, track.title.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, " — ");
    api->c_fmt(ctx, track.artist.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, "\n");

    if (!track.album.empty()) {
        api->c_text(ctx, "💿 Альбом: ");
        api->c_fmt(ctx, track.album.c_str(), KOTO_ENT_ITALIC);
        api->c_text(ctx, "\n");
    }

    if (track.duration_sec > 0) {
        std::string bar = progress_bar(track.progress_sec, track.duration_sec, 8);
        api->c_text(ctx, "▶️ ");
        api->c_fmt(ctx, bar.c_str(), KOTO_ENT_CODE);
        api->c_text(ctx, (" [" + format_duration(track.progress_sec) + "/" + format_duration(track.duration_sec) + "]\n").c_str());
    }

    api->c_text(ctx, "🔗 ");
    if (!track.track_url.empty()) {
        api->c_url(ctx, track.platform.c_str(), track.track_url.c_str());
        api->c_text(ctx, " • ");
    }
    std::string songlink_url = "https://song.link/s/" + url_encode(track.artist + " " + track.title);
    api->c_url(ctx, "SongLink", songlink_url.c_str());

    // Отправляем фото обложки с подписью
    api->c_send_file(ctx, chat_id, tmp_cover.c_str(), 0);

    // Удаляем сообщение команды и временный файл
    api->delete_msg(ctx);
    unlink(tmp_cover.c_str());
}

// ─── Команда .setlastfm ─────────────────────────────────────────────────────
static void cmd_setlastfm(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".setlastfm <username> [api_key]", KOTO_ENT_CODE);
        api->c_text(ctx, "\nПривязывает ваш аккаунт Last.fm к reSwaga.");
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    std::string s = trim(args);
    std::vector<std::string> parts = split(s, ' ');

    std::string user = parts[0];
    db_set_str(api, ctx, "lastfm_user", user);
    db_set_str(api, ctx, "reswag_platform", "lastfm");

    if (parts.size() >= 2) {
        db_set_str(api, ctx, "lastfm_key", parts[1]);
    }

    api->c_reset(ctx);
    api->c_fmt(ctx, "✅ Аккаунт Last.fm успешно привязан:\n", KOTO_ENT_BOLD);
    api->c_text(ctx, "• Пользователь: ");
    api->c_fmt(ctx, user.c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Теперь команда ");
    api->c_fmt(ctx, ".now", KOTO_ENT_CODE);
    api->c_text(ctx, " транслирует текущий трек!");

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .setstatsfm ────────────────────────────────────────────────────
static void cmd_setstatsfm(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".setstatsfm <username>", KOTO_ENT_CODE);
        api->c_text(ctx, "\nПривязывает профиль Stats.fm (Spotify).");
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    std::string user = trim(args);
    db_set_str(api, ctx, "statsfm_user", user);
    db_set_str(api, ctx, "reswag_platform", "spotify");

    api->c_reset(ctx);
    api->c_fmt(ctx, "✅ Аккаунт Stats.fm (Spotify) привязан: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, user.c_str(), KOTO_ENT_CODE);
    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .npplatform ────────────────────────────────────────────────────
static void cmd_npplatform(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    std::string p = args ? trim(args) : "";

    if (p != "lastfm" && p != "spotify") {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".npplatform <lastfm|spotify>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    db_set_str(api, ctx, "reswag_platform", p);

    api->c_reset(ctx);
    api->c_fmt(ctx, "✅ Платформа переключена на: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, p.c_str(), KOTO_ENT_CODE);
    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .npinfo ────────────────────────────────────────────────────────
static void cmd_npinfo(koto_ctx* ctx, const koto_api* api) {
    std::string p = db_get_str(api, ctx, "reswag_platform", "lastfm");
    std::string lfm = db_get_str(api, ctx, "lastfm_user", "<не задан>");
    std::string sp = db_get_str(api, ctx, "statsfm_user", "<не задан>");

    api->c_reset(ctx);
    api->c_fmt(ctx, "🎧 reSwaga Module SDK (C++)\n", KOTO_ENT_BOLD);
    api->c_text(ctx, "• Автор: ");
    api->c_fmt(ctx, "@Diezdd", KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Активная платформа: ");
    api->c_fmt(ctx, p.c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Last.fm пользователь: ");
    api->c_fmt(ctx, lfm.c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Stats.fm пользователь: ");
    api->c_fmt(ctx, sp.c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n\nКоманды:\n");
    api->c_text(ctx, "  - .now — текстовая карточка текущего трека\n");
    api->c_text(ctx, "  - .npcard — карточка с обложкой альбома\n");
    api->c_text(ctx, "  - .setlastfm <username> — настроить Last.fm\n");
    api->c_text(ctx, "  - .setstatsfm <username> — настроить Stats.fm");

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Таблица команд ────────────────────────────────────────────────────────
static const koto_command COMMANDS[] = {
    {
        "now",
        &cmd_now,
        KOTO_LEVEL_ALL,
        "Показать карточку текущего воспроизводимого трека",
        NULL
    },
    {
        "np",
        &cmd_now,
        KOTO_LEVEL_ALL,
        "Короткий алиас для команды .now",
        NULL
    },
    {
        "npcard",
        &cmd_npcard,
        KOTO_LEVEL_ALL,
        "Отправить карточку текущего трека с обложкой альбома",
        NULL
    },
    {
        "setlastfm",
        &cmd_setlastfm,
        KOTO_LEVEL_ALL,
        "Привязать аккаунт Last.fm к юзерботу",
        "<username> [api_key]"
    },
    {
        "setstatsfm",
        &cmd_setstatsfm,
        KOTO_LEVEL_ALL,
        "Привязать аккаунт Stats.fm (Spotify)",
        "<username>"
    },
    {
        "npplatform",
        &cmd_npplatform,
        KOTO_LEVEL_ALL,
        "Выбрать источник музыки (lastfm, spotify)",
        "<lastfm|spotify>"
    },
    {
        "npinfo",
        &cmd_npinfo,
        KOTO_LEVEL_ALL,
        "Справка и статус музыкального стримера reSwaga",
        NULL
    }
};

// ─── Настройки .cfg ────────────────────────────────────────────────────────
static const koto_setting_field SETTINGS[] = {
    {
        "reswag_platform",
        "Источник музыки",
        KOTO_FIELD_CHOICE,
        "lastfm",
        "lastfm,spotify"
    },
    {
        "lastfm_user",
        "Last.fm Username",
        KOTO_FIELD_STR,
        "",
        NULL
    },
    {
        "statsfm_user",
        "Stats.fm (Spotify) Username",
        KOTO_FIELD_STR,
        "",
        NULL
    }
};

// ─── Экспорт модуля ────────────────────────────────────────────────────────
static const koto_module MODULE = {
    KOTO_MODULE_ABI,
    "reswaga",
    "Карточка текущего играющего трека (Now Playing) для Last.fm и Spotify (@Diezdd)",
    "1.0.0",
    2,
    0,
    COMMANDS,
    sizeof(COMMANDS) / sizeof(COMMANDS[0]),
    NULL, 0,
    NULL,
    SETTINGS,
    sizeof(SETTINGS) / sizeof(SETTINGS[0]),
    "https://github.com/Skwiziii/kt-Modules"
};

extern "C" const koto_module* koto_module_register(void) {
    return &MODULE;
}

