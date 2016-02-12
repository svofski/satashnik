#include <inttypes.h>
#include "util.h"

uint8_t frombcd(uint8_t x) {
    return _frombcd(x);
}

uint8_t month_length(uint8_t m, uint8_t leap) {
    uint8_t knuckle = m < 8 ? (m & 1) == 1 : (m & 1) == 0;
    return knuckle ? 31 : m != 2 ? 30 : leap ? 29 : 28;
}

uint8_t days_in_month_bcd(uint8_t year, uint8_t month) {
    uint8_t y = frombcd(year);//(year & 7) + ((year & 070)>>3) * 10;
    uint8_t m = frombcd(month);//(month & 7) + ((month & 070)>>3) * 10;
    uint8_t leap = y % 4 == 0; // non y2k1-compliant, but should be correct for the next 91 years or so :)
    
    uint8_t knuckle = m < 8 ? (m & 1) == 1 : (m & 1) == 0;
    return knuckle ? 0x31 : m != 2 ? 0x30 : leap ? 0x29 : 0x28;
}


uint8_t bcd_increment(uint8_t x) {
	x++;
	if ((x & 0x0f) >= 0x0a)
		x += 0x10 - 0x0a;
	if (x >= 0xa0) 
	   x = 0;
	return x;
}


uint8_t day_of_week(uint8_t y, uint8_t m, uint8_t d) {
    uint8_t leap = y%4 == 0;
    uint16_t centurydays = 6 + y * 365 + (y+3)/4; // year 2000 started on Saturday
    for (; --m >= 1; ) {
        centurydays += month_length(m,leap);
    }
    
    centurydays += d-1;
    
    return (centurydays % 7);
}

uint16_t tobcd16(uint16_t x) {
    uint16_t y;
    
    y  = x % 10;        x /= 10;       
    y |= (x % 10)<<4;   x /= 10;
    y |= (x % 100)<<8;  x /= 10; 
    y |= 0xf000;
    
    return y;
}


