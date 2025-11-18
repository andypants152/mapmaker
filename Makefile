# Simple cross-platform build: linux/desktop (SDL2 + OpenGL) and Nintendo Switch (devkitA64)

TARGET := mapmaker
SRC_DIR := source
ROMFS_DIR := romfs
DESKTOP_BIN := $(TARGET)
WINDOWS_BIN := $(TARGET).exe
SWITCH_ELF := $(TARGET).elf
SWITCH_NRO := $(TARGET).nro
WEB_DIR := web
WEB_PUBLIC := $(WEB_DIR)/public
WEB_TARGET := $(WEB_PUBLIC)/mapmaker
WEB_SHELL := $(WEB_DIR)/shell.html

COMMON_SRC := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/RendererGL.cpp \
	$(SRC_DIR)/stb_image_impl.cpp \
	$(SRC_DIR)/Platform.cpp
COMMON_CXXFLAGS := -std=c++17 -O2 -I$(SRC_DIR)

# Emscripten WebAssembly build (SDL2 + WebGL2)
EMXX ?= em++
EMXXFLAGS := $(COMMON_CXXFLAGS) \
	-sUSE_SDL=2 \
	-sUSE_WEBGL2=1 \
	-sMIN_WEBGL_VERSION=1 \
	-sMAX_WEBGL_VERSION=2 \
	-sALLOW_MEMORY_GROWTH=1 \
	-sASSERTIONS=1 \
	-sENVIRONMENT=web

# MinGW-w64 cross compile settings (override paths if you unpacked SDL2 elsewhere)
WINDOWS_TRIPLE ?= x86_64-w64-mingw32
WINDOWS_PREFIX ?= /usr/$(WINDOWS_TRIPLE)
WINDOWS_SDL2_DIR ?= $(WINDOWS_PREFIX)
WINDOWS_CXX := $(WINDOWS_TRIPLE)-g++
WINDOWS_CXXFLAGS := $(COMMON_CXXFLAGS) \
	-I$(WINDOWS_SDL2_DIR)/include/SDL2
WINDOWS_LDFLAGS := \
	-L$(WINDOWS_SDL2_DIR)/lib \
	-lmingw32 -lSDL2main -lSDL2 -lopengl32

# Desktop linking (SDL2 + OpenGL)
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
	DESKTOP_LIBS := -lmingw32 -lSDL2main -lSDL2 -lopengl32
else ifeq ($(UNAME_S),Darwin)
	DESKTOP_LIBS := -F/Library/Frameworks -framework SDL2 -framework OpenGL
else
	DESKTOP_LIBS := -lSDL2 -lGL -ldl
endif

DESKTOP_DATA_DIR := data
DESKTOP_DATA_SRC := $(ROMFS_DIR)/data

.PHONY: all clean linux switch desktop_data check_devkit windows wasm

all: switch

linux: desktop_data $(DESKTOP_BIN)

desktop_data:
	@mkdir -p $(DESKTOP_DATA_DIR)
	@cp -r $(DESKTOP_DATA_SRC)/. $(DESKTOP_DATA_DIR)/

$(DESKTOP_BIN): $(COMMON_SRC)
	g++ $(COMMON_CXXFLAGS) $(COMMON_SRC) $(DESKTOP_LIBS) -o $@

windows: desktop_data $(WINDOWS_BIN)

$(WINDOWS_BIN): $(COMMON_SRC)
	$(WINDOWS_CXX) $(WINDOWS_CXXFLAGS) $(COMMON_SRC) $(WINDOWS_LDFLAGS) -o $@

switch: check_devkit $(SWITCH_NRO)

wasm: $(WEB_TARGET).html

$(WEB_TARGET).html: $(COMMON_SRC) $(WEB_SHELL)
	@mkdir -p $(WEB_PUBLIC)
	$(EMXX) $(EMXXFLAGS) $(COMMON_SRC) \
		--preload-file $(ROMFS_DIR)/data@/data \
		--shell-file $(WEB_SHELL) \
		-o $@

check_devkit:
	@test -n "$(DEVKITPRO)" || (echo "Please set DEVKITPRO=<path to>/devkitpro" && false)

$(SWITCH_ELF): $(COMMON_SRC)
	$(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++ \
		-std=c++17 -O2 -ffunction-sections -fno-rtti -fno-exceptions -fPIE \
		-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft \
		-D__SWITCH__ \
		-I$(SRC_DIR) -I$(DEVKITPRO)/libnx/include -I$(DEVKITPRO)/portlibs/switch/include \
		-specs=$(DEVKITPRO)/libnx/switch.specs \
		$(COMMON_SRC) \
		-L$(DEVKITPRO)/portlibs/switch/lib -L$(DEVKITPRO)/libnx/lib \
		-lSDL2 -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm \
		-o $@

$(SWITCH_NRO): $(SWITCH_ELF)
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --romfsdir=$(ROMFS_DIR) --nacp=$(TARGET).nacp

clean:
	rm -f $(DESKTOP_BIN) $(WINDOWS_BIN) $(SWITCH_ELF) $(SWITCH_NRO)
	rm -f $(WEB_TARGET).html $(WEB_TARGET).js $(WEB_TARGET).wasm $(WEB_TARGET).data
