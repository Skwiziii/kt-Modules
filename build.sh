#!/usr/bin/env bash
# ==============================================================================
# Kotogram / KoteLoader Module SDK Build Script
# Скрипт сборки модулей для юзербота Kotogram (Android arm64-v8a / Linux x86_64)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INCLUDE_DIR="${SCRIPT_DIR}/include"
SRC_DIR="${SCRIPT_DIR}/src"
OUT_DIR="${SCRIPT_DIR}/out"

mkdir -p "${OUT_DIR}"

# Определение NDK и компилятора
TARGET="android" # по умолчанию arm64-v8a для Android
NDK_CLANG=""

# Возможные пути к Android NDK
NDK_CANDIDATES=(
    "/opt/android-sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
    "/opt/android-sdk/ndk/25.2.9519653/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
    "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
    "${ANDROID_HOME}/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
    "/home/kot/android-toolchain/sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
    "$(which aarch64-linux-android26-clang 2>/dev/null || true)"
)

for cand in "${NDK_CANDIDATES[@]}"; do
    if [ -n "$cand" ] && [ -x "$cand" ]; then
        NDK_CLANG="$cand"
        break
    fi
done

# Проверка флагов командной строки
ARG_FILE=""
for arg in "$@"; do
    case "$arg" in
        --host|--linux)
            TARGET="host"
            ;;
        --android)
            TARGET="android"
            ;;
        --help|-h)
            echo "Использование: ./build.sh [файл.c / файл.lua] [--android | --host]"
            echo "Примеры:"
            echo "  ./build.sh                         # Собрать все файлы в src/"
            echo "  ./build.sh src/my_module.c        # Собрать конкретный C-модуль"
            echo "  ./build.sh examples/lua/copym.lua # Скопировать и проверить Lua-модуль"
            echo "  ./build.sh --host                 # Собрать под локальный Linux x86_64"
            exit 0
            ;;
        *)
            if [ -f "$arg" ]; then
                ARG_FILE="$arg"
            fi
            ;;
    esac
done

if [ "$TARGET" = "android" ]; then
    if [ -z "$NDK_CLANG" ]; then
        echo "⚠️ Android NDK не обнаружен автоматически. Переключаюсь на сборку для локального хоста (host/linux)."
        TARGET="host"
    fi
fi

if [ "$TARGET" = "android" ]; then
    CC="${NDK_CLANG}"
    CXX="${NDK_CLANG}++"
    TARGET_NAME="Android arm64-v8a"
    CFLAGS="-O2 -fPIC -shared -Wall -Wextra -std=c11 -I${INCLUDE_DIR}"
    CXXFLAGS="-O2 -fPIC -shared -Wall -Wextra -std=c++20 -I${INCLUDE_DIR}"
else
    CC="gcc"
    CXX="g++"
    TARGET_NAME="Linux Host ($(uname -m))"
    CFLAGS="-O2 -fPIC -shared -Wall -Wextra -std=c11 -I${INCLUDE_DIR}"
    CXXFLAGS="-O2 -fPIC -shared -Wall -Wextra -std=c++20 -I${INCLUDE_DIR}"
fi

echo "=========================================================="
echo "🛠  Kotogram Module SDK Builder"
echo "🎯 Целевая платформа: ${TARGET_NAME}"
echo "📁 Папка вывода:      ${OUT_DIR}"
echo "=========================================================="

build_c() {
    local src="$1"
    local base="$(basename "$src")"
    local name="${base%.*}"
    local out="${OUT_DIR}/${name}.so"

    echo "⚙️ Компиляция C-модуля: ${src} -> ${out}..."
    ${CC} ${CFLAGS} "${src}" -o "${out}"
    echo "✅ Успешно собран: ${out} ($(wc -c < "${out}") байт)"
}

build_cpp() {
    local src="$1"
    local base="$(basename "$src")"
    local name="${base%.*}"
    local out="${OUT_DIR}/${name}.so"

    echo "⚙️ Компиляция C++ модуля: ${src} -> ${out}..."
    ${CXX} ${CXXFLAGS} "${src}" -o "${out}"
    echo "✅ Успешно собран: ${out} ($(wc -c < "${out}") байт)"
}

build_lua() {
    local src="$1"
    local base="$(basename "$src")"
    local out="${OUT_DIR}/${base}"

    echo "📜 Обработка Lua-модуля: ${src}..."
    if command -v luac &>/dev/null; then
        luac -p "${src}"
        echo "   (синтаксис проверен luac - ошибок нет)"
    fi
    cp -f "${src}" "${out}"
    echo "✅ Готов к установке: ${out}"
}

build_file() {
    local f="$1"
    case "$f" in
        *.c)
            build_c "$f"
            ;;
        *.cpp|*.cc)
            build_cpp "$f"
            ;;
        *.lua)
            build_lua "$f"
            ;;
        *)
            echo "❓ Неизвестное расширение файла: $f"
            ;;
    esac
}

if [ -n "$ARG_FILE" ]; then
    build_file "$ARG_FILE"
else
    # Собираем всё из src/
    FOUND=0
    for f in "${SRC_DIR}"/*; do
        if [ -f "$f" ]; then
            build_file "$f"
            FOUND=1
        fi
    done
    if [ $FOUND -eq 0 ]; then
        echo "ℹ️ В папке src/ пока нет файлов для сборки."
        echo "💡 Соберите пример командой: ./build.sh examples/native/hello.c"
    fi
fi

echo ""
echo "🎉 Готово! Все собранные файлы находятся в: ${OUT_DIR}/"
echo "📲 Чтобы установить модуль в юзербот:"
echo "   1. Отправьте файл .so или .lua себе в Telegram (или в любой чат)."
echo "   2. Ответьте на сообщение с файлом командой: .install"
echo "   3. Модуль мгновенно загрузится и станет доступен в .help и .modules!"
