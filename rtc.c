#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <inttypes.h>

#include "util.h"
#include "rtc.h"

static void spi_wait() {
    while (!(SPSR & _BV(SPIF)));
}

void rtc_init() {
    DDRD |= _BV(6);     // RTCSEL#
    
    DDRB |= BV2(5,7);
    DDRB &= ~_BV(6);
    
    PORTD |= _BV(6);
    
    SPCR = BV4(SPE, MSTR, CPHA, SPR1);
}

void rtc_send(uint8_t b) {
    PORTD &= ~_BV(6);
    SPDR = b;
    spi_wait();
}

void rtc_over() {
    PORTD |= _BV(6);
}

uint8_t rtc_rw(uint8_t addr, int8_t value) {
    rtc_send(addr | (value == -1 ? 0 : 0200));
    rtc_send(value);
    rtc_over();
    return SPDR;
}

uint16_t rtc_gettime() {
    uint16_t time = 0;

    // address 0    
    rtc_send(1);

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



