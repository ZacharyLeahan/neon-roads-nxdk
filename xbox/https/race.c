#include "game.h"
#include "../generated/roads.h"
#include <math.h>
#include <string.h>

int road_count(void) { return sizeof(roads)/sizeof(roads[0]); }
int road_cell(const Road *road, int lane, int row)
{
    if (lane < 0 || lane >= 7 || row < 0 || row >= road->length) return 0;
    return road->cells[row*7+lane];
}

void race_init(Race *s, int index)
{
    memset(s, 0, sizeof(*s));
    s->road = &roads[index];
    s->z = .5f;
    s->grounded = 1;
    s->fuel = s->road->fuel;
    s->oxygen = s->road->oxygen;
}

typedef struct { float lo, hi; int tile; } Span;
static int spans(int cell, float dx, Span out[2])
{
    if (!cell) return 0;
    int block = (cell >> 5) & 3, floor_tile = (cell >> 1) & 15;
    float top = .5f*block;
    int tile = block ? (cell >> 8) & 15 : floor_tile;
    if (cell & 128) {
        float d2 = dx*dx;
        if (!block) top = sqrtf(fmaxf(0,.25f-d2));
        if (d2 < .45f*.45f) {
            out[0] = (Span){-.25f,0,floor_tile};
            out[1] = (Span){sqrtf(.45f*.45f-d2),top,tile};
            return 2;
        }
    }
    out[0] = (Span){-.25f,top,tile};
    return 1;
}

/* Resolve the same five lateral and three longitudinal hull samples as collision.ts. */
static Span probe(const Road *road, float x, float z, float lo, float hi)
{
    Span result = {999,-999,0};
    for (int dz=-1; dz<=1; ++dz) {
        int row = (int)floorf(z+dz*.3f);
        for (int dx=-2; dx<=2; ++dx) {
            float sx = x+dx*.1f;
            int lane = (int)floorf(sx+3.5f);
            Span interval[2];
            int n = spans(road_cell(road,lane,row),sx-lane+3,interval);
            for (int k=0;k<n;++k) {
                Span p = interval[k];
                if (p.hi <= lo+.000001f || p.lo >= hi) continue;
                if (p.hi > result.hi || (p.hi == result.hi && !dx && !dz)) {
                    result.hi = p.hi;
                    result.tile = p.tile;
                }
                result.lo = fminf(result.lo,p.lo);
            }
        }
    }
    return result;
}

static void lose(Race *s, const char *reason) { s->phase = LOST; s->cause = reason; }

void race_step(Race *s, float steer, int brake, int jump, int board_height)
{
    const float dt = 1.f/120.f;
    if (s->phase == FINISHED && s->manual) {
        float target=s->launch_y+1.5f*board_height/100.f;
        s->y+=fmaxf(-14.f*dt,fminf(14.f*dt,target-s->y));
        if(!board_height && fabsf(s->y-s->launch_y)<.001f){
            s->manual=0;s->grounded=1;s->vy=0;
        }
    }
    if (s->phase != RACING) return;
    s->oxygen -= dt;
    if (s->oxygen <= 0) { lose(s,"Out of oxygen"); return; }
    int tile = s->grounded ? s->tile : 0;
    float cap = fmaxf(s->road->practice ? 9.f : 21.f,s->vz-4*dt);
    s->vz += (s->fuel<=0 ? -6 : brake ? -18 : 9)*dt;
    if (tile == 5) { s->vz += 40*dt; cap = 27; }
    if (tile == 6) s->vz -= s->vz*2.4f*dt;
    s->vz = fmaxf(0,fminf(cap,s->vz));
    if (s->fuel<=0 && s->vz<=0 && s->grounded) { lose(s,"Out of fuel"); return; }
    if (tile != 7) s->vx += (steer*4.6f-s->vx)*(s->grounded?22:14)*dt;
    s->coyote = s->grounded ? .07f : s->coyote-dt;
    if(jump) s->jump_strength=jump;
    s->jump_buffer = jump ? .12f : s->jump_buffer-dt;
    if (!s->manual && board_height>0 && s->coyote>0) {
        s->manual=1;
        s->launch_y=s->y;
        s->jump_strength=board_height;
        ++s->jumps;
    }
    if (!s->manual && s->coyote>0 && s->jump_buffer>0) {
        ++s->jumps;
        s->vy = 10.4f*sqrtf(s->jump_strength/100.f); s->grounded = 0; s->coyote = s->jump_buffer = 0;
    }
    if (s->manual) {
        // Anchor the manual at takeoff so holding pitch over a gap preserves altitude.
        float target=s->launch_y+1.5f*board_height/100.f;
        s->vy=fmaxf(-14.f,fminf(14.f,(target-s->y)/dt));
        s->jump_buffer=0;
        if (!board_height && s->y<=s->launch_y+.001f) s->manual=0;
    }
    if (!s->manual) s->vy -= 36*s->road->gravity/500.f*dt;
    float nz = s->z+s->vz*dt, nx = s->x+s->vx*dt;
    if(s->road->practice) nx=fmaxf(-3.2f,fminf(3.2f,nx));
    Span ahead = probe(s->road,s->x,nz,s->y,s->y+.2f);
    if (ahead.hi <= s->y+.07f) { s->z=nz; s->y=fmaxf(s->y,ahead.hi); }
    else if (s->vz>4.5f) { lose(s,"Hull impact"); return; }
    else s->vz = 0;
    Span side = probe(s->road,nx,s->z,s->y,s->y+.2f);
    if (side.hi <= s->y+.07f) { s->x=nx; s->y=fmaxf(s->y,side.hi); }
    else s->vx = 0;
    float ny = s->y+s->vy*dt;
    Span hit = probe(s->road,s->x,s->z,fminf(ny,s->y),fmaxf(ny,s->y)+.2f);
    s->grounded = s->vy<=0 && hit.hi>-999;
    if (s->grounded) {
        if(!board_height)s->manual=0;
        s->y=hit.hi; s->vy=0; s->tile=hit.tile;
        if (hit.tile==3) { lose(s,"Burning tile"); return; }
        if (hit.tile==4) { s->fuel=s->road->fuel; s->oxygen=s->road->oxygen; }
    } else if (s->vy>0 && hit.lo<999 && hit.lo>=s->y+.2f-.00001f) {
        s->y=hit.lo-.2f; s->vy=0;
    } else s->y=ny;
    s->fuel = fmaxf(0,s->fuel-s->vz*dt);
    if (s->y < -5) lose(s,"Lost in space");
    else if (s->z >= s->road->length) s->phase = FINISHED;
}
