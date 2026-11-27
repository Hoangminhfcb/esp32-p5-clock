#include "gfx/clock_face.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <set>
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
        assert(pixels[6][30]&&pixels[13][30]);
        memset(pixels,0,sizeof(pixels));
        clock_face_time(h,m,false,{255,255,255});
        assert(!pixels[6][30]&&!pixels[13][30]);
        for(int y=20;y<32;++y)for(int x=0;x<64;++x)assert(!pixels[y][x]);
    }
    // Mỗi giây thêm đúng một LED, không chạm hàng giờ hoặc hàng lịch.
    for(int second=0;second<60;++second) {
        memset(pixels,0,sizeof(pixels));
        clock_face_seconds(second);
        int lit=0;
        for(int y=0;y<32;++y) for(int x=0;x<64;++x) if(pixels[y][x]) {
            assert(y==21&&x>=2&&x<62);
            ++lit;
        }
        assert(lit==second+1);
    }
    std::set<uint32_t> rainbow;
    for(int x=2;x<62;++x) rainbow.insert(colors[21][x]);
    assert(rainbow.size()>30);
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
