#include "gfx/clock_face.h"
#include "gfx/clock_font.h"
#include "hal/panel.h"
#include "config/app_config.h"
#include <cstring>

// Bitmap gốc: giữ nguyên từng pixel, không phóng méo chữ theo chiều ngang/dọc.
static int small_width(char c) {
    return c==' '?3:(c=='.'||c==':'?1:5);
}
static int advance(char c) {
    // Dấu chấm/space tự tạo khoảng cách; không thêm một cột trống nữa.
    return small_width(c)+(c==' '||c=='.'?0:1);
}
static void small_glyph(char c,int x,int y,Rgb color,int clip_top) {
    const char *found=strchr(CLOCK_SMALL_CODES,c);
    if(!found||c==' ') return;
    const auto &rows=CLOCK_SMALL[found-CLOCK_SMALL_CODES];
    for(int row=0;row<8;++row) for(int col=0;col<5;++col) {
        const int px=x+(c=='.'?0:col), py=y+row;
        if(c=='.'&&col!=2) continue;
        if(px<0||px>=PANEL_W||py<clip_top||py>=PANEL_H) continue;
        if(rows[row]&(16>>col)) panel_set_pixel(px,py,color.r,color.g,color.b);
    }
}
int clock_face_width(const char *text,int) {
    int width=0;
    for(const char *p=text;*p;++p) width+=advance(*p);
    return width&&text[strlen(text)-1]!=' '&&text[strlen(text)-1]!='.'?width-1:width;
}
void clock_face_draw(const char *text,int y,int,Rgb color) {
    int x=(PANEL_W-clock_face_width(text,1))/2;
    for(;*text;++text) {small_glyph(*text,x,y,color,0);x+=advance(*text);}
}
void clock_face_time(int hour,int minute,bool colon,Rgb color) {
    const int digits[]={hour/10,hour%10,minute/10,minute%10};
    const int xs[]={0,14,36,50};
    for(int i=0;i<4;++i) for(int row=0;row<19;++row) for(int col=0;col<14;++col) {
        if(CLOCK_DIGITS[digits[i]][row]&(1<<(13-col)))
            panel_set_pixel(xs[i]+col,row+1,color.r,color.g,color.b);
    }
    if(colon) for(int row=0;row<2;++row) for(int col=0;col<2;++col) {
        panel_set_pixel(30+col,6+row,color.r,color.g,color.b);
        panel_set_pixel(30+col,13+row,color.r,color.g,color.b);
    }
}
int clock_face_date_width(const char *text) {return clock_face_width(text,1);}
void clock_face_date(const char *text,int offset_y,Rgb color) {
    int x=(PANEL_W-clock_face_date_width(text))/2;
    for(;*text;++text) {
        // Trắng giữ dấu / nổi bật với mọi màu lịch bão hòa theo giờ.
        const Rgb ink=*text=='/'?Rgb{255,255,255}:color;
        small_glyph(*text,x,23+offset_y,ink,22);
        x+=advance(*text);
    }
}

void clock_face_seconds(int second,Rgb color) {
    if(second<0||second>59) return;
    const int width=PANEL_W<60?PANEL_W:60;
    const int left=(PANEL_W-width)/2;
    const int lit=(second+1)*width/60;
    for(int x=0;x<lit;++x) panel_set_pixel(left+x,21,color.r,color.g,color.b);
}
