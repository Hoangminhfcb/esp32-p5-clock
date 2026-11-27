#include "gfx/clock_face.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <set>
#include <cstdlib>
#include "app/clock_palette.h"
static bool pixels[32][64];
static uint32_t colors[32][64];
static int clip_top=0;
static int white_pixels=0, colored_pixels=0;
void panel_set_pixel(int x,int y,uint8_t r,uint8_t g,uint8_t b) {
    if(r==255&&g==255&&b==255) ++white_pixels; else ++colored_pixels;
    assert(x>=0&&x<64&&y>=clip_top&&y<32);
    pixels[y][x]=true;
    colors[y][x]=(r<<16)|(g<<8)|b;
}
int main() {
    for(int h=0;h<24;++h) for(int m=0;m<60;++m) {
        clip_top=0;
        memset(pixels,0,sizeof(pixels));
        clock_face_time(h,m,true,{255,255,255});
        assert(pixels[7][31]&&pixels[13][31]);
        memset(pixels,0,sizeof(pixels));
        clock_face_time(h,m,false,{255,255,255});
        assert(!pixels[7][31]&&!pixels[13][31]);
        for(int y=0;y<32;++y)for(int x=0;x<64;++x)if(pixels[y][x])assert(x>0&&x<63&&y>0&&y<20);
    }
    // Đuôi rainbow chạy liên tục quanh mép; không tích lũy sáng cả viền.
    std::set<int> visited;
    uint32_t initial[32][64]={};
    for(int ms=0;ms<60000;ms+=40) {
        memset(pixels,0,sizeof(pixels));memset(colors,0,sizeof(colors));
        clock_face_seconds(ms);
        int count=0;
        for(int y=0;y<32;++y) for(int x=0;x<64;++x) if(pixels[y][x]) {
            assert(x==0||x==63||y==0||y==31);
            visited.insert(y*64+x);++count;
        }
        assert(count>0&&count<=25);
        if(ms==0) {assert(pixels[0][32]);memcpy(initial,colors,sizeof(initial));}
    }
    assert(visited.size()==188);
    memset(pixels,0,sizeof(pixels));memset(colors,0,sizeof(colors));
    clock_face_seconds(59999);
    int wrap_difference=0;
    for(int y=0;y<32;++y)for(int x=0;x<64;++x)for(int shift:{0,8,16})
        wrap_difference+=std::abs(int((initial[y][x]>>shift)&255)-int((colors[y][x]>>shift)&255));
    assert(wrap_difference<500);
    memset(colors,0,sizeof(colors));clock_face_seconds(0);
    const uint32_t first_head=colors[0][32];
    memset(colors,0,sizeof(colors));clock_face_seconds(40);
    assert(colors[0][32]!=first_head); // Có nội suy trước khi nhảy sang pixel kế tiếp.
    for(bool connecting:{false,true}) {
        memset(pixels,0,sizeof(pixels));
        clock_face_status(connecting);
        int count=0;
        for(int y=0;y<32;++y)for(int x=0;x<64;++x) if(pixels[y][x]) {assert(y>=12&&y<20);++count;}
        assert(count>40);
    }
    const uint32_t palette[]={0xFF0000,0x00FF00,0x0000FF,0xFFFF00};
    for(int cycle:{24,28}) {
        std::set<uint32_t> unique;
        for(int slot=0;slot<cycle;++slot) {
            const auto a=clock_palette_mix(palette,slot,cycle),b=clock_palette_mix(palette,slot+cycle,cycle);
            assert(a.r==b.r&&a.g==b.g&&a.b==b.b);
            unique.insert((a.r<<16)|(a.g<<8)|a.b);
        }
        assert(unique.size()==static_cast<size_t>(cycle));
    }
    clip_top=22;
    char text[32];
    for(const char *wd:{"CN","T2","T3","T4","T5","T6","T7"})
      for(int d=1;d<=31;++d)for(int m=1;m<=12;++m) {
        snprintf(text,sizeof(text),"%s %02d/%02d/26",wd,d,m);
        assert(clock_face_date_width(text)<=64);
        for(int offset=-10;offset<=10;++offset) clock_face_date(text,offset,{255,255,255});
      }
    for(const char *lunar:{"19/10 AL","29/12 ALN"}) {
        assert(clock_face_date_width(lunar)<=64);
        for(int offset=-10;offset<=10;++offset) clock_face_date(lunar,offset,{255,255,255});
    }
    white_pixels=colored_pixels=0;
    clock_face_date("T6 27/11/26",0,{80,180,255});
    assert(white_pixels==16&&colored_pixels>0);
    white_pixels=colored_pixels=0;
    clock_face_date("19/10 AL",0,{255,80,120});
    assert(white_pixels==8&&colored_pixels>0);
    puts("Clock tests passed: all hours, colon, calendar clipping, rainbow seconds, status text and 24/28 color cycles");
}
