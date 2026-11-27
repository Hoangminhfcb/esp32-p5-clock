#include "gfx/clock_face.h"
#include <cassert>
#include <cstdio>
static int y_min,y_max;
void panel_set_pixel(int x,int y,uint8_t,uint8_t,uint8_t){assert(x>=0&&x<64&&y>=y_min&&y<y_max);}
static void fit(const char* s,int scale,int y,int height){assert(clock_face_width(s,scale)<=64);y_min=y;y_max=y+height;clock_face_draw(s,y,scale,{255,255,255});}
int main(){char s[40];for(int h=0;h<24;++h)for(int m=0;m<60;++m){snprintf(s,sizeof(s),"%02d:%02d",h,m);fit(s,3,0,18);}for(int d=1;d<=31;++d)for(int m=1;m<=12;++m){snprintf(s,sizeof(s),"T7 %02d/%02d",d,m);fit(s,1,18,8);snprintf(s,sizeof(s),"AL%02d/%02dN",d,m);fit(s,1,26,6);}fit("2026 AL2026",1,26,6);fit("192.168.4.1",1,26,6);puts("Clock layout tests passed");}
