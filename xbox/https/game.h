#pragma once
#include <stdint.h>

typedef struct {
    const char *world, *name;
    int length, gravity, fuel, oxygen, music;
    const unsigned short *cells;
    unsigned colors[10];
    int practice;
} Road;

typedef enum { READY, RACING, LOST, FINISHED } Phase;
typedef struct {
    const Road *road;
    Phase phase;
    float x, y, z, vx, vy, vz, fuel, oxygen, coyote, jump_buffer;
    int jump_strength, manual;
    float launch_y;
    int grounded, tile;
    unsigned jumps;
    const char *cause;
} Race;

void race_init(Race *race, int index);
int road_count(void);
int road_cell(const Road *road, int lane, int row);
void race_step(Race *race, float steer, int brake, int jump, int board_height);
typedef struct { int live, peak, left, right, jump;
    int roll100, pitch100, pose_valid, lift100, lift_valid, accel_y100, y_valid, height, settle; } BoardAngles;
void race_draw(const Race *race, int setup, BoardAngles angles);
void game_init(void);
void game_jump_stats(Phase *phase, unsigned *requests, unsigned *takeoffs, int *strength, int *y100);
Phase game_input(int degrees, int brake, unsigned action_id, char action, int strength, int height, int setup, BoardAngles angles);
void game_run(void (*pairing)(int reconnect));
