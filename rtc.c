#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <inttypes.h>

#include "util.h"
#include "rtc.h"

#define DDRRTCSEL    DDRB
#define PORTRTCSEL   PORTB

#define DDRSPI      DDRB
#define PORTSPI     PORTB

#define RTCSEL      6
#define MISO        4
#define MOSI        3
#define SCK         5

static void spi_wait() {
    while (!(SPSR & _BV(SPIF)));
}

void rtc_init() {
    //DDRD |= _BV(4);     // RTCSEL#
    DDRRTCSEL |= _BV(RTCSEL);
    
    DDRSPI |= BV2(MOSI,SCK);
    DDRB &= ~_BV(MISO);
    
    PORTRTCSEL |= _BV(RTCSEL);
    
    SPCR = BV4(SPE, MSTR, CPHA, SPR1);
}

void rtc_send(uint8_t b) {
    PORTRTCSEL &= ~_BV(RTCSEL); 
    SPDR = b;
    spi_wait();
}

void rtc_over() {
    PORTRTCSEL |= _BV(RTCSEL);
}

uint8_t rtc_rw(uint8_t addr, int8_t value) {
    rtc_send(addr | (value == -1 ? 0 : 0200));
    rtc_send(value);
    rtc_over();
    return SPDR;
}

uint16_t rtc_gettime(uint8_t ss) {
    uint16_t time = 0;

    // address 0 for seconds, minutes; minutes, hours otherwise   
    rtc_send(ss ? 0 : 1);

    // data 1
    rtc_send(0);
    time = SPDR; 
    
    // data 2
    rtc_send(0);
    while (!(SPSR & _BV(SPIF)));
    time |= SPDR<<8;
    
    rtc_over();
          
    return time; 
}

void rtc_dump() {
    uint8_t i;
    
    rtc_send(0); 
    for (i = 0; i < 0x1a; i++) {
        rtc_send(0); 
        printf_P(PSTR("%02x:%02x   "), i, SPDR);
    }
    rtc_over();
}



