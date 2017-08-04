#include "oslib/audiobackend_sdl.h"
#if USE_SDL_AUDIO
#include <SDL2/SDL.h>

static u8 samples_temp[1024 * 16];
static volatile int samples_ptr = 0;

static SDL_AudioDeviceID devid_out = 0;

static void AudioCallback(void*  userdata, Uint8* stream, int len) {
    memcpy(stream, samples_temp, len);
    //
}

// We're making these functions static - there's no need to pollute the global namespace
static void sdl_init()
{

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
	{
		if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
		{
			die("error initializing SDL audio subsystem");
		}
	}

	SDL_AudioSpec wanted;
	SDL_AudioSpec spec;

	SDL_zero(wanted);
    wanted.freq = 44100;
    wanted.format = AUDIO_S16LSB;
    wanted.channels = 2;
    wanted.samples = 4096;
	wanted.callback = 0;


	devid_out = SDL_OpenAudioDevice(NULL, SDL_FALSE, &wanted, &spec, 0);
	
	if (!devid_out) {
		die("Couldn't open an audio device for playback");
	}

	SDL_PauseAudioDevice(devid_out, 0);
}

static u32 sdl_push(void* frame, u32 samples, bool wait)
{
	if (SDL_QueueAudio(devid_out, frame, samples * 4)) {
		printf("SDL_QueueAudio failed\n");
	}

	return 1;
}

bool sdl_audio_should_wait() {
	return SDL_GetQueuedAudioSize(devid_out) > 16000;
}

static void sdl_term()
{
	SDL_PauseAudioDevice(devid_out, 1);
	SDL_CloseAudioDevice(devid_out);
}

audiobackend_t audiobackend_sdl = {
    "sdl", // Slug
    "sdl audio stream", // Name
    &sdl_init,
    &sdl_push,
    &sdl_term
};
#endif
