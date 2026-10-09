// music_dl.cpp
// Полнофункциональный модуль поиска и загрузки музыки для Kotogram (C-ABI 3).
// Перенесено из music_dl.plugin.
// Автор: @Diezdd

#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <unistd.h>
#include "module_abi.h"
#include "koto_utils.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace koto_utils;

struct TrackInfo {
    std::string id;
    std::string title;
    std::string artist;
    long long duration_sec = 0;
    std::string stream_url;
    std::string artwork_url;
    long long likes = 0;
    std::string service = "sc";
};

// ─── SoundCloud Client ID Management ─────────────────────────────────────────
static std::string sc_parse_client_id() {
    std::vector<std::string> headers = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Referer: https://soundcloud.com/"
    };
    std::string html = http_get("https://soundcloud.com", headers, 8);
    if (html.empty()) return "";

    std::regex js_regex(R"(https://a-v2\.sndcdn\.com/assets/[^"'\s]+\.js)");
    auto words_begin = std::sregex_iterator(html.begin(), html.end(), js_regex);
    auto words_end = std::sregex_iterator();

    std::vector<std::string> js_urls;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        js_urls.push_back((*i).str());
    }

    std::regex cid_regex(R"raw(client_id\s*:\s*"([a-zA-Z0-9]{32})")raw");
    for (auto it = js_urls.rbegin(); it != js_urls.rend(); ++it) {
        std::string js = http_get(*it, headers, 6);
        std::smatch match;
        if (std::regex_search(js, match, cid_regex) && match.size() > 1) {
            return match[1].str();
        }
    }
    return "";
}

static std::string sc_get_cid(const koto_api* api, koto_ctx* ctx) {
    std::string custom = cfg_get_str(api, ctx, "sc_cid", "");
    if (!custom.empty()) return custom;

    std::string cached = db_get_str(api, ctx, "sc_cached_cid", "");
    if (!cached.empty()) return cached;

    std::string parsed = sc_parse_client_id();
    if (!parsed.empty()) {
        db_set_str(api, ctx, "sc_cached_cid", parsed);
        return parsed;
    }

    // Запасной ключ
    return "T574VRKI5KSLMzLdUreWOpXss1R3Fzfx";
}

// ─── SoundCloud Search ───────────────────────────────────────────────────────
static std::vector<TrackInfo> soundcloud_search(const koto_api* api, koto_ctx* ctx, const std::string& query, int limit = 15) {
    std::vector<TrackInfo> results;
    std::string cid = sc_get_cid(api, ctx);

    std::vector<std::string> headers = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Referer: https://soundcloud.com/"
    };

    std::string url = "https://api-v2.soundcloud.com/search/tracks?q=" + url_encode(query) +
                      "&client_id=" + cid + "&limit=" + std::to_string(limit) + "&offset=0";

    std::string body = http_get(url, headers, 10);
    if (body.empty() || body.find("401") != std::string::npos) {
        // Пробуем обновить client_id если старый протух
        std::string fresh = sc_parse_client_id();
        if (!fresh.empty() && fresh != cid) {
            db_set_str(api, ctx, "sc_cached_cid", fresh);
            cid = fresh;
            url = "https://api-v2.soundcloud.com/search/tracks?q=" + url_encode(query) +
                  "&client_id=" + cid + "&limit=" + std::to_string(limit) + "&offset=0";
            body = http_get(url, headers, 10);
        }
    }

    try {
        json j = json::parse(body);
        if (j.contains("collection") && j["collection"].is_array()) {
            for (const auto& item : j["collection"]) {
                TrackInfo t;
                t.id = std::to_string(item.value("id", 0LL));
                t.title = item.value("title", "Unknown Title");

                if (item.contains("user") && item["user"].is_object()) {
                    t.artist = item["user"].value("username", "Unknown Artist");
                } else {
                    t.artist = "SoundCloud";
                }

                long long dur_ms = item.value("duration", 0LL);
                t.duration_sec = dur_ms / 1000;
                t.likes = item.value("likes_count", 0LL);
                t.artwork_url = item.value("artwork_url", "");
                t.service = "sc";

                // Поиск прогрессивного mp3 потока
                if (item.contains("media") && item["media"].contains("transcodings") && item["media"]["transcodings"].is_array()) {
                    std::string prog_url;
                    std::string hls_url;
                    for (const auto& tr : item["media"]["transcodings"]) {
                        std::string format = tr.value("format", json::object()).value("protocol", "");
                        std::string tr_url = tr.value("url", "");
                        if (format == "progressive" && prog_url.empty()) {
                            prog_url = tr_url;
                        } else if (format == "hls" && hls_url.empty()) {
                            hls_url = tr_url;
                        }
                    }

                    std::string chosen_tr = !prog_url.empty() ? prog_url : hls_url;
                    if (!chosen_tr.empty()) {
                        std::string stream_req = chosen_tr + "?client_id=" + cid;
                        std::string stream_res = http_get(stream_req, headers, 6);
                        try {
                            json sj = json::parse(stream_res);
                            t.stream_url = sj.value("url", "");
                        } catch (...) {}
                    }
                }

                results.push_back(t);
            }
        }
    } catch (...) {}

    return results;
}

// ─── Сериализация и хранение сессии поиска ──────────────────────────────────
static void save_search_session(const koto_api* api, koto_ctx* ctx, long long chat_id, const std::string& query,
                                int page, int per_page, const std::vector<TrackInfo>& tracks) {
    json j;
    j["query"] = query;
    j["page"] = page;
    j["per_page"] = per_page;
    j["tracks"] = json::array();

    for (const auto& t : tracks) {
        json tj;
        tj["id"] = t.id;
        tj["title"] = t.title;
        tj["artist"] = t.artist;
        tj["duration_sec"] = t.duration_sec;
        tj["stream_url"] = t.stream_url;
        tj["artwork_url"] = t.artwork_url;
        tj["likes"] = t.likes;
        tj["service"] = t.service;
        j["tracks"].push_back(tj);
    }

    std::string key = "s_sess_" + std::to_string(chat_id);
    db_set_str(api, ctx, key, j.dump());
}

static bool load_search_session(const koto_api* api, koto_ctx* ctx, long long chat_id, std::string& query,
                                int& page, int& per_page, std::vector<TrackInfo>& tracks) {
    std::string key = "s_sess_" + std::to_string(chat_id);
    std::string data = db_get_str(api, ctx, key, "");
    if (data.empty()) return false;

    try {
        json j = json::parse(data);
        query = j.value("query", "");
        page = j.value("page", 1);
        per_page = j.value("per_page", 5);
        tracks.clear();

        if (j.contains("tracks") && j["tracks"].is_array()) {
            for (const auto& tj : j["tracks"]) {
                TrackInfo t;
                t.id = tj.value("id", "");
                t.title = tj.value("title", "");
                t.artist = tj.value("artist", "");
                t.duration_sec = tj.value("duration_sec", 0LL);
                t.stream_url = tj.value("stream_url", "");
                t.artwork_url = tj.value("artwork_url", "");
                t.likes = tj.value("likes", 0LL);
                t.service = tj.value("service", "sc");
                tracks.push_back(t);
            }
        }
        return !tracks.empty();
    } catch (...) {
        return false;
    }
}

// ─── Отрисовка страницы результатов ─────────────────────────────────────────
static void render_page(koto_ctx* ctx, const koto_api* api, const std::string& query,
                        int page, int per_page, const std::vector<TrackInfo>& tracks) {
    int total_tracks = static_cast<int>(tracks.size());
    int total_pages = (total_tracks + per_page - 1) / per_page;
    if (total_pages < 1) total_pages = 1;
    if (page < 1) page = 1;
    if (page > total_pages) page = total_pages;

    int start_idx = (page - 1) * per_page;
    int end_idx = std::min(start_idx + per_page, total_tracks);

    api->c_reset(ctx);
    api->c_emoji(ctx, "🔍", 5188621441926438751LL);
    api->c_fmt(ctx, " Результаты поиска: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, query.c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n\n");

    for (int i = start_idx; i < end_idx; ++i) {
        const auto& t = tracks[i];
        std::string num = std::to_string(i + 1) + ". ";
        api->c_fmt(ctx, num.c_str(), KOTO_ENT_BOLD);

        if (t.service == "sc") {
            api->c_text(ctx, "☁️ ");
        } else if (t.service == "sp") {
            api->c_text(ctx, "🟢 ");
        } else {
            api->c_text(ctx, "🎵 ");
        }

        api->c_fmt(ctx, t.title.c_str(), KOTO_ENT_BOLD);
        api->c_text(ctx, " — ");
        api->c_fmt(ctx, t.artist.c_str(), KOTO_ENT_ITALIC);

        std::string dur = " [" + format_duration(t.duration_sec) + "]";
        api->c_fmt(ctx, dur.c_str(), KOTO_ENT_CODE);

        if (t.likes > 0) {
            std::string likes_str = " (" + std::to_string(t.likes) + " ❤️)";
            api->c_text(ctx, likes_str.c_str());
        }
        api->c_text(ctx, "\n");
    }

    std::string footer = "\n📄 Страница " + std::to_string(page) + "/" + std::to_string(total_pages) +
                         " • Всего треков: " + std::to_string(total_tracks) + "\n";
    api->c_fmt(ctx, footer.c_str(), KOTO_ENT_ITALIC);

    api->c_fmt(ctx, "💡 Скачать: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, "dlf <номер>", KOTO_ENT_CODE);
    api->c_text(ctx, " | ");
    api->c_fmt(ctx, ".fc next", KOTO_ENT_CODE);
    api->c_text(ctx, " / ");
    api->c_fmt(ctx, ".fc prev", KOTO_ENT_CODE);

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .find ──────────────────────────────────────────────────────────
static void cmd_find(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".find <название трека или исполнитель>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    std::string query = trim(args);
    api->c_reset(ctx);
    api->c_fmt(ctx, "🔎 Ищу в SoundCloud: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, query.c_str(), KOTO_ENT_CODE);
    api->c_fmt(ctx, "...", KOTO_ENT_ITALIC);
    api->c_edit(ctx, KOTO_FMT_PLAIN);

    auto tracks = soundcloud_search(api, ctx, query, 15);
    if (tracks.empty()) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "❌ По запросу ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, query.c_str(), KOTO_ENT_CODE);
        api->c_fmt(ctx, " ничего не найдено.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    long long chat_id = api->chat_id(ctx);
    int per_page = static_cast<int>(db_get_int64(api, ctx, "per_page", 5));
    if (per_page < 1 || per_page > 10) per_page = 5;

    save_search_session(api, ctx, chat_id, query, 1, per_page, tracks);
    render_page(ctx, api, query, 1, per_page, tracks);
}

// ─── Команда .fc (пагинация) ────────────────────────────────────────────────
static void cmd_fc(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    std::string action = args ? trim(args) : "next";

    long long chat_id = api->chat_id(ctx);
    std::string query;
    int page = 1;
    int per_page = 5;
    std::vector<TrackInfo> tracks;

    if (!load_search_session(api, ctx, chat_id, query, page, per_page, tracks)) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Нет активного сеанса поиска. Сначала выполните: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".find <запрос>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    int total_pages = (static_cast<int>(tracks.size()) + per_page - 1) / per_page;

    if (action == "next") {
        if (page < total_pages) page++;
        else {
            api->c_reset(ctx);
            api->c_fmt(ctx, "ℹ️ Вы уже на последней странице.", KOTO_ENT_ITALIC);
            api->c_edit(ctx, KOTO_FMT_PLAIN);
            return;
        }
    } else if (action == "prev" || action == "previous") {
        if (page > 1) page--;
        else {
            api->c_reset(ctx);
            api->c_fmt(ctx, "ℹ️ Вы уже на первой странице.", KOTO_ENT_ITALIC);
            api->c_edit(ctx, KOTO_FMT_PLAIN);
            return;
        }
    }

    save_search_session(api, ctx, chat_id, query, page, per_page, tracks);
    render_page(ctx, api, query, page, per_page, tracks);
}

// ─── Команда dlf (скачивание трека) ─────────────────────────────────────────
static void cmd_dlf(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, "dlf <номер>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    int idx = 0;
    try {
        idx = std::stoi(trim(args)) - 1;
    } catch (...) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Укажите корректный номер трека из списка.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    long long chat_id = api->chat_id(ctx);
    std::string query;
    int page = 1;
    int per_page = 5;
    std::vector<TrackInfo> tracks;

    if (!load_search_session(api, ctx, chat_id, query, page, per_page, tracks)) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Нет активного сеанса поиска. Сделайте: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".find <запрос>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    if (idx < 0 || idx >= static_cast<int>(tracks.size())) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Номер трека вне диапазона (1..", KOTO_ENT_BOLD);
        api->c_text(ctx, std::to_string(tracks.size()).c_str());
        api->c_fmt(ctx, ").", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    const auto& track = tracks[idx];

    if (track.stream_url.empty()) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "❌ Не удалось получить прямую аудиоссылку для этого трека.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    api->c_reset(ctx);
    api->c_fmt(ctx, "⏳ Загружаю аудио: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, track.title.c_str(), KOTO_ENT_ITALIC);
    api->c_text(ctx, "...\n");
    api->c_edit(ctx, KOTO_FMT_PLAIN);

    std::string tmp_path = "/tmp/kototrack_" + std::to_string(getpid()) + "_" + track.id + ".mp3";
    std::vector<std::string> headers = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
    };

    bool ok = download_file(track.stream_url, tmp_path, headers, 45);
    if (!ok) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "❌ Ошибка при скачивании аудиофайла.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    // Составляем подпись к файлу
    api->c_reset(ctx);
    api->c_emoji(ctx, "🎵", 5188621441926438751LL);
    api->c_fmt(ctx, " ", KOTO_ENT_NONE);
    api->c_fmt(ctx, track.title.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, " — ");
    api->c_fmt(ctx, track.artist.c_str(), KOTO_ENT_ITALIC);
    api->c_text(ctx, "\n⏱ Длительность: ");
    api->c_fmt(ctx, format_duration(track.duration_sec).c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n🎧 Источник: SoundCloud | @Diezdd");

    // Отправляем файл
    api->c_send_file(ctx, chat_id, tmp_path.c_str(), 0);

    // Удаляем сообщение-статус
    api->delete_msg(ctx);
    unlink(tmp_path.c_str());
}

// ─── Команда .download (быстрый поиск + скачивание) ─────────────────────────
static void cmd_download(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".download <название трека>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    std::string query = trim(args);
    api->c_reset(ctx);
    api->c_fmt(ctx, "⚡️ Быстрый поиск и скачивание: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, query.c_str(), KOTO_ENT_CODE);
    api->c_edit(ctx, KOTO_FMT_PLAIN);

    auto tracks = soundcloud_search(api, ctx, query, 1);
    if (tracks.empty() || tracks[0].stream_url.empty()) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "❌ Трек не найден в сети.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    const auto& track = tracks[0];
    std::string tmp_path = "/tmp/kototrack_" + std::to_string(getpid()) + "_" + track.id + ".mp3";
    std::vector<std::string> headers = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
    };

    bool ok = download_file(track.stream_url, tmp_path, headers, 45);
    if (!ok) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "❌ Ошибка скачивания аудиофайла.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    long long chat_id = api->chat_id(ctx);
    api->c_reset(ctx);
    api->c_emoji(ctx, "🎧", 5188621441926438751LL);
    api->c_fmt(ctx, " ", KOTO_ENT_NONE);
    api->c_fmt(ctx, track.title.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, " — ");
    api->c_fmt(ctx, track.artist.c_str(), KOTO_ENT_ITALIC);
    api->c_text(ctx, "\n⏱ Длительность: ");
    api->c_fmt(ctx, format_duration(track.duration_sec).c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n📥 Загружено через @Diezdd");

    api->c_send_file(ctx, chat_id, tmp_path.c_str(), 0);
    api->delete_msg(ctx);
    unlink(tmp_path.c_str());
}

// ─── Команда .like / .favs ──────────────────────────────────────────────────
static void cmd_like(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".like <номер из .find>", KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    int idx = 0;
    try { idx = std::stoi(trim(args)) - 1; } catch (...) { return; }

    long long chat_id = api->chat_id(ctx);
    std::string query;
    int page = 1, per_page = 5;
    std::vector<TrackInfo> tracks;

    if (!load_search_session(api, ctx, chat_id, query, page, per_page, tracks) ||
        idx < 0 || idx >= static_cast<int>(tracks.size())) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Трек не найден в результатах поиска.", KOTO_ENT_BOLD);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    const auto& t = tracks[idx];
    std::string favs_str = db_get_str(api, ctx, "liked_tracks", "[]");
    json fj = json::parse(favs_str, nullptr, false);
    if (!fj.is_array()) fj = json::array();

    json item;
    item["id"] = t.id;
    item["title"] = t.title;
    item["artist"] = t.artist;
    item["duration_sec"] = t.duration_sec;
    fj.push_back(item);

    db_set_str(api, ctx, "liked_tracks", fj.dump());

    api->c_reset(ctx);
    api->c_fmt(ctx, "❤️ Трек добавлен в Избранное:\n", KOTO_ENT_BOLD);
    api->c_fmt(ctx, t.title.c_str(), KOTO_ENT_BOLD);
    api->c_text(ctx, " — ");
    api->c_fmt(ctx, t.artist.c_str(), KOTO_ENT_ITALIC);
    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

static void cmd_favs(koto_ctx* ctx, const koto_api* api) {
    std::string favs_str = db_get_str(api, ctx, "liked_tracks", "[]");
    json fj = json::parse(favs_str, nullptr, false);

    api->c_reset(ctx);
    api->c_fmt(ctx, "⭐️ Избранные треки (@Diezdd):\n\n", KOTO_ENT_BOLD);

    if (!fj.is_array() || fj.empty()) {
        api->c_text(ctx, "Список избранного пуст. Добавляйте треки командой .like <номер>");
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    int count = 1;
    for (const auto& item : fj) {
        std::string title = item.value("title", "Unknown");
        std::string artist = item.value("artist", "Unknown");
        long long dur = item.value("duration_sec", 0LL);

        std::string line = std::to_string(count++) + ". " + title + " — " + artist +
                           " [" + format_duration(dur) + "]\n";
        api->c_text(ctx, line.c_str());
        if (count > 25) break;
    }

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .musicinfo ─────────────────────────────────────────────────────
static void cmd_musicinfo(koto_ctx* ctx, const koto_api* api) {
    std::string cid = sc_get_cid(api, ctx);

    api->c_reset(ctx);
    api->c_fmt(ctx, "🎵 Music Downloader SDK (C++)\n", KOTO_ENT_BOLD);
    api->c_text(ctx, "• Автор: ");
    api->c_fmt(ctx, "@Diezdd", KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Сервис по умолчанию: ");
    api->c_fmt(ctx, "SoundCloud ☁️", KOTO_ENT_CODE);
    api->c_text(ctx, "\n• SoundCloud Client ID: ");
    api->c_fmt(ctx, (cid.substr(0, 8) + "...").c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Команды:\n");
    api->c_text(ctx, "  - .find <запрос> — поиск треков\n");
    api->c_text(ctx, "  - dlf <номер> — скачать трек\n");
    api->c_text(ctx, "  - .download <запрос> — быстрый поиск + загрузка\n");
    api->c_text(ctx, "  - .favs — список избранного\n");
    api->c_text(ctx, "  - .like <номер> — лайкнуть трек");

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Таблица команд ────────────────────────────────────────────────────────
static const koto_command COMMANDS[] = {
    {
        "find",
        &cmd_find,
        KOTO_LEVEL_ALL,
        "Поиск музыки по названию или исполнителю",
        "<запрос>"
    },
    {
        "fc",
        &cmd_fc,
        KOTO_LEVEL_ALL,
        "Перелистывание страниц результатов поиска (next/prev)",
        "[next|prev]"
    },
    {
        "dlf",
        &cmd_dlf,
        KOTO_LEVEL_ALL,
        "Скачать трек по номеру из результатов поиска",
        "<номер>"
    },
    {
        "download",
        &cmd_download,
        KOTO_LEVEL_ALL,
        "Быстрый поиск и скачивание первого трека",
        "<запрос>"
    },
    {
        "msc",
        &cmd_download,
        KOTO_LEVEL_ALL,
        "Короткий алиас для команды .download",
        "<запрос>"
    },
    {
        "like",
        &cmd_like,
        KOTO_LEVEL_ALL,
        "Добавить трек из текущего поиска в избранное",
        "<номер>"
    },
    {
        "favs",
        &cmd_favs,
        KOTO_LEVEL_ALL,
        "Показать список избранных треков",
        NULL
    },
    {
        "musicinfo",
        &cmd_musicinfo,
        KOTO_LEVEL_ALL,
        "Информация о модуле и сервисе скачивания музыки",
        NULL
    }
};

// ─── Настройки .cfg ────────────────────────────────────────────────────────
static const koto_setting_field SETTINGS[] = {
    {
        "per_page",
        "Треков на страницу поиска",
        KOTO_FIELD_INT,
        "5",
        NULL
    },
    {
        "sc_cid",
        "Пользовательский SoundCloud Client ID (опц.)",
        KOTO_FIELD_STR,
        "",
        NULL
    }
};

// ─── Экспорт модуля ────────────────────────────────────────────────────────
static const koto_module MODULE = {
    KOTO_MODULE_ABI,
    "music_dl",
    "Музыкальный поисковик и загрузчик из SoundCloud (@Diezdd)",
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
