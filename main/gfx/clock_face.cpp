#include "gfx/clock_face.h"
#include "hal/panel.h"
#include "config/app_config.h"
#include <cstring>
#include <initializer_list>
static const char CODES[]="0123456789:/. -ACFGILNOTWY";
static const uint8_t GLYPHS[][5]={
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
 {0,2,0,2,0},{1,1,2,4,4},{0,0,0,0,2},{0,0,0,0,0},{0,0,7,0,0},
 {2,5,7,5,5},{7,4,4,4,7},{7,4,6,4,4},{7,4,5,5,7},{7,2,2,2,7},
 {4,4,4,4,7},{5,7,7,7,5},{7,5,5,5,7},{7,2,2,2,2},{5,5,7,7,5},{5,5,2,2,2}
};
static_assert(sizeof(GLYPHS)/sizeof(GLYPHS[0])==sizeof(CODES)-1);
static void draw_glyph(char c,int x,int y,int width,int height,Rgb color,int clip_top=0) {
    const char *p=strchr(CODES,c);
    if(!p) return;
    const auto &rows=GLYPHS[p-CODES];
    for(int dy=0;dy<height;++dy) {
        const int py=y+dy;
        if(py<clip_top||py>=PANEL_H) continue;
        for(int dx=0;dx<width;++dx) {
            const int px=x+dx;
            if(px>=0&&px<PANEL_W&&(rows[dy*5/height]&(4>>(dx*3/width))))
                panel_set_pixel(px,py,color.r,color.g,color.b);
        }
    }
}
int clock_face_width(const char *text,int scale) {return text[0]?(strlen(text)*4-1)*scale:0;}
void clock_face_draw(const char *text,int y,int scale,Rgb color) {
    int x=(PANEL_W-clock_face_width(text,scale))/2;
    for(;*text;++text,x+=4*scale) draw_glyph(*text,x,y,3*scale,5*scale,color);
}
void clock_face_time(int hour,int minute,bool colon,Rgb color) {
    const int digits[]={hour/10,hour%10,minute/10,minute%10};
    const int xs[]={1,15,35,49};
    // Số bảy đoạn cao 20 px, nét 2 px để lòng số thoáng và dễ nhìn xa.
    static const uint8_t MASKS[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    static const int SEGMENTS[][4]={
        {2,0,8,2},{10,2,2,7},{10,11,2,7},{2,18,8,2},
        {0,11,2,7},{0,2,2,7},{2,9,8,2}
    };
    for(int i=0;i<4;++i) for(int segment=0;segment<7;++segment) {
        if(!(MASKS[digits[i]]&(1<<segment))) continue;
        const auto &rect=SEGMENTS[segment];
        for(int dy=0;dy<rect[3];++dy) for(int dx=0;dx<rect[2];++dx)
            panel_set_pixel(xs[i]+rect[0]+dx,rect[1]+dy,color.r,color.g,color.b);
    }
    if(colon) for(int y:{5,13}) for(int dy=0;dy<3;++dy) for(int dx=0;dx<3;++dx)
        panel_set_pixel(30+dx,y+dy,color.r,color.g,color.b);
}
static int char_width(char c,int width) {return c=='.'?1:c==' '?2:width;}
static int date_width(const char *text,int width) {
    int total=0;
    for(const char *p=text;*p;++p) total+=char_width(*p,width)+1;
    return total?total-1:0;
}
static int date_glyph_width(const char *text) {
    int width=6;
    while(width>3&&date_width(text,width)>PANEL_W) --width;
    return width;
}
int clock_face_date_width(const char *text) {return date_width(text,date_glyph_width(text));}
void clock_face_date(const char *text,int offset_y,Rgb color) {
    const int width=date_glyph_width(text);
    int x=(PANEL_W-date_width(text,width))/2;
    for(;*text;++text) {
        int w=char_width(*text,width);
        // Dấu chấm giữ dạng vuông, không kéo nét từ font 3 cột.
        if(*text=='.') {
            for(int dy=8;dy<10;++dy) {
                int py=22+offset_y+dy;
                if(py>=22&&py<PANEL_H&&x>=0&&x<PANEL_W) panel_set_pixel(x,py,color.r,color.g,color.b);
            }
        } else draw_glyph(*text,x,22+offset_y,w,10,color,22);
        x+=w+1;
    }
}
