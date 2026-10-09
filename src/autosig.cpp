// autosig.cpp
// Модуль автоматической подписи сообщений для Kotogram (C-ABI 3).
// Перенесено из auto_advanced.plugin.
// Автор: @Diezdd

#include <string>
#include <vector>
#include <cstring>
#include "module_abi.h"
#include "koto_utils.hpp"

using namespace koto_utils;

// Настройки по умолчанию
static const char* DEFAULT_SIG = "✍️ [Kotogram](tg://emoji?id=5370697920704159828)";

// Вспомогательная функция определения типа чата
static bool is_chat_allowed(const koto_api* api, koto_ctx* ctx, long long chat_id) {
    bool for_pm = db_get_int64(api, ctx, "sig_for_pm", 1) != 0;
    bool for_groups = db_get_int64(api, ctx, "sig_for_groups", 1) != 0;
    bool for_channels = db_get_int64(api, ctx, "sig_for_channels", 1) != 0;

    // В Telegram:
    // chat_id > 0: Личные сообщения (PM)
    // chat_id < 0 и chat_id > -1000000000000: Обычные группы
    // chat_id <= -1000000000000: Супергруппы / Каналы (-100...)
    if (chat_id > 0) {
        return for_pm;
    } else if (chat_id > -1000000000000LL) {
        return for_groups;
    } else {
        // Для каналов и супергрупп
        return for_groups || for_channels;
    }
}

// ─── Команда .autosig / .sig ────────────────────────────────────────────────
static void cmd_autosig(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    std::string arg = args ? trim(args) : "";

    long long current = db_get_int64(api, ctx, "sig_enabled", 0);

    if (arg == "on" || arg == "1" || arg == "enable") {
        db_set_int64(api, ctx, "sig_enabled", 1);
        current = 1;
    } else if (arg == "off" || arg == "0" || arg == "disable") {
        db_set_int64(api, ctx, "sig_enabled", 0);
        current = 0;
    } else if (arg.empty()) {
        // Переключатель (toggle)
        current = (current == 0) ? 1 : 0;
        db_set_int64(api, ctx, "sig_enabled", current);
    }

    std::string sig = db_get_str(api, ctx, "sig_text", DEFAULT_SIG);
    std::string pos = db_get_str(api, ctx, "sig_pos", "end");

    api->c_reset(ctx);
    if (current) {
        api->c_fmt(ctx, "✅ Автоподпись включена!\n", KOTO_ENT_BOLD);
    } else {
        api->c_fmt(ctx, "❌ Автоподпись выключена.\n", KOTO_ENT_BOLD);
    }

    api->c_fmt(ctx, "\n• Текст: ", KOTO_ENT_BOLD);
    api->c_markdown(ctx, sig.c_str());
    api->c_fmt(ctx, "\n• Положение: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, pos.c_str(), KOTO_ENT_CODE);
    api->c_fmt(ctx, "\n\n💡 Управление: ", KOTO_ENT_ITALIC);
    api->c_fmt(ctx, ".setsig <текст>", KOTO_ENT_CODE);
    api->c_text(ctx, " | ");
    api->c_fmt(ctx, ".sigpos <start|end|both>", KOTO_ENT_CODE);

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .setsig ────────────────────────────────────────────────────────
static void cmd_setsig(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".setsig <текст подписи>", KOTO_ENT_CODE);
        api->c_text(ctx, "\nПоддерживается Markdown и кастомные эмодзи.");
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    std::string new_sig = trim(args);
    db_set_str(api, ctx, "sig_text", new_sig);

    api->c_reset(ctx);
    api->c_fmt(ctx, "✨ Текст подписи успешно сохранён:\n", KOTO_ENT_BOLD);
    api->c_markdown(ctx, new_sig.c_str());
    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .getsig ────────────────────────────────────────────────────────
static void cmd_getsig(koto_ctx* ctx, const koto_api* api) {
    long long enabled = db_get_int64(api, ctx, "sig_enabled", 0);
    std::string sig = db_get_str(api, ctx, "sig_text", DEFAULT_SIG);
    std::string pos = db_get_str(api, ctx, "sig_pos", "end");
    bool for_pm = db_get_int64(api, ctx, "sig_for_pm", 1) != 0;
    bool for_groups = db_get_int64(api, ctx, "sig_for_groups", 1) != 0;
    bool for_channels = db_get_int64(api, ctx, "sig_for_channels", 1) != 0;

    api->c_reset(ctx);
    api->c_fmt(ctx, "📋 Настройки автоподписи (@Diezdd):\n\n", KOTO_ENT_BOLD);
    api->c_text(ctx, "• Статус: ");
    api->c_fmt(ctx, enabled ? "Активна 🟢" : "Отключена 🔴", KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Позиция: ");
    api->c_fmt(ctx, pos.c_str(), KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Личные чаты (ЛС): ");
    api->c_fmt(ctx, for_pm ? "Да" : "Нет", KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Группы: ");
    api->c_fmt(ctx, for_groups ? "Да" : "Нет", KOTO_ENT_CODE);
    api->c_text(ctx, "\n• Каналы: ");
    api->c_fmt(ctx, for_channels ? "Да" : "Нет", KOTO_ENT_CODE);
    api->c_fmt(ctx, "\n\n• Текст подписи:\n", KOTO_ENT_BOLD);
    api->c_markdown(ctx, sig.c_str());

    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .sigpos ────────────────────────────────────────────────────────
static void cmd_sigpos(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    std::string pos = args ? trim(args) : "";

    if (pos != "start" && pos != "end" && pos != "both") {
        api->c_reset(ctx);
        api->c_fmt(ctx, "⚠️ Использование: ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".sigpos <start|end|both>", KOTO_ENT_CODE);
        api->c_text(ctx, "\n• start — в начале сообщения\n• end — в конце сообщения\n• both — в начале и в конце");
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    db_set_str(api, ctx, "sig_pos", pos);

    api->c_reset(ctx);
    api->c_fmt(ctx, "✅ Положение подписи изменено на: ", KOTO_ENT_BOLD);
    api->c_fmt(ctx, pos.c_str(), KOTO_ENT_CODE);
    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Команда .sigchat (персональные подписи для конкретных чатов) ───────────
static void cmd_sigchat(koto_ctx* ctx, const koto_api* api) {
    const char* args = api->cmd_args(ctx);
    if (!args || !*args) {
        api->c_reset(ctx);
        api->c_fmt(ctx, "💬 Персональные подписи для чатов:\n", KOTO_ENT_BOLD);
        api->c_fmt(ctx, ".sigchat set <chat_id> <текст>", KOTO_ENT_CODE);
        api->c_text(ctx, " — задать подпись для чата\n");
        api->c_fmt(ctx, ".sigchat del <chat_id>", KOTO_ENT_CODE);
        api->c_text(ctx, " — удалить подпись для чата\n");
        api->c_fmt(ctx, ".sigchat here <текст>", KOTO_ENT_CODE);
        api->c_text(ctx, " — задать подпись для текущего чата");
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    std::string s = trim(args);
    std::vector<std::string> parts = split(s, ' ');

    if (parts.size() >= 2 && parts[0] == "here") {
        long long current_chat = api->chat_id(ctx);
        std::string text = trim(s.substr(parts[0].length()));
        std::string key = "sig_c_" + std::to_string(current_chat);
        db_set_str(api, ctx, key, text);

        api->c_reset(ctx);
        api->c_fmt(ctx, "✅ Установлена индивидуальная подпись для текущего чата:\n", KOTO_ENT_BOLD);
        api->c_markdown(ctx, text.c_str());
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    if (parts.size() >= 3 && parts[0] == "set") {
        std::string cid = parts[1];
        size_t off = s.find(parts[1]) + parts[1].length();
        std::string text = trim(s.substr(off));
        std::string key = "sig_c_" + cid;
        db_set_str(api, ctx, key, text);

        api->c_reset(ctx);
        api->c_fmt(ctx, "✅ Установлена подпись для чата ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, cid.c_str(), KOTO_ENT_CODE);
        api->c_text(ctx, ":\n");
        api->c_markdown(ctx, text.c_str());
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    if (parts.size() >= 2 && parts[0] == "del") {
        std::string key = "sig_c_" + parts[1];
        api->db_del(ctx, key.c_str());

        api->c_reset(ctx);
        api->c_fmt(ctx, "🗑 Удалена индивидуальная подпись для чата ", KOTO_ENT_BOLD);
        api->c_fmt(ctx, parts[1].c_str(), KOTO_ENT_CODE);
        api->c_edit(ctx, KOTO_FMT_PLAIN);
        return;
    }

    api->c_reset(ctx);
    api->c_fmt(ctx, "⚠️ Неверный формат команды. См. .sigchat", KOTO_ENT_BOLD);
    api->c_edit(ctx, KOTO_FMT_PLAIN);
}

// ─── Watcher Hook (перехват исходящих сообщений) ───────────────────────────
static int autosig_watch(koto_ctx* ctx, const koto_api* api) {
    // Работаем только если модуль включен
    long long enabled = db_get_int64(api, ctx, "sig_enabled", 0);
    if (!enabled) return 0;

    const char* text = api->msg_text(ctx);
    if (!text || !*text) return 0;

    // Не подписываем служебные команды (начинаются с точки)
    if (text[0] == '.') return 0;

    long long chat_id = api->chat_id(ctx);

    // Проверяем фильтры по типу чата
    if (!is_chat_allowed(api, ctx, chat_id)) return 0;

    // Проверяем персональную подпись чата
    std::string chat_key = "sig_c_" + std::to_string(chat_id);
    std::string sig = db_get_str(api, ctx, chat_key, "");
    if (sig.empty()) {
        // Если нет индивидуальной — берем глобальную
        sig = db_get_str(api, ctx, "sig_text", DEFAULT_SIG);
    }

    if (sig.empty()) return 0;

    std::string pos = db_get_str(api, ctx, "sig_pos", "end");
    std::string original = text;

    api->c_reset(ctx);
    if (pos == "start") {
        api->c_markdown(ctx, sig.c_str());
        api->c_text(ctx, "\n\n");
        api->c_text(ctx, original.c_str());
    } else if (pos == "both") {
        api->c_markdown(ctx, sig.c_str());
        api->c_text(ctx, "\n\n");
        api->c_text(ctx, original.c_str());
        api->c_text(ctx, "\n\n");
        api->c_markdown(ctx, sig.c_str());
    } else { // "end" (по умолчанию)
        api->c_text(ctx, original.c_str());
        api->c_text(ctx, "\n\n");
        api->c_markdown(ctx, sig.c_str());
    }

    // Редактируем исходящее сообщение с добавленной подписью
    api->c_edit(ctx, KOTO_FMT_PLAIN);
    return 0; // продолжаем обычный роутинг
}

// ─── Таблица команд ────────────────────────────────────────────────────────
static const koto_command COMMANDS[] = {
    {
        "autosig",
        &cmd_autosig,
        KOTO_LEVEL_ALL,
        "Включить/выключить автоподпись или показать статус",
        "[on|off]"
    },
    {
        "sig",
        &cmd_autosig,
        KOTO_LEVEL_ALL,
        "Короткий алиас для команды .autosig",
        "[on|off]"
    },
    {
        "setsig",
        &cmd_setsig,
        KOTO_LEVEL_ALL,
        "Установить глобальный текст подписи сообщений",
        "<текст>"
    },
    {
        "getsig",
        &cmd_getsig,
        KOTO_LEVEL_ALL,
        "Показать текущие настройки и текст автоподписи",
        NULL
    },
    {
        "sigpos",
        &cmd_sigpos,
        KOTO_LEVEL_ALL,
        "Изменить положение подписи (start, end, both)",
        "<start|end|both>"
    },
    {
        "sigchat",
        &cmd_sigchat,
        KOTO_LEVEL_ALL,
        "Настроить индивидуальные подписи для конкретных чатов",
        "[set|del|here] [id] [текст]"
    }
};

// ─── Интерактивные настройки для меню .cfg ─────────────────────────────────
static const koto_setting_field SETTINGS[] = {
    {
        "sig_enabled",
        "Включить автоподпись",
        KOTO_FIELD_BOOL,
        "0",
        NULL
    },
    {
        "sig_text",
        "Текст подписи",
        KOTO_FIELD_STR,
        DEFAULT_SIG,
        NULL
    },
    {
        "sig_pos",
        "Положение подписи",
        KOTO_FIELD_CHOICE,
        "end",
        "end,start,both"
    },
    {
        "sig_for_pm",
        "Применять в личных чатах (ЛС)",
        KOTO_FIELD_BOOL,
        "1",
        NULL
    },
    {
        "sig_for_groups",
        "Применять в группах",
        KOTO_FIELD_BOOL,
        "1",
        NULL
    },
    {
        "sig_for_channels",
        "Применять в каналах",
        KOTO_FIELD_BOOL,
        "1",
        NULL
    }
};

// ─── Экспорт модуля ────────────────────────────────────────────────────────
static const koto_module MODULE = {
    KOTO_MODULE_ABI,      // ABI: 3
    "autosig",            // Системное имя
    "Автоматическая подпись исходящих сообщений с форматированием (@Diezdd)",
    "1.0.0",              // Версия модуля
    2,                    // min_host_version (Kotogram 0.1.1+)
    0,                    // flags
    COMMANDS,
    sizeof(COMMANDS) / sizeof(COMMANDS[0]),
    &autosig_watch,
    KOTO_WATCH_OUTGOING,  // Перехватываем исходящие
    NULL,                 // on_callback
    SETTINGS,
    sizeof(SETTINGS) / sizeof(SETTINGS[0]),
    "https://github.com/Skwiziii/kt-Modules"
};

extern "C" const koto_module* koto_module_register(void) {
    return &MODULE;
}

