#pragma once

#include "../MusicAPI.h"

class MusicAPILinuxOpenALImpl : public MusicAPI {
    public:
        void init();
        OpaqueMusicPtr* createPlayer(int sampleRate);
        int8_t* allocate(int size);
        void deallocate(int8_t* b);
        int pcmBufferSize(int sampleRate);
        void queueMusicData(OpaqueMusicPtr* ptr, int8_t* data, int size, int sampleRate);
        int initialPacketCount(OpaqueMusicPtr* ptr);
        void startPlaying(OpaqueMusicPtr* ptr, OpaqueMusicPtr* master, int offset);
        void stopPlayer(OpaqueMusicPtr* ptr);
        void pausePlayer(OpaqueMusicPtr* ptr);
        int getPosition(OpaqueMusicPtr* ptr);
        void setPosition(OpaqueMusicPtr* ptr, int pos);
        void setVolume(OpaqueMusicPtr* ptr, float v);
        void deletePlayer(OpaqueMusicPtr* ptr);
        bool isPlaying(OpaqueMusicPtr* ptr);
};
