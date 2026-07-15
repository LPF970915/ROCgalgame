CXX ?= g++
PKG_CONFIG ?= pkg-config
TARGET ?= build/rocgalgame_sdl

SRCS := \
  src/main.cpp \
  src/app.cpp \
  src/input.cpp \
  src/game_library.cpp \
  src/ui_assets.cpp \
  src/core_launcher.cpp \
  src/config.cpp \
  src/app_language.cpp \
  src/system_controls.cpp \
  src/system_status.cpp \
  src/platform.cpp

OBJDIR ?= $(dir $(TARGET))obj
OBJS := $(SRCS:src/%.cpp=$(OBJDIR)/%.o)
DEPS := $(OBJS:.o=.d)

SDL_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
IMG_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags SDL2_image 2>/dev/null)
IMG_LIBS ?= $(shell $(PKG_CONFIG) --libs SDL2_image 2>/dev/null)
TTF_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags SDL2_ttf 2>/dev/null)
TTF_LIBS ?= $(shell $(PKG_CONFIG) --libs SDL2_ttf 2>/dev/null)
ALSA_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags alsa 2>/dev/null)
ALSA_LIBS ?= $(shell $(PKG_CONFIG) --libs alsa 2>/dev/null)

ifeq ($(strip $(SDL_LIBS)),)
SDL_LIBS := -lSDL2
endif

CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter
CXXFLAGS += $(SDL_CFLAGS) -I./src -pthread -MMD -MP
LDFLAGS += -pthread $(SDL_LIBS)

ifneq ($(strip $(IMG_LIBS)),)
CXXFLAGS += -DHAVE_SDL2_IMAGE $(IMG_CFLAGS)
LDFLAGS += $(IMG_LIBS)
endif

ifneq ($(strip $(TTF_LIBS)),)
CXXFLAGS += -DHAVE_SDL2_TTF $(TTF_CFLAGS)
LDFLAGS += $(TTF_LIBS)
endif

ifneq ($(strip $(ALSA_LIBS)),)
CXXFLAGS += -DHAVE_ALSA $(ALSA_CFLAGS)
LDFLAGS += $(ALSA_LIBS)
endif

CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

.PHONY: all clean install-runtime print-config

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

install-runtime: $(TARGET)
	rm -rf runtime
	@mkdir -p runtime/cores/ons runtime/games runtime/covers runtime/saves runtime/cache
	cp $(TARGET) runtime/rocgalgame_sdl
	cp ROCgalgame.sh runtime/ROCgalgame.sh
	cp native_config.ini runtime/native_config.ini
	cp native_keymap.ini runtime/native_keymap.ini
	cp -r ui fonts sounds runtime/

print-config:
	@echo CXX=$(CXX)
	@echo TARGET=$(TARGET)
	@echo OBJDIR=$(OBJDIR)
	@echo CXXFLAGS=$(CXXFLAGS)
	@echo LDFLAGS=$(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(TARGET)
