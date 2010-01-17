#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <stdio.h>

#include "buttonry.h"
#include "util.h"
#include "rtc.h"

enum _setstates {
    SET_NONE = 0,
    SET_HOUR,
    SET_MINUTE,
    SET_SEC0,
    SET_YEAR,
    SET_MONTH,
    SET_DAY,
};


/// buttons 1 and 2 states for debouncing
static uint8_t button1_state, button2_state;

static uint8_t set_state;       //<! setup state, see enum _setstates 

uint8_t reset_seconds = 0;      //<! a flag that tells to reset seconds to zero after set button is pressed


RTC_TIME rtc_time;              //<! current time values, used only during setup 


/// Catch button press and release moments, call handler
void debounce(uint8_t port, uint8_t* state, void (*handler)(uint8_t)) {
    if (*state && !port) handler(1);
    if (!*state && port) handler(0);
    *state = port;
}

/// Initialize ports and variables
void buttons_init() {
    DDRD &= ~BV2(3,4);
    PORTD |= BV2(3,4);
    button1_state = button2_state = 0;
}


#define set_blinkhandler(x) { cli();\
                              if (blinkhandler == NULL) {\
                                blinkhandler = x;\
                                skip = 1;\
                              }\
                              sei();\
                            }

/// Handler for button 1: "SET"
void button1_handler(uint8_t on) {
    static uint8_t skip = 0;
    
    if (on) {
        if (skip > 0) {
            skip--;
            return;
        }
        
        switch (set_state) {
            case SET_NONE:
                duty_set(duty_get() % 4 + 1);
                skip = 1;
                break;
                
            case SET_HOUR:
                set_blinkhandler(button1_handler);
                
                rtc_time.hour = bcd_increment(rtc_time.hour);

                if (rtc_time.hour == 0x24) rtc_time.hour = 0;
                
                rtc_xhour(rtc_time.hour);
                
                fadeto((get_display_value() & 0377) | (rtc_time.hour << 8));
                break;
                
            case SET_MINUTE:
                set_blinkhandler(button1_handler);
                
                rtc_time.minute = bcd_increment(rtc_time.minute);
                if (rtc_time.minute == 0x60) rtc_time.minute = 0;
                rtc_xminute(rtc_time.minute);
                rtc_xseconds(0);
                reset_seconds = 1;
                
                fadeto((get_display_value() & 0xff00) | rtc_time.minute);
                break;
                
            case SET_YEAR:
                set_blinkhandler(button1_handler);
                
                rtc_time.year = bcd_increment(rtc_time.year);
                rtc_xyear(rtc_time.year);

                rtc_xdow(day_of_week(frombcd(rtc_time.year), frombcd(rtc_time.month), frombcd(rtc_time.day)));

                fadeto(0x2000 + rtc_time.year);
                break;     
                       
            case SET_MONTH:
                set_blinkhandler(button1_handler);

                rtc_time.month = bcd_increment(rtc_time.month);
                if (rtc_time.month == 0x13) rtc_time.month = 1;                
                rtc_xmonth(rtc_time.month);

                rtc_xdow(day_of_week(frombcd(rtc_time.year), frombcd(rtc_time.month), frombcd(rtc_time.day)));
                
                fadeto(maketime(rtc_time.day, rtc_time.month));
                break;
            
            case SET_DAY:
                set_blinkhandler(button1_handler);
                
                rtc_time.day = bcd_increment(rtc_time.day);
                if (rtc_time.day > days_in_month_bcd(rtc_time.year, rtc_time.month)) {
                    rtc_time.day = 1;
                }
                rtc_xday(rtc_time.day);
                
                rtc_xdow(day_of_week(frombcd(rtc_time.year), frombcd(rtc_time.month), frombcd(rtc_time.day)));
                
                fadeto(maketime(rtc_time.day, rtc_time.month));
                break;
        }
    } else {
        cli(); blinkhandler = NULL; skip = 0; sei();
    }
}

/// Handler for button 2, "+"
void button2_handler(uint8_t on) {
    if (on) {
        switch (set_state) {
            case SET_NONE:
                set_state = SET_HOUR;
                set_blinkmode(BLINK_HH);
                rtc_time.hour = rtc_xhour(-1);
                rtc_time.minute = rtc_xminute(-1); 
                fadeto(maketime(rtc_time.hour, rtc_time.minute));
                break;
            case SET_HOUR:
                set_state = SET_MINUTE;
                set_blinkmode(BLINK_MM);
                break;
            case SET_MINUTE:
                set_state = SET_YEAR;
                set_blinkmode(BLINK_ALL);
                rtc_time.year = rtc_xyear(-1);
                fadeto(0x2000 + rtc_time.year);

                if (reset_seconds) {
                    rtc_xseconds(0);
                    reset_seconds = 0;
                }
                break;
            case SET_YEAR:
                set_state = SET_MONTH;
                set_blinkmode(BLINK_MM);
                
                rtc_time.month = rtc_xmonth(-1);
                if (rtc_time.month == 0) rtc_time.month = 1;
                
                rtc_time.day   = rtc_xday(-1);
                if (rtc_time.day == 0) rtc_time.day = 1;
                
                fadeto(maketime(rtc_time.day, rtc_time.month));
                break;
            case SET_MONTH:
                set_state = SET_DAY;
                set_blinkmode(BLINK_HH);
                break;
            case SET_DAY:
            default:
                set_state = SET_NONE;
                set_blinkmode(BLINK_NONE);
                break;
        }
    } else {
        cli(); blinkhandler = NULL; sei();
    }
}

uint8_t is_setting() {
    return set_state != SET_NONE;
}

void buttonry_tick(uint8_t b1, uint8_t b2) {
    debounce(b1, &button1_state, button1_handler);
    debounce(b2, &button2_state, button2_handler);
}
