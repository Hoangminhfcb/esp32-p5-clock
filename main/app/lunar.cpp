#include "app/lunar.h"
#include <cmath>

// Tính sóc và tiết khí theo các chuỗi thiên văn Meeus (lịch hiện đại).
static constexpr double PI = 3.14159265358979323846;
static double sine(double degrees) { return sin(degrees * PI / 180); }
static int jd(int d, int m, int y) {
    int a = (14-m)/12; y += 4800-a; m += 12*a-3;
    return d+(153*m+2)/5+365*y+y/4-y/100+y/400-32045;
}
static int moon(int k) {
    double t=k/1236.85, t2=t*t, t3=t2*t;
    double j=2415020.75933+29.53058868*k+0.0001178*t2-0.000000155*t3;
    j+=0.00033*sine(166.56+132.87*t-0.009173*t2);
    double m=359.2242+29.10535608*k-0.0000333*t2-0.00000347*t3;
    double p=306.0253+385.81691806*k+0.0107306*t2+0.00001236*t3;
    double f=21.2964+390.67050646*k-0.0016528*t2-0.00000239*t3;
    double c=(0.1734-0.000393*t)*sine(m)+0.0021*sine(2*m)-0.4068*sine(p)
        +0.0161*sine(2*p)-0.0004*sine(3*p)+0.0104*sine(2*f)
        -0.0051*sine(m+p)-0.0074*sine(m-p)+0.0004*sine(2*f+m)
        -0.0004*sine(2*f-m)-0.0006*sine(2*f+p)+0.001*sine(2*f-p)+0.0005*sine(2*p+m);
    double dt=t < -11 ? 0.001+0.000839*t+0.0002261*t2-0.00000845*t3-0.000000081*t*t3
                      : -0.000278+0.000265*t+0.000262*t2;
    return (int)floor(j+c-dt+0.5+7.0/24);
}
static int sector(int day) {
    double t=(day-2451545.5-7.0/24)/36525, t2=t*t;
    double m=357.52910+35999.05030*t-0.0001559*t2-0.00000048*t*t2;
    double l=280.46645+36000.76983*t+0.0003032*t2
        +(1.914600-0.004817*t-0.000014*t2)*sine(m)
        +(0.019993-0.000101*t)*sine(2*m)+0.000290*sine(3*m);
    return (int)floor((l/360-floor(l/360))*12);
}
static int month11(int y) {
    int k=(int)floor((jd(31,12,y)-2415021.0)/29.530588853);
    int n=moon(k); return sector(n)>=9 ? moon(k-1) : n;
}
LunarDate lunar_date(int d,int m,int y) {
    int day=jd(d,m,y), k=(int)floor((day-2415021.076998695)/29.530588853);
    // Ước lượng k có thể lệch một sóc ở xa mốc 1900; tìm tháng chứa ngày.
    while(moon(k)>day) --k;
    while(moon(k+1)<=day) ++k;
    int start=moon(k);
    int a=month11(y), b=a, ly=y;
    if(a>=start) { ly=y; a=month11(y-1); } else { ly=y+1; b=month11(y+1); }
    int diff=(int)floor((start-a)/29.0), lm=diff+11; bool leap=false;
    if(b-a>365) {
        int base=(int)floor((a-2415021.076998695)/29.530588853+0.5);
        int i=1, prev=sector(moon(base+1));
        do { ++i; int next=sector(moon(base+i)); if(next==prev) break; prev=next; } while(i<14);
        int offset=i-1;
        if(diff>=offset) { lm=diff+10; leap=diff==offset; }
    }
    if(lm>12) lm-=12;
    if(lm>=11 && diff<4) --ly;
    return {day-start+1,lm,ly,leap};
}
