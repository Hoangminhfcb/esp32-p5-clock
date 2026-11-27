#include "app/lunar.h"
#include <cassert>
#include <cstdio>
int main() {
 struct Case {int d,m,y,ld,lm,ly;bool leap;};
 Case cases[]={ {10,2,2024,1,1,2024,false},{29,1,2025,1,1,2025,false},{17,2,2026,1,1,2026,false},{6,2,2027,1,1,2027,false},{22,3,2023,1,2,2023,true},{19,4,2023,29,2,2023,true},{20,4,2023,1,3,2023,false},{31,12,2024,1,12,2024,false},{1,1,2025,2,12,2024,false} };
 for(auto c:cases){auto l=lunar_date(c.d,c.m,c.y);assert(l.day==c.ld&&l.month==c.lm&&l.year==c.ly&&l.leap==c.leap);}
 LunarDate prev={}; int count=0;
 for(int y=2000;y<=2099;++y) for(int m=1;m<=12;++m){
  const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  int limit=days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));
  for(int d=1;d<=limit;++d){auto l=lunar_date(d,m,y);
   assert(l.day>=1&&l.day<=30&&l.month>=1&&l.month<=12);
   if(count){if(l.day==1)assert(prev.day==29||prev.day==30);else assert(l.day==prev.day+1&&l.month==prev.month&&l.year==prev.year&&l.leap==prev.leap);}
   prev=l;++count;
  }
 }
 printf("Lunar tests passed: 9 reference dates, %d consecutive days\n",count);
}
