#ifndef _RTC_H
#define _RTC_H

typedef struct _rtc_time {
    uint8_t hour;
    uint8_t minute;
    uint8_t year;
    uint8_t month;
    uint8_t day;
} RTC_TIME;

#define rtc_xseconds(x) rtc_rw(0,x);

#define rtc_xminute(x) rtc_rw(1,x)

#define rtc_xhour(x) rtc_rw(2,x)

#define rtc_xyear(x) rtc_rw(6,x)

#define rtc_xmonth(x) rtc_rw(5,x)

#define rtc_xday(x) rtc_rw(4,x)

#define rtc_xdow(x) rtc_rw(3,x)

void rtc_init();
void rtc_send(uint8_t b);
void rtc_over();
uint16_t rtc_gettime(uint8_t);
uint8_t rtc_rw(uint8_t addr, int8_t value);

void rtc_dump();


#endif
