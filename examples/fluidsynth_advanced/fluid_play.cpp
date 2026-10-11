#include <signal.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "fluid_seq.h"

enum { nch = 2 };
enum { buffer_size = 4096 };

/* variable declarations */
static Uint32 is_playing = 0; /* remaining length of the sample we have to play */

static void sig_playing(int)
{
    is_playing = 0;
}

/*
 audio callback function
 here you have to copy the data of your audio buffer into the
 requesting audio buffer (stream)
 you should only copy as much as the requested length (len)
*/
void my_audio_callback(void *midi_player, Uint8 *stream, int len)
{
    FluidMidiSeq *mp = (FluidMidiSeq*)midi_player;
    int got = mp->playBuffer(stream, len);
    if(got == 0)
        is_playing = 0;
}

int main(int argc, char **argv)
{
    static SDL_AudioSpec spec;
    int soundfont = -1;
    const char *soundFontPatch = argc >= 3 ? (const char*)argv[2] : "/usr/share/sounds/sf2/FluidR3_GM.sf2";;

    if(argc<=1)
    {
        printf("%s filename.mid [bank.sf2]\n", argv[0]);
        return 0;
    }

    FluidMidiSeq m(44100, true);

    // Load the SoundFont
    soundfont = fluid_synth_sfload(m.getSynth(), soundFontPatch, 1);

    if(!m.openMusicFile(argv[1]))
    {
        printf("File open error!\n");
        fflush(stdout);
        return 1;
    }

    // Set loop enabled
    m.setLoop(true);

    // Example of FluidSynth's reverb configuring
    fluid_synth_reverb_on(m.getSynth(), -1, 1);
    fluid_synth_set_reverb_group_roomsize(m.getSynth(), -1, 1.0);
    fluid_synth_set_reverb_group_damp(m.getSynth(), -1, 0.2);
    fluid_synth_set_reverb_group_width(m.getSynth(), -1, 12.0);
    fluid_synth_set_reverb_group_level(m.getSynth(), -1, 0.4);

    /* Initialize SDL.*/
    if(SDL_Init(SDL_INIT_AUDIO) < 0)
        return 1;

    signal(SIGINT, &sig_playing);
    signal(SIGTERM, &sig_playing);

    memset(&spec, 0, sizeof(SDL_AudioSpec));
    spec.freq = m.sampleRate();
    spec.format = m.isFloat32() ? AUDIO_F32SYS : AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = buffer_size;

    /* set the callback function */
    spec.callback = my_audio_callback;
    /* set ADLMIDI's descriptor as userdata to use it for sound generation */
    spec.userdata = &m;

    /* Open the audio device */
    if (SDL_OpenAudio(&spec, nullptr) < 0)
    {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        fflush(stderr);
        return 1;
    }

    is_playing = 1;
    /* Start playing */
    SDL_PauseAudio(0);

    printf("Playing... Hit Ctrl+C to quit!\n");
    fflush(stdout);

    /* wait until we're don't playing */
    while(is_playing)
    {
        SDL_Delay(100);
    }

    /* shut everything down */
    SDL_CloseAudio();

    return 0;
}
