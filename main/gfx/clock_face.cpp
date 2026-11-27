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
    const int xs[]={3,17,34,48};
    for(int i=0;i<4;++i) for(int row=0;row<13;++row) for(int col=0;col<13;++col) {
        if(CLOCK_DIGITS[digits[i]][row]&(1<<(12-col)))
            panel_set_pixel(xs[i]+col,row+4,color.r,color.g,color.b);
    }
    if(colon) for(int row=0;row<2;++row) for(int col=0;col<2;++col) {
        panel_set_pixel(31+col,7+row,color.r,color.g,color.b);
        panel_set_pixel(31+col,13+row,color.r,color.g,color.b);
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

void clock_face_seconds(int second) {
    if(second<0||second>59) return;
    // Khung (0,1)..(63,20): 164 điểm duy nhất, bắt đầu giữa cạnh trên.
    constexpr int RIGHT=63,TOP=1,BOTTOM=20,PERIMETER=164;
    const int lit=(second+1)*PERIMETER/60;
    for(int i=0;i<lit;++i) {
        // Thứ tự chuẩn: góc trên trái -> phải -> dưới -> trái -> trên.
        // Dịch 32 điểm để mốc 00 giây ở giữa cạnh trên, x=32.
        int position=(i+32)%PERIMETER;
        int x,y;
        if(position<63) {x=position;y=TOP;}
        else if(position<82) {x=RIGHT;y=TOP+position-63;}
        else if(position<145) {x=RIGHT-(position-82);y=BOTTOM;}
        else {x=0;y=BOTTOM-(position-145);}
        const Rgb color=hue_to_rgb(static_cast<uint8_t>(i*256/PERIMETER+second*4));
        panel_set_pixel(x,y,color.r,color.g,color.b);
    }
}

void clock_face_status(bool connecting) {
    clock_face_draw(connecting?"CONNECTING":"SETTING",12,1,{80,190,255});
}
