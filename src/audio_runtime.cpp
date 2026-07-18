#include "audio_runtime.h"

#include <algorithm>
#include <iostream>
#include <vector>

bool SfxBank::Init(const std::filesystem::path &root) {
  SDL_AudioSpec desired{};
  desired.freq = 44100;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = 1024;
  device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &spec_, 0);
  if (device_ == 0) {
    std::cerr << "SDL audio init failed: " << SDL_GetError() << "\n";
    return false;
  }
  move_ = LoadPcm(root / "sounds" / "move.wav");
  select_ = LoadPcm(root / "sounds" / "select.wav");
  back_ = LoadPcm(root / "sounds" / "back.wav");
  change_ = LoadPcm(root / "sounds" / "change.wav");
  SDL_PauseAudioDevice(device_, 0);
  return true;
}

void SfxBank::Shutdown() {
  FreePcm(move_);
  FreePcm(select_);
  FreePcm(back_);
  FreePcm(change_);
  if (device_ != 0) SDL_CloseAudioDevice(device_);
  device_ = 0;
}

void SfxBank::Play(SfxId id) {
  if (device_ == 0 || volume_ <= 0) return;
  const PcmData *pcm = &change_;
  if (id == SfxId::Move) pcm = &move_;
  else if (id == SfxId::Select) pcm = &select_;
  else if (id == SfxId::Back) pcm = &back_;
  if (!pcm->data || pcm->len == 0) return;
  SDL_ClearQueuedAudio(device_);
  if (volume_ >= SDL_MIX_MAXVOLUME) {
    SDL_QueueAudio(device_, pcm->data, pcm->len);
    return;
  }
  std::vector<Uint8> mixed(pcm->len, 0);
  SDL_MixAudioFormat(mixed.data(), pcm->data, spec_.format, pcm->len, volume_);
  SDL_QueueAudio(device_, mixed.data(), pcm->len);
}

void SfxBank::SetVolume(int volume) { volume_ = std::clamp(volume, 0, SDL_MIX_MAXVOLUME); }

SfxBank::PcmData SfxBank::LoadPcm(const std::filesystem::path &path) {
  PcmData out{};
  SDL_AudioSpec wav_spec{};
  Uint8 *wav = nullptr;
  Uint32 wav_len = 0;
  if (!SDL_LoadWAV(path.u8string().c_str(), &wav_spec, &wav, &wav_len)) return out;
  SDL_AudioCVT cvt{};
  if (SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq,
                        spec_.format, spec_.channels, spec_.freq) < 0) {
    SDL_FreeWAV(wav);
    return out;
  }
  cvt.len = static_cast<int>(wav_len);
  cvt.buf = static_cast<Uint8 *>(SDL_malloc(cvt.len * std::max(1, cvt.len_mult)));
  if (!cvt.buf) {
    SDL_FreeWAV(wav);
    return out;
  }
  SDL_memcpy(cvt.buf, wav, wav_len);
  SDL_FreeWAV(wav);
  if (cvt.needed && SDL_ConvertAudio(&cvt) != 0) {
    SDL_free(cvt.buf);
    return out;
  }
  out.len = cvt.needed ? static_cast<Uint32>(cvt.len_cvt) : wav_len;
  out.data = static_cast<Uint8 *>(SDL_malloc(out.len));
  if (out.data) SDL_memcpy(out.data, cvt.buf, out.len);
  SDL_free(cvt.buf);
  return out;
}

void SfxBank::FreePcm(PcmData &pcm) {
  if (pcm.data) SDL_free(pcm.data);
  pcm = {};
}
