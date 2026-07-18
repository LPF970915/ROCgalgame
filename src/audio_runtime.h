#pragma once

#include <SDL.h>

#include <filesystem>

enum class SfxId { Move, Select, Back, Change };

class SfxBank {
public:
  bool Init(const std::filesystem::path &root);
  void Shutdown();
  void Play(SfxId id);
  void SetVolume(int volume);

private:
  struct PcmData {
    Uint8 *data = nullptr;
    Uint32 len = 0;
  };

  PcmData LoadPcm(const std::filesystem::path &path);
  static void FreePcm(PcmData &pcm);

  PcmData move_{};
  PcmData select_{};
  PcmData back_{};
  PcmData change_{};
  SDL_AudioDeviceID device_ = 0;
  SDL_AudioSpec spec_{};
  int volume_ = SDL_MIX_MAXVOLUME;
};
