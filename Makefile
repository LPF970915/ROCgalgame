CXX ?= g++
PKG_CONFIG ?= pkg-config
TARGET ?= build/rocgalgame_sdl
TEST_EXEEXT :=
ifeq ($(OS),Windows_NT)
TEST_EXEEXT := .exe
endif
TEST_TARGET ?= build/core_launch_test$(TEST_EXEEXT)
FOUNDATION_TEST_TARGET ?= build/app_foundation_test$(TEST_EXEEXT)
CONFIG_STORE_TEST_TARGET ?= build/config_store_test$(TEST_EXEEXT)
INPUT_MANAGER_TEST_TARGET ?= build/input_manager_test$(TEST_EXEEXT)
SYSTEM_RUNTIME_TEST_TARGET ?= build/system_runtime_test$(TEST_EXEEXT)
MENU_RUNTIME_TEST_TARGET ?= build/menu_runtime_test$(TEST_EXEEXT)
GAME_DOMAIN_TEST_TARGET ?= build/game_domain_runtime_test$(TEST_EXEEXT)

APP_SRCS := \
  src/main.cpp \
  src/app_loop.cpp \
  src/app_bootstrap.cpp \
  src/app_composition.cpp \
  src/app_context.cpp \
  src/app_environment.cpp \
  src/app_services.cpp \
  src/app_shell.cpp \
  src/app_layout.cpp \
  src/app_runtime.cpp \
  src/app_stores.cpp \
  src/animation.cpp \
  src/scene_manager.cpp \
  src/app_language.cpp \
  src/config.cpp

PLATFORM_SRCS := \
  src/screen_profile.cpp \
  src/input_manager.cpp \
  src/lid_power_control.cpp \
  src/power_lifecycle.cpp \
  src/system_controls.cpp \
  src/system_settings_runtime.cpp \
  src/system_status.cpp

SHELF_SRCS := \
  src/game_library.cpp \
  src/game_scanner.cpp \
  src/game_library_service.cpp \
  src/shelf_runtime.cpp \
  src/shelf_scene.cpp \
  src/cover_resolver.cpp \
  src/cover_cache_runtime.cpp \
  src/cover_service.cpp

MENU_SRCS := \
  src/menu_runtime.cpp \
  src/menu_scene.cpp \
  src/settings_runtime.cpp \
  src/settings_panel_router.cpp \
  src/settings_composition.cpp \
  src/system_controls_panel.cpp \
  src/game_settings_runtime.cpp \
  src/game_settings_panel.cpp \
  src/key_guide_panel.cpp \
  src/key_calibration_panel.cpp \
  src/key_calibration_runtime.cpp \
  src/contributor_avatar_runtime.cpp \
  src/contributor_avatar_panel.cpp \
  src/contact_panel.cpp \
  src/version_update_runtime.cpp \
  src/update_panel.cpp \
  src/exit_panel.cpp

UI_SRCS := \
  src/ui_assets.cpp \
  src/ui_assets_loader.cpp \
  src/ui_text_cache.cpp \
  src/texture_registry.cpp \
  src/volume_overlay.cpp

STATUS_SRCS := \
  src/audio_runtime.cpp \
  src/status_bar_runtime.cpp

GAME_DOMAIN_SRCS := \
  src/game_core_adapter.cpp \
  src/game_core_registry.cpp \
  src/ons_core_adapter.cpp \
  src/krkr_core_adapter.cpp \
  src/core_process_runner.cpp \
  src/game_launch_service.cpp

SRCS := \
  $(APP_SRCS) \
  $(PLATFORM_SRCS) \
  $(SHELF_SRCS) \
  $(MENU_SRCS) \
  $(UI_SRCS) \
  $(STATUS_SRCS) \
  $(GAME_DOMAIN_SRCS)

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
MIXER_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags SDL2_mixer 2>/dev/null)
MIXER_LIBS ?= $(shell $(PKG_CONFIG) --libs SDL2_mixer 2>/dev/null)

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

ifneq ($(strip $(MIXER_LIBS)),)
CXXFLAGS += -DHAVE_SDL2_MIXER $(MIXER_CFLAGS)
LDFLAGS += $(MIXER_LIBS)
endif

CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

.PHONY: all clean install-runtime print-config test

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
	python3 scripts/pack_ui_assets.py ui runtime/ui.pack
	cp -r fonts sounds runtime/

print-config:
	@echo CXX=$(CXX)
	@echo TARGET=$(TARGET)
	@echo OBJDIR=$(OBJDIR)
	@echo CXXFLAGS=$(CXXFLAGS)
	@echo LDFLAGS=$(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(TARGET)

test:
	@mkdir -p build
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src tests/core_launch_test.cpp src/game_library.cpp src/game_scanner.cpp src/cover_resolver.cpp src/config.cpp src/game_core_adapter.cpp src/game_core_registry.cpp src/ons_core_adapter.cpp src/krkr_core_adapter.cpp src/core_process_runner.cpp src/game_launch_service.cpp -o $(TEST_TARGET)
	./$(TEST_TARGET)
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src $(SDL_CFLAGS) -DSDL_MAIN_HANDLED -Umain tests/app_foundation_test.cpp src/app_layout.cpp src/screen_profile.cpp src/input_manager.cpp src/app_environment.cpp src/scene_manager.cpp src/app_composition.cpp src/app_context.cpp -o $(FOUNDATION_TEST_TARGET) $(SDL_LIBS)
	./$(FOUNDATION_TEST_TARGET)
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src tests/config_store_test.cpp src/config.cpp src/app_stores.cpp -o $(CONFIG_STORE_TEST_TARGET)
	./$(CONFIG_STORE_TEST_TARGET)
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src $(SDL_CFLAGS) -DSDL_MAIN_HANDLED -Umain tests/input_manager_test.cpp src/input_manager.cpp -o $(INPUT_MANAGER_TEST_TARGET) $(SDL_LIBS)
	./$(INPUT_MANAGER_TEST_TARGET)
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src $(SDL_CFLAGS) -DSDL_MAIN_HANDLED -Umain tests/system_runtime_test.cpp src/app_runtime.cpp src/app_stores.cpp src/config.cpp src/system_controls.cpp src/system_settings_runtime.cpp src/power_lifecycle.cpp src/lid_power_control.cpp -o $(SYSTEM_RUNTIME_TEST_TARGET) $(SDL_LIBS)
	./$(SYSTEM_RUNTIME_TEST_TARGET)
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src $(SDL_CFLAGS) -DSDL_MAIN_HANDLED -Umain tests/menu_runtime_test.cpp src/animation.cpp src/input_manager.cpp src/menu_scene.cpp src/settings_runtime.cpp src/settings_panel_router.cpp src/system_controls_panel.cpp src/game_settings_runtime.cpp src/game_settings_panel.cpp src/app_stores.cpp src/config.cpp src/app_language.cpp -o $(MENU_RUNTIME_TEST_TARGET) $(SDL_LIBS)
	./$(MENU_RUNTIME_TEST_TARGET)
	$(CXX) -O2 -std=c++17 -Wall -Wextra -I./src $(SDL_CFLAGS) -DSDL_MAIN_HANDLED -Umain tests/game_domain_runtime_test.cpp src/config.cpp src/app_stores.cpp src/game_settings_runtime.cpp src/input_manager.cpp src/game_library.cpp src/game_scanner.cpp src/cover_resolver.cpp src/game_library_service.cpp src/shelf_runtime.cpp src/game_core_adapter.cpp src/game_core_registry.cpp src/game_launch_service.cpp -o $(GAME_DOMAIN_TEST_TARGET) $(SDL_LIBS)
	./$(GAME_DOMAIN_TEST_TARGET)
