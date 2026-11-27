#include "gfx/clock_face.h"
#include "hal/panel.h"
#include "config/app_config.h"
#include <cstring>
static const char CODES[]="0123456789:/. -ACFGILNOTWY";
static const uint8_t GLYPHS[][5]={
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
 {0,2,0,2,0},{1,1,2,4,4},{0,0,0,0,2},{0,0,0,0,0},{0,0,7,0,0},
 {2,5,7,5,5},{7,4,4,4,7},{7,4,6,4,4},{7,4,5,5,7},{7,2,2,2,7},
 {4,4,4,4,7},{5,7,7,7,5},{7,5,5,5,7},{7,2,2,2,2},{5,5,7,7,5},{5,5,2,2,2}
};
static_assert(sizeof(GLYPHS)/sizeof(GLYPHS[0])==sizeof(CODES)-1);
int clock_face_width(const char *text,int scale){return text[0]?(strlen(text)*4-1)*scale:0;}
void clock_face_draw(const char *text,int y,int scale,Rgb color){
 int x=(PANEL_W-clock_face_width(text,scale))/2;
 for(;*text;++text,x+=4*scale){const char *p=strchr(CODES,*text);if(!p)continue;const auto &rows=GLYPHS[p-CODES];
  for(int row=0;row<5;++row)for(int col=0;col<3;++col)if(rows[row]&(4>>col))
   for(int sy=0;sy<scale;++sy)for(int sx=0;sx<scale;++sx)
    panel_set_pixel(x+col*scale+sx,y+row*scale+sy,color.r,color.g,color.b);
 }
}
