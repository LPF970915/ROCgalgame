#include "core_input_bridge.h"
#include "core_input_protocol.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <utility>

#if defined(__linux__)
#include <dlfcn.h>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct CoreInputBridge::Impl {
  Impl(InputManager &input_value, const EffectiveGameSettings &settings_value,
       std::string display_value)
      : input(input_value), settings(settings_value),
        display_name(std::move(display_value)) {}

  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;

  InputManager &input;
  EffectiveGameSettings settings;
  std::string display_name;
  std::chrono::steady_clock::time_point last_poll = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last_stats_report = last_poll;
  std::chrono::steady_clock::time_point next_connect_attempt{};
  float exit_chord_seconds = 0.0f;
  float residual_x = 0.0f;
  float residual_y = 0.0f;
  float last_axis_x = 0.0f;
  float last_axis_y = 0.0f;
  std::uint64_t axis_sequence = 0;
  std::chrono::steady_clock::time_point last_axis_send{};
  bool axis_state_sent = false;
  bool exit_logged = false;
  std::uint64_t pump_polls = 0;
  std::uint64_t axis_state_changes = 0;
  std::uint64_t axis_heartbeats = 0;
  std::uint64_t ipc_writes = 0;
  std::uint64_t ipc_write_failures = 0;
  std::uint64_t ipc_reconnects = 0;

#if defined(__linux__)
  struct _XDisplay;
  using Display = _XDisplay;
  using KeySym = unsigned long;
  using KeyCode = unsigned char;
  using OpenDisplayFn = Display *(*)(const char *);
  using CloseDisplayFn = int (*)(Display *);
  using FlushFn = int (*)(Display *);
  using KeysymToKeycodeFn = KeyCode (*)(Display *, KeySym);
  using FakeRelativeMotionFn = int (*)(Display *, int, int, unsigned long);
  using FakeButtonFn = int (*)(Display *, unsigned int, int, unsigned long);
  using FakeKeyFn = int (*)(Display *, unsigned int, int, unsigned long);

  void *x11_library = nullptr;
  void *xtst_library = nullptr;
  Display *display = nullptr;
  OpenDisplayFn open_display = nullptr;
  CloseDisplayFn close_display = nullptr;
  FlushFn flush = nullptr;
  KeysymToKeycodeFn keysym_to_keycode = nullptr;
  FakeRelativeMotionFn fake_relative_motion = nullptr;
  FakeButtonFn fake_button = nullptr;
  FakeKeyFn fake_key = nullptr;
  std::array<bool, 2> mouse_down{};
  std::array<bool, 6> key_down{};
  bool libraries_checked = false;
  bool unavailable_logged = false;
  std::string fifo_path;
  int fifo_fd = -1;

  template <typename T>
  static T LoadSymbol(void *library, const char *name) {
    return reinterpret_cast<T>(dlsym(library, name));
  }

  bool LoadLibraries() {
    if (libraries_checked) return x11_library && xtst_library;
    libraries_checked = true;
    x11_library = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
    xtst_library = dlopen("libXtst.so.6", RTLD_NOW | RTLD_LOCAL);
    if (!x11_library || !xtst_library) return false;
    open_display = LoadSymbol<OpenDisplayFn>(x11_library, "XOpenDisplay");
    close_display = LoadSymbol<CloseDisplayFn>(x11_library, "XCloseDisplay");
    flush = LoadSymbol<FlushFn>(x11_library, "XFlush");
    keysym_to_keycode =
        LoadSymbol<KeysymToKeycodeFn>(x11_library, "XKeysymToKeycode");
    fake_relative_motion = LoadSymbol<FakeRelativeMotionFn>(
        xtst_library, "XTestFakeRelativeMotionEvent");
    fake_button = LoadSymbol<FakeButtonFn>(xtst_library, "XTestFakeButtonEvent");
    fake_key = LoadSymbol<FakeKeyFn>(xtst_library, "XTestFakeKeyEvent");
    return open_display && close_display && flush && keysym_to_keycode &&
           fake_relative_motion && fake_button && fake_key;
  }

  bool EnsureDisplay(std::chrono::steady_clock::time_point now) {
    if (display) return true;
    if (now < next_connect_attempt) return false;
    next_connect_attempt = now + std::chrono::milliseconds(500);
    if (!LoadLibraries()) {
      if (!unavailable_logged) {
        std::clog << "[core_input] X11/XTest libraries unavailable\n";
        unavailable_logged = true;
      }
      return false;
    }
    display = open_display(display_name.c_str());
    if (display) {
      std::clog << "[core_input] bridge ready display=" << display_name << "\n";
      return true;
    }
    return false;
  }

  bool EnsureFifo() {
    if (fifo_fd >= 0) return true;
    if (fifo_path.empty()) return false;
    fifo_fd = open(fifo_path.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fifo_fd < 0) return false;
    ++ipc_reconnects;
    mouse_down.fill(false);
    key_down.fill(false);
    axis_state_sent = false;
    std::clog << "[core_input] transport=fifo path=" << fifo_path << "\n";
    return true;
  }

  bool WriteFifo(const std::string &line) {
    if (!EnsureFifo()) {
      ++ipc_write_failures;
      return false;
    }
    const ssize_t written = write(fifo_fd, line.data(), line.size());
    if (written == static_cast<ssize_t>(line.size())) {
      ++ipc_writes;
      return true;
    }
    ++ipc_write_failures;
    close(fifo_fd);
    fifo_fd = -1;
    return false;
  }

  void SetMouseButton(size_t index, unsigned int button, bool down) {
    if (mouse_down[index] == down) return;
    mouse_down[index] = down;
    if (fifo_fd >= 0) {
      WriteFifo(std::string("B ") + (button == 1 ? "L " : "R ") +
                (down ? "1\n" : "0\n"));
    } else if (display) {
      fake_button(display, button, down ? 1 : 0, 0);
    }
    std::clog << "[core_input] " << (button == 1 ? "A/left" : "B/right")
              << (down ? " down\n" : " up\n");
  }

  void SetKey(size_t index, KeySym symbol, bool down) {
    if (key_down[index] == down) return;
    key_down[index] = down;
    if (fifo_fd >= 0) {
      unsigned int vk = 0;
      switch (symbol) {
        case 0xff0d: vk = 13; break;
        case 0xff1b: vk = 27; break;
        case 0xff51: vk = 37; break;
        case 0xff52: vk = 38; break;
        case 0xff53: vk = 39; break;
        case 0xff54: vk = 40; break;
        default: break;
      }
      if (vk != 0) {
        WriteFifo("K " + std::to_string(vk) + (down ? " 1\n" : " 0\n"));
      }
    } else if (display) {
      const KeyCode code = keysym_to_keycode(display, symbol);
      if (code != 0) fake_key(display, code, down ? 1 : 0, 0);
    }
  }

  void SendAxisState(float axis_x, float axis_y,
                     std::chrono::steady_clock::time_point now) {
    const auto elapsed = axis_state_sent
        ? std::chrono::duration_cast<std::chrono::milliseconds>(
              now - last_axis_send).count()
        : 0;
    if (!CoreAxisStateNeedsSend(axis_x, axis_y, last_axis_x, last_axis_y,
                                axis_state_sent,
                                static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed))))
      return;
    const bool changed = !axis_state_sent ||
        std::abs(axis_x - last_axis_x) >= kCoreAxisChangeThreshold ||
        std::abs(axis_y - last_axis_y) >= kCoreAxisChangeThreshold;
    const std::uint64_t next_sequence = axis_sequence + 1;
    if (!WriteFifo(EncodeCoreAxisState(axis_x, axis_y, next_sequence))) {
      axis_state_sent = false;
      return;
    }
    axis_sequence = next_sequence;
    last_axis_x = axis_x;
    last_axis_y = axis_y;
    last_axis_send = now;
    axis_state_sent = true;
    if (changed) ++axis_state_changes;
    else ++axis_heartbeats;
  }

  void Inject(float dt, std::chrono::steady_clock::time_point now) {
    constexpr KeySym kReturn = 0xff0d;
    constexpr KeySym kEscape = 0xff1b;
    constexpr KeySym kLeft = 0xff51;
    constexpr KeySym kUp = 0xff52;
    constexpr KeySym kRight = 0xff53;
    constexpr KeySym kDown = 0xff54;

    const float axis_x = input.CursorAxisX();
    const float axis_y = input.CursorAxisY();
    if (settings.virtual_mouse) {
      if (fifo_fd >= 0) {
        SendAxisState(axis_x, axis_y, now);
      } else if (display) {
        const auto velocity = [&](float axis) {
          if (axis == 0.0f) return 0.0f;
          return std::copysign(
              static_cast<float>(settings.mouse_speed) *
                  std::pow(std::abs(axis), settings.mouse_acceleration),
              axis);
        };
        residual_x += velocity(axis_x) * dt;
        residual_y += velocity(axis_y) * dt;
        const int dx = static_cast<int>(std::trunc(residual_x));
        const int dy = static_cast<int>(std::trunc(residual_y));
        residual_x -= static_cast<float>(dx);
        residual_y -= static_cast<float>(dy);
        if (dx != 0 || dy != 0)
          fake_relative_motion(display, dx, dy, 0);
      }
    }

    SetMouseButton(0, 1, input.IsPressed(Button::A));
    SetMouseButton(1, 3, input.IsPressed(Button::B));
    SetKey(0, kReturn, input.IsPressed(Button::X));
    SetKey(1, kEscape,
           input.IsPressed(Button::Y) || input.IsPressed(Button::Menu));

    const bool analog_x_active = axis_x != 0.0f;
    const bool analog_y_active = axis_y != 0.0f;
    SetKey(2, kUp, !analog_y_active && input.IsPressed(Button::Up));
    SetKey(3, kDown, !analog_y_active && input.IsPressed(Button::Down));
    SetKey(4, kLeft, !analog_x_active && input.IsPressed(Button::Left));
    SetKey(5, kRight, !analog_x_active && input.IsPressed(Button::Right));
    if (fifo_fd < 0 && display) flush(display);
  }
#endif

  bool Poll() {
    ++pump_polls;
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(
        std::chrono::duration<float>(now - last_poll).count(), 0.001f, 0.05f);
    last_poll = now;
    input.BeginFrame(dt);
    input.EndFrame();

    if (input.IsPressed(Button::Start) && input.IsPressed(Button::Select)) {
      exit_chord_seconds += dt;
    } else {
      exit_chord_seconds = 0.0f;
      exit_logged = false;
    }
    if (exit_chord_seconds >= 0.45f) {
      if (!exit_logged) {
        std::clog << "[core_input] exit chord requested\n";
        exit_logged = true;
      }
      return true;
    }

#if defined(__linux__)
    if (EnsureFifo()) {
      Inject(dt, now);
    } else if (EnsureDisplay(now)) {
      Inject(dt, now);
    }
#endif
    if (now - last_stats_report >= std::chrono::seconds(5)) {
      const InputPumpStats pump = input.TakePumpStats();
      std::clog << "[core_input] stats pump_polls=" << pump_polls
                << " sdl_events=" << pump.sdl_events
                << " sdl_axis_events=" << pump.sdl_axis_events
                << " linux_events=" << pump.linux_events
                << " linux_abs_events=" << pump.linux_abs_events
                << " linux_key_events=" << pump.linux_key_events
                << " cursor_axis_updates=" << pump.cursor_axis_updates
                << " axis_state_changes=" << axis_state_changes
                << " axis_heartbeats=" << axis_heartbeats
                << " ipc_writes=" << ipc_writes
                << " ipc_write_failures=" << ipc_write_failures
                << " ipc_reconnects=" << ipc_reconnects << '\n';
      pump_polls = axis_state_changes = axis_heartbeats = 0;
      ipc_writes = ipc_write_failures = ipc_reconnects = 0;
      last_stats_report = now;
    }
    return false;
  }

  ~Impl() {
#if defined(__linux__)
    if (fifo_fd >= 0) close(fifo_fd);
    if (!fifo_path.empty()) unlink(fifo_path.c_str());
    unsetenv("ROCGALGAME_KRKR_INPUT_FIFO");
    if (display && close_display) close_display(display);
    if (xtst_library) dlclose(xtst_library);
    if (x11_library) dlclose(x11_library);
#endif
  }
};

CoreInputBridge::CoreInputBridge(InputManager &input,
                                 const EffectiveGameSettings &settings,
                                 std::string display)
    : impl_(std::make_unique<Impl>(input, settings, std::move(display))) {
  input.ResetAll();
  input.TakePumpStats();
#if defined(__linux__)
  impl_->fifo_path = "/tmp/rocgalgame-krkr2-input-" +
                     std::to_string(static_cast<long long>(getpid())) + ".fifo";
  unlink(impl_->fifo_path.c_str());
  if (mkfifo(impl_->fifo_path.c_str(), 0600) == 0) {
    setenv("ROCGALGAME_KRKR_INPUT_FIFO", impl_->fifo_path.c_str(), 1);
    std::signal(SIGPIPE, SIG_IGN);
  } else {
    std::clog << "[core_input] failed to create fifo errno=" << errno << "\n";
    impl_->fifo_path.clear();
  }
#endif
}

CoreInputBridge::~CoreInputBridge() = default;

bool CoreInputBridge::Poll() { return impl_->Poll(); }
