#pragma once
#include "game.h"

enum SoundEvent {
    SOUND_LAUNCH = 1, SOUND_JUMP = 2, SOUND_LAND = 4, SOUND_LOSS = 8,
    SOUND_FINISH = 16, SOUND_BOOST = 32, SOUND_SUPPLY = 64, SOUND_STICKY = 128
};
int audio_open(void);
void audio_update(const Race *race, int connected, unsigned events);
