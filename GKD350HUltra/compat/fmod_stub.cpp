#include "fmod.hpp"

#include <cstddef>

namespace {

alignas(std::max_align_t) unsigned char system_storage[1];
alignas(std::max_align_t) unsigned char sound_storage[1];
alignas(std::max_align_t) unsigned char channel_storage[1];

void *sound_user_data = nullptr;
void *channel_user_data = nullptr;

FMOD::System *system_instance() {
    return reinterpret_cast<FMOD::System *>(system_storage);
}

FMOD::Sound *sound_instance() {
    return reinterpret_cast<FMOD::Sound *>(sound_storage);
}

FMOD::Channel *channel_instance() {
    return reinterpret_cast<FMOD::Channel *>(channel_storage);
}

} // namespace

FMOD_RESULT F_API FMOD_System_Create(FMOD_SYSTEM **system) {
    if(system) {
        *system = reinterpret_cast<FMOD_SYSTEM *>(system_instance());
    }
    return FMOD_OK;
}

namespace FMOD {

FMOD_RESULT F_API System::release() { return FMOD_OK; }
FMOD_RESULT F_API System::setOutput(FMOD_OUTPUTTYPE) { return FMOD_OK; }
FMOD_RESULT F_API System::init(int, FMOD_INITFLAGS, void *) { return FMOD_OK; }
FMOD_RESULT F_API System::close() { return FMOD_OK; }
FMOD_RESULT F_API System::update() { return FMOD_OK; }

FMOD_RESULT F_API System::createSound(const char *, FMOD_MODE,
                                     FMOD_CREATESOUNDEXINFO *, Sound **sound) {
    if(sound) {
        *sound = sound_instance();
    }
    return FMOD_OK;
}

FMOD_RESULT F_API System::playSound(Sound *, ChannelGroup *, bool,
                                   Channel **channel) {
    if(channel) {
        *channel = channel_instance();
    }
    return FMOD_OK;
}

FMOD_RESULT F_API Sound::release() { return FMOD_OK; }

FMOD_RESULT F_API Sound::getLength(unsigned int *length, FMOD_TIMEUNIT) {
    if(length) {
        *length = 0;
    }
    return FMOD_OK;
}

FMOD_RESULT F_API Sound::setUserData(void *userdata) {
    sound_user_data = userdata;
    return FMOD_OK;
}

FMOD_RESULT F_API Sound::getUserData(void **userdata) {
    if(userdata) {
        *userdata = sound_user_data;
    }
    return FMOD_OK;
}

FMOD_RESULT F_API ChannelControl::stop() { return FMOD_OK; }
FMOD_RESULT F_API ChannelControl::setPaused(bool) { return FMOD_OK; }
FMOD_RESULT F_API ChannelControl::setVolume(float) { return FMOD_OK; }
FMOD_RESULT F_API ChannelControl::setMode(FMOD_MODE) { return FMOD_OK; }

FMOD_RESULT F_API
ChannelControl::setCallback(FMOD_CHANNELCONTROL_CALLBACK) {
    return FMOD_OK;
}

FMOD_RESULT F_API ChannelControl::setUserData(void *userdata) {
    channel_user_data = userdata;
    return FMOD_OK;
}

FMOD_RESULT F_API ChannelControl::getUserData(void **userdata) {
    if(userdata) {
        *userdata = channel_user_data;
    }
    return FMOD_OK;
}

FMOD_RESULT F_API Channel::setPosition(unsigned int, FMOD_TIMEUNIT) {
    return FMOD_OK;
}

FMOD_RESULT F_API Channel::getPosition(unsigned int *position, FMOD_TIMEUNIT) {
    if(position) {
        *position = 0;
    }
    return FMOD_OK;
}

FMOD_RESULT F_API Channel::setLoopCount(int) { return FMOD_OK; }

} // namespace FMOD
