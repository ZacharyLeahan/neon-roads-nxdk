#include <stdlib.h>
#include "game.h"
#include <hal/video.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define W 640
#define H 480
static uint32_t frame[H][W], sky[H][W];
static const unsigned char font[] = {
#include <hal/font_unscii_16.h>
};

static int clamp(int x,int lo,int hi) { return x<lo?lo:x>hi?hi:x; }
static void span(int y,int x0,int x1,unsigned c)
{
    if ((unsigned)y>=H) return;
    x0=clamp(x0,0,W); x1=clamp(x1,0,W);
    for (int x=x0;x<x1;++x) frame[y][x]=c;
}
static void box(int x,int y,int w,int h,unsigned c)
{
    for (int i=0;i<h;++i) span(y+i,x,x+w,c);
}
static unsigned blend(unsigned a,unsigned b,int ratio)
{
    unsigned rb=(((a&0xff00ff)*(256-ratio)+(b&0xff00ff)*ratio)>>8)&0xff00ff;
    unsigned g=(((a&0xff00)*(256-ratio)+(b&0xff00)*ratio)>>8)&0xff00;
    return rb|g;
}
static void backdrop(const Road *r)
{
    static const Road *cached;
    if (cached==r) return;
    cached=r;
    for(int y=0;y<H;++y) {
        unsigned c=blend(r->colors[7],r->colors[8],clamp(y*256/168,0,256));
        if(y>168) c=blend(c,0x080819,clamp((y-168)*2,0,256));
        for(int x=0;x<W;++x) sky[y][x]=c;
    }
    unsigned rng=137;
    for(int i=0;i<165;++i) {
        rng=rng*1664525u+1013904223u;
        int x=(rng>>12)%W, y=(rng>>3)%146;
        sky[y][x]=blend(0x645b86,0xffffff,rng>>24);
    }
    for(int y=-35;y<=35;++y) {
        if(y>6 && y%7<2) continue;
        int dx=(int)sqrtf(35*35-y*y);
        for(int x=450-dx;x<=450+dx;++x)
            sky[118+y][x]=blend(0xffdb93,r->colors[9],(y+35)*3);
    }
}
typedef struct {float x,y;} Point;
static void triangle(Point a,Point b,Point c,unsigned color)
{
    Point t;
    if(a.y>b.y){t=a;a=b;b=t;}
    if(b.y>c.y){t=b;b=c;c=t;}
    if(a.y>b.y){t=a;a=b;b=t;}
    if(c.y-a.y<.01f) return;
    float slope=(c.x-a.x)/(c.y-a.y);
    for(int part=0;part<2;++part){
        Point p=part?b:a, q=part?c:b;
        if(q.y-p.y<.001f) continue;
        float edge=(q.x-p.x)/(q.y-p.y);
        int y0=clamp((int)ceilf(p.y-.5f),0,H), y1=clamp((int)ceilf(q.y-.5f),0,H);
        for(int y=y0;y<y1;++y) {
            float x=a.x+(y+.5f-a.y)*slope, x2=p.x+(y+.5f-p.y)*edge;
            span(y,(int)ceilf(fminf(x,x2)-.5f),(int)ceilf(fmaxf(x,x2)-.5f),color);
        }
    }
}
static void quad(Point a,Point b,Point c,Point d,unsigned color)
{
    triangle(a,b,c,color); triangle(a,c,d,color);
}
static Point project(const Race *s,float x,float y,float depth)
{
    float scale=340.f/depth;
    return (Point){320+(x-s->x*.66f)*scale,149+(2.6f+fmaxf(0,s->y)*.3f-y)*scale};
}

static unsigned surface(const Road *r,int tile)
{
    static const unsigned special[]={0xff4139,0x2fb8ff,0x24d566,0x516c2c,0xc2d6e8};
    return tile<3?r->colors[tile]:special[tile-3];
}
static void road_view(const Race *s)
{
    const Road *r=s->road;
    int current=(int)s->z;
    for(int row=clamp(current+63,0,r->length-1);row>=clamp(current-2,0,r->length-1);--row) {
        float near=row-s->z+4.2f, far=near+1;
        for(int lane=0;lane<7;++lane) {
            int cell=road_cell(r,lane,row);
            if(!cell) continue;
            float left=lane-3.5f,right=left+1;
            int block=(cell>>5)&3,tunnel=cell&128;
            float height=block*.5f;
            unsigned color=surface(r,(cell>>1)&15);
            if(block) color=(cell>>8)&15 ? surface(r,(cell>>8)&15) : r->colors[cell&4096?4:3];
            if((row+lane)&1) color=blend(color,0xffffff,14);
            Point a=project(s,left,height,near),b=project(s,right,height,near);
            Point c=project(s,right,height,far),d=project(s,left,height,far);
            quad(a,b,c,d,color);
            quad(a,b,project(s,right,-.14f,near),project(s,left,-.14f,near),blend(color,0x150824,90));
            if(block){
                float edge=left>s->x*.66f?left:right;
                quad(project(s,edge,height,near),project(s,edge,height,far),
                     project(s,edge,0,far),project(s,edge,0,near),blend(color,0,80));
            }
            if(tunnel){
                // Opening and curved roof match the inner/outer simulation radii.
                static const float cs[]={1,.866f,.5f,0,-.5f,-.866f,-1};
                static const float sn[]={0,.5f,.866f,1,.866f,.5f,0};
                float mid=left+.5f;
                for(int k=0;k<6;++k){
                    float x0=mid+cs[k]*.5f,x1=mid+cs[k+1]*.5f;
                    float h0=block?height:.5f*sn[k],h1=block?height:.5f*sn[k+1];
                    Point o0=project(s,x0,h0,near),o1=project(s,x1,h1,near);
                    quad(o0,o1,project(s,x1,h1,far),project(s,x0,h0,far),r->colors[5]);
                    quad(o0,o1,project(s,mid+cs[k+1]*.45f,sn[k+1]*.45f,near),
                         project(s,mid+cs[k]*.45f,sn[k]*.45f,near),r->colors[6]);
                }
                if(block) {
                    // Restore the open bore on the leading face of a solid tunnel.
                    if(!(road_cell(r,lane,row-1)&128)) {
                        Point midfloor=project(s,mid,0,near);
                        for(int k=0;k<6;++k) triangle(midfloor,project(s,mid+cs[k]*.45f,sn[k]*.45f,near),
                            project(s,mid+cs[k+1]*.45f,sn[k+1]*.45f,near),0x180e25);
                    }
                }
            }
        }
    }
}
static void text(int x,int y,const char *s,unsigned color)
{
    for(;*s && x<W-8;++s,x+=8)
        for(int j=0;j<16;++j){
            unsigned bits=font[(unsigned char)*s*16+j];
            for(int i=0;i<8;++i) if(bits&(128>>i)) frame[y+j][x+i]=color;
        }
}
static void reading(char *out,size_t size,int value,int valid)
{
    if(valid) snprintf(out,size,"%s%d.%02d",value<0?"-":"",abs(value)/100,abs(value)%100);
    else snprintf(out,size,"--");
}
static void board_hud(const Race *s,BoardAngles a)
{
    const int x=34,w=252;
    char roll[12],pitch[12],lift[12],accel_y[12],ship[12],line[82];
    reading(roll,sizeof(roll),a.roll100,a.pose_valid);
    reading(pitch,sizeof(pitch),a.pitch100,a.pose_valid);
    reading(lift,sizeof(lift),a.lift100,a.lift_valid);
    reading(accel_y,sizeof(accel_y),a.accel_y100,a.y_valid);
    reading(ship,sizeof(ship),(int)roundf(s->y*100),1);
    box(24,377,592,78,0x141724);
    snprintf(line,sizeof(line),"Pitch %s deg",pitch);text(x,380,line,0xffe59a);
    snprintf(line,sizeof(line),"Roll %s deg",roll);text(330,380,line,0xbde7ff);
    for(int i=0;i<2;++i){
        int start=i?330:x, range=i?1000:50, value=i?a.roll100:a.pitch100;
        box(start,401,w,6,0x394958);
        box(start+w/2,399,1,10,0xa0aab5);
        if(a.pose_valid)box(start+w/2+clamp(value,-range,range)*(w/2-3)/range-2,398,5,12,i?0x74d9ff:0xffcf65);
    }
    snprintf(line,sizeof(line),"Height command %d%%      Ship Y %s   %s",a.height,ship,
        s->phase==FINISHED?"FINISHED":s->phase==LOST?"LOST":s->phase==READY?"READY":s->grounded?"ON ROAD":"IN AIR");
    text(x,413,line,0xffffff);
    snprintf(line,sizeof(line),"Phone acceleration (m/s2)    Y %s    Lift Z %s",accel_y,lift);
    text(x,434,line,0xb5cbd7);
}
void race_draw(const Race *s,int setup,BoardAngles angles)
{
    backdrop(s->road);
    memcpy(frame,sky,sizeof(frame));
    road_view(s);
    Point pos=project(s,s->x,s->y,4.2f);
    float x=pos.x,y=pos.y,bank=s->vx*1.4f;
    if(y<480){
        Point nose={x+bank,y-26},tail={x,y+13},l={x-27,y+10-bank},r={x+27,y+10+bank};
        triangle((Point){x-6,y+9},(Point){x+6,y+9},(Point){x,y+19+(int)(s->z*4)%8},0xff8d43);
        triangle(l,nose,tail,0x57deef); triangle(tail,nose,r,0xb39afa);
        triangle((Point){x-6,y+2},nose,(Point){x+6,y+2},0x26364e);
    }
    char hud[100];
    box(24,20,592,51,0x161124);
    snprintf(hud,sizeof(hud),"%s  /  %s",s->road->world,s->road->name);
    text(34,24,hud,0xffebff);
    snprintf(hud,sizeof(hud),"O2 %3d   FUEL %4d   SPEED %2d      %3d%%",(int)s->oxygen,(int)s->fuel,(int)s->vz,(int)(s->z*100/s->road->length));
    text(34,45,hud,0xa9f8e0);
    box(24,71,(int)(592*fminf(1,s->z/s->road->length)),3,s->road->colors[6]);
    if(setup){
        int stage=setup<0?-setup:setup;
        box(38,150,564,166,0x181227);
        snprintf(hud,sizeof(hud),"Board setup   %d / 6",stage==1?1:stage<=3?2:stage<=5?3:stage<=7?4:stage-3);
        text(58,165,hud,0xffd48d);
        if(stage>=2 && stage<=7)
            snprintf(hud,sizeof(hud),"%s   %d of 2",stage<=3?"Lean left":stage<=5?"Lean right":"Kick the board up",1+(stage-2)%2);
        else snprintf(hud,sizeof(hud),"%s",stage==1?"Center the phone on your board":
            stage==8?"How will you signal ready to play?":"Board setup complete");
        text(58,192,hud,0xffffff);
        int live=angles.live<0?-angles.live:angles.live;
        int peak=angles.peak<0?-angles.peak:angles.peak;
        if(stage==9){
            int l=(abs(angles.left)*8+5)/10, r=(abs(angles.right)*8+5)/10;
            snprintf(hud,sizeof(hud),"Full steer L %d.%d / R %d.%d deg    Jump %d.%d deg",
                l/10,l%10,r/10,r%10,angles.jump/10,angles.jump%10);
        }else{
            snprintf(hud,sizeof(hud),"Live %d.%d deg    Measured %d.%d deg",
                live/10,live%10,peak/10,peak%10);
        }
        text(68,226,hud,0xa9f8e0);
        const char *hint=stage==1?"Hold still for 3 seconds. Movement restarts the timer.":
            stage==9?"Wait 2 seconds, then repeat your ready gesture.":
            stage==8?"Choose a tilt or twist. Hold it briefly.":
            stage<=5?"Lean comfortably, then ease off a little.":
            "Lift either end briefly, then ease off.";
        text(58,256,hint,0xffd48d);
    } else if(s->phase!=RACING){
        box(124,190,392,81,0x181227);
        const char *title=s->phase==FINISHED?"Road complete":s->phase==LOST?s->cause:"Ready to launch";
        text(144,201,title,0xffd48d);
        text(144,226,"Rest board flat for 2s to start",0xe3fffb);
        box(144,250,352,6,0x394958);
        box(144,250,352*angles.settle/100,6,0xffcf65);
    }
    if(!setup)board_hud(s,angles);
    memcpy(XVideoGetFB(),frame,sizeof(frame)); XVideoFlushFB();
}
