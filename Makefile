# ==============================================================================
# Makefile для Kotogram Module SDK
# ==============================================================================

CC        ?= gcc
CXX       ?= g++
CFLAGS    ?= -O2 -fPIC -shared -Wall -Wextra -std=c11 -Iinclude
CXXFLAGS  ?= -O2 -fPIC -shared -Wall -Wextra -std=c++20 -Iinclude

NDK_CLANG ?= /home/kot/android-toolchain/sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang

SRCS      := $(wildcard src/*.c) $(wildcard src/*.cpp)
MODULES   := $(patsubst src/%.c,out/%.so,$(filter %.c,$(SRCS))) \
             $(patsubst src/%.cpp,out/%.so,$(filter %.cpp,$(SRCS)))

.PHONY: all android host clean help

# По умолчанию собираем под Android (arm64-v8a) через build.sh
all: android

android:
	@./build.sh --android

host:
	@./build.sh --host

clean:
	rm -rf out/*.so out/*.lua

help:
	@echo "Доступные цели:"
	@echo "  make android   - Собрать модули под Android arm64-v8a (для APK Kotogram)"
	@echo "  make host      - Собрать модули под текущий Linux x86_64"
	@echo "  make clean     - Очистить папку out/"
