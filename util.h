/// \file
/// \brief Utilities
///
/// Bit settings. BCD conversions. Calendar.
///
#ifndef _UTIL_H
#define _UTIL_H

#define PORTDIGIT PORTC
#define DDRDIGIT  DDRC

#define PORTBUTTON PORTC
#define DDRBUTTON  DDRC

#define PORTSA234  PORTD
#define DDRSA234   DDRD
#define PORTSA1    PORTB
#define DDRSA1     DDRB

#define PORTHVPUMP PORTB
#define DDRHVPUMP  DDRB


#define BV2(a,b) (_BV(a)|_BV(b))
#define BV3(a,b,c) (_BV(a)|_BV(b)|_BV(c))
#define BV4(a,b,c,d) (_BV(a)|_BV(b)|_BV(c)|_BV(d))
#define BV5(a,b,c,d,e) (_BV(a)|_BV(b)|_BV(c)|_BV(d)|_BV(e))
#define BV6(a,b,c,d,e,f) (_BV(a)|_BV(b)|_BV(c)|_BV(d)|_BV(e)|_BV(f))
#define BV7(a,b,c,d,e,f,g) (_BV(a)|_BV(b)|_BV(c)|_BV(d)|_BV(e)|_BV(f)|_BV(g))

// Select Anode constants
enum _sa_values {
    SA1 = 0,
    SA2, 
    SA3,
    SA4,
    SAX = 0377
};

/// Blinking modes, see timer0 overflow interrupt
enum _blinkmode {
    BLINK_NONE = 0,
    BLINK_HH = 1,
    BLINK_MM = 2,
    BLINK_ALL = 3,
};

#define NDISPLAYMODES 3
enum _displaymode {
    HHMM = 0,
    MMSS,
    VOLTAGE
};


/// Make BCD time from 2 bytes
#define maketime(hh,mm) (((hh) << 8) + (mm))

/// Convert to binary from BCD representation 
/// \see frombcd
#define _frombcd(x) ((x & 017) + (((x) & 0160)>>4) * 10)

/// Convert to binary from BCD representation as a function.
/// \see _frombcd
uint8_t frombcd(uint8_t);

uint16_t tobcd16(uint16_t);

/// Return BCD count of days in month for given BCD year and month.
/// Valid only for years 2000-2099.
uint8_t days_in_month_bcd(uint8_t year, uint8_t month);

/// Increment BCD value by 1
uint8_t bcd_increment(uint8_t x);

/// Get day of week, 0 = Sunday. Input parameters are binary (not BCD)
uint8_t day_of_week(uint8_t y, uint8_t m, uint8_t d);

/// Set blinkmode
void set_blinkmode(uint8_t mode);

/// Called every blink, unless NULL
void (*blinkhandler)(uint8_t);

void duty_set(uint8_t d);

uint8_t duty_get();

void fadeto(uint16_t t);

uint16_t get_display_value();

void mode_next();


#endif
