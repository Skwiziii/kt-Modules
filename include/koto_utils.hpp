#ifndef KOTO_UTILS_HPP
#define KOTO_UTILS_HPP

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <unistd.h>
#include "module_abi.h"
#include "nlohmann/json.hpp"

namespace koto_utils {

// Обрезка пробельных символов
inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Замена всех вхождений подстроки
inline std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// URL-кодирование для поисковых запросов и параметров
inline std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << int(c);
        }
    }
    return escaped.str();
}

// Экранирование для shell аргументов (защита от инъекций при вызове curl)
inline std::string shell_escape(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

// HTTP GET запрос с получением строкового ответа (через curl)
inline std::string http_get(const std::string& url, const std::vector<std::string>& headers = {}, int timeout_sec = 10) {
    std::string cmd = "curl -sL --max-time " + std::to_string(timeout_sec);
    for (const auto& h : headers) {
        cmd += " -H " + shell_escape(h);
    }
    cmd += " " + shell_escape(url);

    struct PipeCloser {
        void operator()(FILE* fp) const {
            if (fp) pclose(fp);
        }
    };
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Скачивание файла по URL во временный файл
inline bool download_file(const std::string& url, const std::string& out_path, const std::vector<std::string>& headers = {}, int timeout_sec = 60) {
    std::string cmd = "curl -sL --max-time " + std::to_string(timeout_sec);
    for (const auto& h : headers) {
        cmd += " -H " + shell_escape(h);
    }
    cmd += " -o " + shell_escape(out_path) + " " + shell_escape(url);

    int ret = std::system(cmd.c_str());
    if (ret != 0) return false;

    // Проверяем, существует ли файл и не пустой ли он
    FILE* f = fopen(out_path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size > 0;
}

// Форматирование секунд в формат MM:SS или HH:MM:SS
inline std::string format_duration(long long total_seconds) {
    if (total_seconds < 0) total_seconds = 0;
    long long hours = total_seconds / 3600;
    long long minutes = (total_seconds % 3600) / 60;
    long long seconds = total_seconds % 60;

    char buf[32];
    if (hours > 0) {
        snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld", hours, minutes, seconds);
    } else {
        snprintf(buf, sizeof(buf), "%02lld:%02lld", minutes, seconds);
    }
    return std::string(buf);
}

// Генерация визуального прогресс-бара для плеера (например: 🔘────────)
inline std::string progress_bar(long long current, long long total, int bar_width = 10) {
    if (total <= 0) total = 1;
    if (current < 0) current = 0;
    if (current > total) current = total;

    double progress = static_cast<double>(current) / static_cast<double>(total);
    int pos = static_cast<int>(progress * bar_width);
    if (pos >= bar_width) pos = bar_width - 1;

    std::string bar = "";
    for (int i = 0; i < bar_width; ++i) {
        if (i == pos) {
            bar += "🔘";
        } else {
            bar += "─";
        }
    }
    return bar;
}

// Разделение строки по разделителю
inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

// Хелперы для работы с локальной базой данных модуля
inline std::string db_get_str(const koto_api* api, koto_ctx* ctx, const std::string& key, const std::string& def = "") {
    char buf[2048] = {0};
    int len = api->db_get(ctx, key.c_str(), buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return def;
}

inline void db_set_str(const koto_api* api, koto_ctx* ctx, const std::string& key, const std::string& val) {
    api->db_set(ctx, key.c_str(), val.c_str());
}

inline long long db_get_int64(const koto_api* api, koto_ctx* ctx, const std::string& key, long long def = 0) {
    return api->db_get_int(ctx, key.c_str(), def);
}

inline void db_set_int64(const koto_api* api, koto_ctx* ctx, const std::string& key, long long val) {
    api->db_set_int(ctx, key.c_str(), val);
}

// Хелперы для работы с настройками (.cfg)
inline std::string cfg_get_str(const koto_api* api, koto_ctx* ctx, const std::string& key, const std::string& def = "") {
    char buf[512] = {0};
    int len = api->cfg_get(ctx, key.c_str(), buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return def;
}

} // namespace koto_utils

#endif // KOTO_UTILS_HPP
