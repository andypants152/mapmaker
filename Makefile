# Simple cross-platform build: desktop (SDL2 + OpenGL) and Nintendo Switch (devkitA64)

TARGET := mapmaker
SRC_DIR := source
ROMFS_DIR := romfs
DESKTOP_BIN := $(TARGET)_desktop
SWITCH_ELF := $(TARGET).elf
SWITCH_NRO := $(TARGET).nro

COMMON_SRC := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/RendererGL.cpp \
	$(SRC_DIR)/stb_image_impl.cpp \
	$(SRC_DIR)/Platform.cpp
COMMON_CXXFLAGS := -std=c++17 -O2 -I$(SRC_DIR)

# Desktop linking (SDL2 + OpenGL + PNG)
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
	DESKTOP_LIBS := -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lpng -lz
else ifeq ($(UNAME_S),Darwin)
	DESKTOP_LIBS := -F/Library/Frameworks -framework SDL2 -framework OpenGL -lpng -lz
else
	DESKTOP_LIBS := -lSDL2 -lGL -lpng -lz -ldl
endif

DESKTOP_DATA_DIR := data
DESKTOP_DATA_SRC := $(ROMFS_DIR)/data

.PHONY: all clean desktop switch desktop_data check_devkit

all: desktop

desktop: desktop_data $(DESKTOP_BIN)

desktop_data:
	@mkdir -p $(DESKTOP_DATA_DIR)
	@cp -r $(DESKTOP_DATA_SRC)/. $(DESKTOP_DATA_DIR)/

$(DESKTOP_BIN): $(COMMON_SRC)
	g++ $(COMMON_CXXFLAGS) $(COMMON_SRC) $(DESKTOP_LIBS) -o $@

switch: check_devkit $(SWITCH_NRO)

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
		-lSDL2 -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lpng -lz -lnx -lm \
		-o $@

$(SWITCH_NRO): $(SWITCH_ELF)
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --romfsdir=$(ROMFS_DIR) --nacp=$(TARGET).nacp

clean:
	rm -f $(DESKTOP_BIN) $(SWITCH_ELF) $(SWITCH_NRO)
