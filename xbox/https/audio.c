#include "audio.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <math.h>
#include <string.h>

#define RATE 48000
typedef struct { int bpm,root,style,scale[7],progression[4]; } Theme;
#include "../generated/themes.h"

typedef struct {
    float phase, hz, sweep, gain, noise;
    int remaining, duration, delay;
} Voice;
static SDL_AudioDeviceID device;
static Voice effects[12];
static unsigned next_voice, random_state=47;
static int connected, racing, theme_index, step, beat_left, beat_length;
static float speed, motor_phase, motor_gain, bass_phase, arp_phase, bass_hz, arp_hz;

static float oscillator(float *phase,float hz)
{
    *phase += hz/RATE;
    if(*phase>=1) *phase-=1;
    return 1-4*fabsf(*phase-.5f);
}

static void tone(float start,float end,float seconds,float gain,float noise,int delay)
{
    Voice *v=&effects[next_voice++ % 12];
    int samples=(int)(RATE*seconds);
    *v=(Voice){.hz=start,.sweep=(end-start)/samples,.gain=gain,.noise=noise,
               .duration=samples,.remaining=samples,.delay=delay};
}

static float midi(int n) { return 440*powf(2,(n-69)/12.f); }

static void beat(void)
{
    const Theme *t=&themes[theme_index];
    static const int pattern[]={0,1,2,1,0,2,1,3};
    int degree=t->progression[(step/16)%4];
    int offset=pattern[(step+(t->style==1?step/4:0))%8]*2+degree;
    bass_hz=midi(t->root-12+t->scale[degree]);
    arp_hz=midi(t->root+24+t->scale[offset%7]+12*(offset/7));
    beat_length=RATE*60/(4*t->bpm);
    beat_left=beat_length;
    step=(step+1)%64;
}

static void mix(void *arg,Uint8 *out,int bytes)
{
    (void)arg;
    int16_t *pcm=(int16_t *)out;
    for(int i=0;i<bytes/4;++i){
        float sample=0;
        if(connected){
            if(!beat_left) beat();
            float env=(float)beat_left/beat_length;
            float attack=fminf(1,(beat_length-beat_left)/96.f);
            sample=oscillator(&arp_phase,arp_hz)*env*env*attack*.10f;
            sample+=oscillator(&bass_phase,bass_hz)*.11f;
            --beat_left;
        }
        float target=racing?.04f+speed*.07f:0;
        motor_gain+=(target-motor_gain)*.001f;
        sample+=oscillator(&motor_phase,42+speed*70)*motor_gain;
        for(int j=0;j<12;++j){
            Voice *v=&effects[j];
            if(!v->remaining) continue;
            if(v->delay){--v->delay;continue;}
            float env=(float)v->remaining/v->duration;
            float wave=oscillator(&v->phase,v->hz);
            if(v->noise){
                random_state=random_state*1664525u+1013904223u;
                wave=wave*(1-v->noise)+v->noise*((random_state>>16)/32768.f-1);
            }
            sample+=wave*v->gain*env*fminf(1,(v->duration-v->remaining)/96.f);
            v->hz+=v->sweep;
            --v->remaining;
        }
        int16_t value=(int16_t)(fminf(1,fmaxf(-1,sample))*30000);
        pcm[i*2]=pcm[i*2+1]=value;
    }
}

int audio_open(void)
{
    SDL_AudioSpec spec={0};
    SDL_SetMainReady();
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0) return 0;
    spec.freq=RATE;spec.channels=2;spec.format=AUDIO_S16LSB;
    spec.samples=1024;spec.callback=mix;
    device=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
    if(device) SDL_PauseAudioDevice(device,0);
    return device!=0;
}

void audio_update(const Race *race,int online,unsigned events)
{
    if(!device) return;
    SDL_LockAudioDevice(device);
    if(connected!=online || theme_index!=race->road->music){
        beat_left=step=0;
        memset(effects,0,sizeof(effects));
    }
    connected=online;
    racing=online && race->phase==RACING;
    speed=fminf(1,race->vz/21);
    theme_index=race->road->music;
    if(events&SOUND_LAUNCH) tone(880,1320,.14f,.16f,0,0);
    if(events&SOUND_JUMP) tone(260,720,.16f,.32f,.04f,0);
    if(events&SOUND_LAND) tone(110,50,.14f,.26f,.18f,0);
    if(events&SOUND_BOOST) tone(180,900,.35f,.24f,.12f,0);
    if(events&SOUND_STICKY) tone(160,60,.25f,.20f,.15f,0);
    if(events&SOUND_LOSS){
        if(!strcmp(race->cause,"Lost in space")) tone(900,120,1.1f,.28f,0,0);
        else tone(110,28,.85f,.52f,.45f,0);
    }
    if(events&(SOUND_SUPPLY|SOUND_FINISH)){
        const int notes[]={0,4,7,12,16,19,24};
        int finish=events&SOUND_FINISH;
        for(int i=0;i<(finish?7:4);++i){
            float hz=midi((finish?67:76)+notes[i]);
            tone(hz,hz,.22f,.18f,0,i*RATE/16);
        }
    }
    SDL_UnlockAudioDevice(device);
}
