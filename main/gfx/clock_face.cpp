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
void clock_face_time(int hour,int minute,bool colon,Rgb color) {
    static const uint8_t MASKS[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    const int digits[]={hour/10,hour%10,minute/10,minute%10};
    const int xs[]={4,18,35,49};
    // Bảy đoạn chuẩn, ô 12x17, nét 2 px với đầu đoạn vát.
    for(int i=0;i<4;++i) {
        const uint8_t mask=MASKS[digits[i]];
        const int horizontal_bits[]={0,6,3}, ys[]={2,9,17};
        for(int segment=0;segment<3;++segment) if(mask&(1<<horizontal_bits[segment])) {
            for(int dy=0;dy<2;++dy) for(int dx=0;dx<8;++dx) {
                if((dx==0&&dy==0)||(dx==7&&dy==1)) continue;
                panel_set_pixel(xs[i]+2+dx,ys[segment]+dy,color.r,color.g,color.b);
            }
        }
        const int vertical_bits[]={5,1,4,2}, vx[]={0,10,0,10}, vy[]={4,4,11,11};
        for(int segment=0;segment<4;++segment) if(mask&(1<<vertical_bits[segment])) {
            for(int dy=0;dy<5;++dy) for(int dx=0;dx<2;++dx) {
                if((dy==0&&dx==0)||(dy==4&&dx==1)) continue;
                panel_set_pixel(xs[i]+vx[segment]+dx,vy[segment]+dy,color.r,color.g,color.b);
            }
        }
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
        small_glyph(*text,x,22+offset_y,ink,22,PANEL_H-2);
        x+=advance(*text);
    }
}

void clock_face_seconds(uint32_t minute_ms) {
    if(minute_ms>=60000) return;
    constexpr int WIDTH=PANEL_W-1,HEIGHT=PANEL_H-1;
    constexpr int PERIMETER=2*(WIDTH+HEIGHT),TAIL=24;
    const uint32_t head=minute_ms*PERIMETER*256/60000;
    const int index=head/256,fraction=head%256;
    auto sample=[&](int distance) -> Rgb {
        if(distance>=TAIL) return {0,0,0};
        const int gain=(TAIL-distance)*(TAIL-distance)*255/(TAIL*TAIL);
        const Rgb c=hue_to_rgb(static_cast<uint8_t>(distance*256/TAIL+minute_ms*256/60000));
        return {static_cast<uint8_t>(c.r*gain/255),static_cast<uint8_t>(c.g*gain/255),static_cast<uint8_t>(c.b*gain/255)};
    };
    for(int i=0;i<PERIMETER;++i) {
        const int distance=(index-i+PERIMETER)%PERIMETER;
        const Rgb a=sample(distance),b=sample((distance+1)%PERIMETER);
        const Rgb color={static_cast<uint8_t>((a.r*(256-fraction)+b.r*fraction)/256),
                         static_cast<uint8_t>((a.g*(256-fraction)+b.g*fraction)/256),
                         static_cast<uint8_t>((a.b*(256-fraction)+b.b*fraction)/256)};
        if(!(color.r||color.g||color.b)) continue;
        // Bắt đầu giữa cạnh trên; chạy trên toàn bộ mép 64x32.
        const int position=(i+PANEL_W/2)%PERIMETER;
        int x,y;
        if(position<WIDTH) {x=position;y=0;}
        else if(position<WIDTH+HEIGHT) {x=WIDTH;y=position-WIDTH;}
        else if(position<2*WIDTH+HEIGHT) {x=WIDTH-(position-WIDTH-HEIGHT);y=HEIGHT;}
        else {x=0;y=HEIGHT-(position-2*WIDTH-HEIGHT);}
        panel_set_pixel(x,y,color.r,color.g,color.b);
    }
}

void clock_face_status(bool connecting) {
    clock_face_draw(connecting?"CONNECTING":"SETTING",12,1,{80,190,255});
}
