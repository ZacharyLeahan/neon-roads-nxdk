#include "game.h"
#include "audio.h"
#include <windows.h>
#include <math.h>

static CRITICAL_SECTION lock;
typedef struct {
    DWORD last;
    int connected, degrees, brake, play, jump, height, setup;
    BoardAngles angles;
    unsigned action_id;
} Controls;
static Controls controls;
static Phase current_phase;
static int last_takeoff_strength, ship_y100;
static unsigned jump_requests, jump_takeoffs;

void game_jump_stats(Phase *phase, unsigned *requests, unsigned *takeoffs, int *strength, int *y100)
{
    EnterCriticalSection(&lock);
    *phase=current_phase;
    *requests=jump_requests;
    *takeoffs=jump_takeoffs;
    *strength=last_takeoff_strength;
    *y100=ship_y100;
    LeaveCriticalSection(&lock);
}

void game_init(void) { InitializeCriticalSection(&lock); }

Phase game_input(int degrees,int brake,unsigned id,char action,int strength,int height,int setup,BoardAngles angles)
{
    EnterCriticalSection(&lock);
    controls.last=GetTickCount();
    controls.connected=1;
    controls.degrees=degrees;
    controls.brake=brake;controls.height=height;
    controls.setup=setup;controls.angles=angles;
    if(id && id!=controls.action_id) {
        controls.action_id=id;
        if(action=='p') controls.play=1;
        if(action=='j') { controls.jump=strength; ++jump_requests; }
    }
    Phase phase=current_phase;
    LeaveCriticalSection(&lock);
    return phase;
}

void game_run(void (*pairing)(int reconnect))
{
    int road=0, paired_screen=0, jump=0;
    Race race;
    race_init(&race,road);
    DWORD previous=GetTickCount();
    unsigned accumulator=0;
    for(;;) {
        EnterCriticalSection(&lock);
        Controls input=controls;
        controls.play=controls.jump=0;
        LeaveCriticalSection(&lock);
        DWORD now=GetTickCount(), delta=now-previous;
        previous=now;
        // Setup is a paused screen. Allow a slow HTTPS reconnect while the rider holds a pose.
        DWORD timeout=input.setup ? 15000 : 1500;
        if(!input.connected || now-input.last>timeout) {
            if(!paired_screen) { audio_update(&race,0,0); pairing(input.connected); }
            paired_screen=1;
            if(race.phase==RACING){
                race_init(&race,road);
                EnterCriticalSection(&lock);current_phase=READY;ship_y100=0;LeaveCriticalSection(&lock);
            }
            accumulator=0;
            jump=0;
            Sleep(30);
            continue;
        }
        paired_screen=0;
        unsigned sounds=0;
        if(input.setup && race.phase==RACING) race.phase=READY;
        if(!input.setup && input.play && race.phase!=RACING) {
            if(race.phase==FINISHED) race_init(&race,road=(road+1)%road_count());
            if(race.phase==LOST) race_init(&race,road);
            race.phase=RACING; sounds|=SOUND_LAUNCH;
            accumulator=0;
        }
        if(input.jump)jump=input.jump;
        float steer=fminf(1,fmaxf(-1,input.degrees/28.f));
        if(fabsf(steer)<.06f) steer=0;
        accumulator+=(delta>120?120:delta)*120;
        unsigned previous_jumps=race.jumps;
        while(accumulator>=1000) {
            int grounded=race.grounded, tile=race.tile;
            Phase phase=race.phase;
            float vy=race.vy;
            unsigned jumps_before_step=race.jumps;
            race_step(&race,steer,input.brake,jump,input.height);
            if(phase==RACING && race.phase==LOST) sounds|=SOUND_LOSS;
            if(phase==RACING && race.phase==FINISHED) sounds|=SOUND_FINISH;
            if(race.jumps!=jumps_before_step) sounds|=SOUND_JUMP;
            if(!grounded && race.grounded && vy < -2) sounds|=SOUND_LAND;
            if(race.grounded && (!grounded || tile!=race.tile)) {
                if(race.tile==4) sounds|=SOUND_SUPPLY;
                if(race.tile==5) sounds|=SOUND_BOOST;
                if(race.tile==6) sounds|=SOUND_STICKY;
            }
            jump=0;
            accumulator-=1000;
        }
        audio_update(&race,1,sounds);
        EnterCriticalSection(&lock);
        if(race.jumps!=previous_jumps) last_takeoff_strength=race.jump_strength;
        jump_takeoffs+=race.jumps-previous_jumps;
        ship_y100=(int)roundf(100*race.y);
        current_phase=race.phase;
        LeaveCriticalSection(&lock);
        race_draw(&race,input.setup,input.angles);
        Sleep(10);
    }
}
