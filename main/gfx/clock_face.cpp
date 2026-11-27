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
static void small_glyph(char c,int x,int y,Rgb color,int clip_top,int clip_bottom=PANEL_H) {
    const char *found=strchr(CLOCK_SMALL_CODES,c);
    if(!found||c==' ') return;
    const auto &rows=CLOCK_SMALL[found-CLOCK_SMALL_CODES];
    for(int row=0;row<8;++row) for(int col=0;col<5;++col) {
        const int px=x+(c=='.'?0:col), py=y+row;
        if(c=='.'&&col!=2) continue;
        if(px<0||px>=PANEL_W||py<clip_top||py>=clip_bottom) continue;
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
void clock_face_time(int hour,int minute,uint32_t second_ms,Rgb color) {
    const int digits[]={hour/10,hour%10,minute/10,minute%10};
    const int xs[]={0,15,36,51};
    // Bộ số lấy từ mẫu 8: sát trên/trái, toàn hàng dùng cột 0..62.
    for(int i=0;i<4;++i) for(int row=0;row<21;++row) for(int col=0;col<12;++col) {
        if(CLOCK_DIGITS[digits[i]][row]&(1<<(11-col)))
            panel_set_pixel(xs[i]+col,row,color.r,color.g,color.b);
    }
    // Hai chấm đi vào nhau, chạm ở nửa giây rồi nảy ra; nội suy để không giật pixel.
    const int ms=second_ms%1000;
    const int progress=(ms<=500?ms:1000-ms)*256/500;
    const int eased=progress*progress*(768-2*progress)/65536;
    const int positions[]={6*256+3*eased,14*256-3*eased};
    for(int position:positions) {
        const int top=position/256,fraction=position%256;
        const int weights[]={256-fraction,256,fraction};
        for(int row=0;row<3;++row) if(weights[row]) for(int col=0;col<2;++col)
            panel_set_pixel(31+col,top+row,color.r*weights[row]/256,color.g*weights[row]/256,color.b*weights[row]/256);
    }
}
int clock_face_date_width(const char *text) {return clock_face_width(text,1);}
void clock_face_date(const char *text,int offset_y,Rgb color) {
    int x=(PANEL_W-clock_face_date_width(text))/2;
    for(;*text;++text) {
        // Trắng giữ dấu / nổi bật với mọi màu lịch bão hòa theo giờ.
        const Rgb ink=*text=='/'?Rgb{255,255,255}:color;
        small_glyph(*text,x,24+offset_y,ink,24,PANEL_H);
        x+=advance(*text);
    }
}

void clock_face_seconds(uint32_t minute_ms) {
    if(minute_ms>=60000) return;
    // 30 giây đi tới, 30 giây quay lại; vị trí Q8 giúp nội suy từng pixel.
    constexpr int RADIUS=8;
    const uint32_t phase=minute_ms<=30000?minute_ms:60000-minute_ms;
    const int head=phase*(PANEL_W-1)*256/30000;
    for(int x=0;x<PANEL_W;++x) {
        int distance=x*256-head;
        if(distance<0) distance=-distance;
        if(distance>=RADIUS*256) continue;
        const int linear=(RADIUS*256-distance)*255/(RADIUS*256);
        const int gain=linear*linear/255;
        if(!gain) continue;
        const Rgb c=hue_to_rgb(static_cast<uint8_t>(x*256/(2*RADIUS)+minute_ms*256/60000));
        panel_set_pixel(x,22,c.r*gain/255,c.g*gain/255,c.b*gain/255);
    }
}

void clock_face_status(bool connecting) {
    clock_face_draw(connecting?"CONNECTING":"SETTING",12,1,{80,190,255});
}
