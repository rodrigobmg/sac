#include "SoundSystem.h"

INSTANCE_IMPL(SoundSystem);

SoundSystem::SoundSystem() : ComponentSystemImpl<SoundComponent>("sound"), nextValidRef(1) {
}

void SoundSystem::init() {
	#ifndef ANDROID
	ALCdevice* Device = alcOpenDevice(NULL);
	ALCcontext* Context = alcCreateContext(Device, NULL);
	if (!(Device && Context && alcMakeContextCurrent(Context)))
		LOGI("probleme initialisation du son");
	#endif
}

SoundRef SoundSystem::loadSoundFile(const std::string& assetName, bool music) {
	#ifdef ANDROID
	if (!music && assetSounds.find(assetName) != assetSounds.end())
	#else
	if (assetSounds.find(assetName) != assetSounds.end())
	#endif
		return assetSounds[assetName];

	#ifdef ANDROID
	int soundID = androidSoundAPI->loadSound(assetName, music);
	#else
	ALuint soundID = linuxSoundAPI->loadSound(assetName);
	#endif

	sounds[nextValidRef] = soundID;
	assetSounds[assetName] = nextValidRef;
	LOGW("Sound : %s -> %d -> %d", assetName.c_str(), nextValidRef, soundID);

	return nextValidRef++;
}

void SoundSystem::DoUpdate(float dt) {
	/* play component with a valid sound ref */
	for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
		Entity a = (*it).first;
		SoundComponent* rc = (*it).second;
		if (rc->sound != InvalidSoundRef) {
			if (!rc->started) {
				LOGW("sound started (%d)", rc->sound);
				#ifdef ANDROID
				androidSoundAPI->play(sounds[rc->sound], (rc->type == SoundComponent::MUSIC));
				#else
				linuxSoundAPI->play(sounds[rc->sound]);
				#endif
				rc->started = true;
			} else if (rc->type == SoundComponent::MUSIC) {
				 #ifdef ANDROID
				float newPos = androidSoundAPI->musicPos(sounds[rc->sound]);
				#else
				ALfloat newPos = linuxSoundAPI->musicPos(sounds[rc->sound]);
				#endif
				if (newPos >= 0.999) {
					LOGW("sound ended (%d)", rc->sound);
					rc->position = 0;
					rc->sound = InvalidSoundRef;
					rc->started = false;
				} else {
					rc->position = newPos;
				}
			} else if (rc->type == SoundComponent::EFFECT) {
				rc->sound = InvalidSoundRef;
				rc->started = false;
			}
		}
	}
}
