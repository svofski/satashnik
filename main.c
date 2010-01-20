///\file main.c
///\author Viacheslav Slavinsky
///
///\brief Satashnik
///
/// \mainpage Satashnik: nixie clock with AVR-driven boost converter and DS3234 RTC.
/// \section Files
/// - main.c    main file
/// - rtc.c     RTC-related stuff
/// - voltage.c Everything related to boost converter control
/// - util.c    Calendar and other utils
///

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <stdio.h>
#include <stdlib.h>

#include <util/delay.h>

#include "usrat.h"
#include "rtc.h"
#include "util.h"
#include "voltage.h"
#include "buttonry.h"

volatile uint16_t time = 0;         //!< current display value
volatile uint16_t timef = 0;        //!< fadeto display value

volatile uint8_t digitmux = 0;              //!< displayed digit, 0..3
volatile uint16_t digitsraw = 0;            //!< raw port values
volatile uint16_t rawfadefrom = 0177777;    //!< digitsraw fade from
volatile uint16_t rawfadeto = 0177777;      //!< digitsraw fade to

uint16_t ocr1a_reload = 121;

volatile uint8_t blinktick = 0;     //!< 1 when a pressed button is autorepeated

volatile uint8_t display_mode = HHMM;   //!< current display mode. \see _displaymode


volatile uint8_t blinkmode;     //!< current blinking mode
volatile uint16_t blinkctr;     //!< blinkmode counter
volatile uint8_t blinkduty;     //!< blinkmode duty 

volatile uint8_t fadeduty, fadectr; //!< crossfade counters
volatile int16_t fadetime;      //!< crossfade time and trigger, write "-1" to start fade to timef
volatile uint8_t fademode;      //!< \see _fademode

volatile uint8_t savingmode;    //!< nixe preservation mode


#define FADETIME    1024        //<! Transition time for xfading digits, in tmr0 overflow-counts

#define FADETIME_S  3000        //<! Slow transition time

#define BLINKTIME   4096        //<! Blink time in tmr0-overflow-counts
#define BLINKBIT    10          //<! Flag bit, used internally by the blinker

#define TIMERCOUNT  15          //<! Timer reloads with 256-TIMERCOUNT

volatile uint16_t fadetime_full = FADETIME;
volatile uint16_t fadetime_quart= FADETIME/4;

void set_fadespeed(uint8_t mode) {
    switch (mode) {
        case FADE_ON:
            fadetime_full = FADETIME;
            fadetime_quart = FADETIME/4;
            fademode = FADE_ON;
            break;
        case FADE_OFF:
            fademode = FADE_OFF;
            break;
        case FADE_SLOW:
            fadetime_full = FADETIME_S;
            fadetime_quart = FADETIME_S/4;
            fademode = FADE_ON;
            break;
    }
}


inline void blinkmode_set(uint8_t mode) {
    blinkmode = mode;
    blinkctr = 0;
}

inline uint8_t blinkmode_get() { return blinkmode; }

void savingmode_set(uint8_t s) {
    savingmode = s; 
    switch (savingmode) {
        case SAVE:
            voltage_setpoint = VOLTAGE_SAVE;
            break;
        case WASTE:
            voltage_setpoint = VOLTAGE_WASTE;
            break;
        default:
            break;
    }
}
inline uint8_t savingmode_get() { return savingmode; }


/// Init display-related DDRs.
void initdisplay() {
    DDRDIGIT |= BV4(0,1,2,3);
    DDRSA1 |= _BV(0);
    DDRSA234 |= BV3(5,6,7);
}

/// Rehash bits to match the schematic
inline uint8_t swapbits(uint8_t x) {
    return ((x & 1) << 3) | (x & 2) | ((x & 4) >> 2) | ((x & 8) >> 1);
}

/// get raw digits from hh:mm
inline uint16_t getrawdigits(uint8_t h1, uint8_t h2, uint8_t m1, uint8_t m2) {
    return (swapbits(h1)<<12) | (swapbits(h2)<<8) | (swapbits(m1)<<4) | swapbits(m2);
}

/// get raw digits from a BCD value
uint16_t getrawdigits_bcd(uint16_t time) {
    return getrawdigits((time & 0xf000)>>12, (time & 0x0f00)>>8, (time & 0x00f0)>>4, time & 0x000f);
}

/// Output the current digit code to ID1
uint8_t display_currentdigit(uint8_t n) {
    uint8_t dispbit = 0x0f;
    
    if (n < 4) {
        dispbit = 0x0f & (digitsraw >> (n<<2));
        PORTDIGIT = (PORTDIGIT & ~BV4(0,1,2,3)) | dispbit;
    } else {
        PORTDIGIT |= 0x0f;
    }
    
    return dispbit != 0x0f;
}

/// Shut off previous anode, wait to prevent ghosting,
/// output new digit code to ID1 and enable new anode
void display_selectdigit(uint8_t n) {
    switch (n) {
        case SA1: 
                PORTSA234 &= ~BV3(5,6,7);
                _delay_ms(0.02);
                if (display_currentdigit(n)) {
                    PORTSA1 |= _BV(0);
                }
                break;
        case SA2:
        case SA3:
        case SA4:
                PORTSA1 &= ~_BV(0);
                PORTSA234 &= ~BV3(5,6,7);
                _delay_ms(0.02);
                if (display_currentdigit(n)) {
                    PORTSA234 |= 0200 >> (n-1);
                }
                break;
        default:
                PORTSA234 &= ~BV3(5,6,7);
                PORTSA1 &= ~_BV(0);
                break;
    }
}

/// Start fading time to given value. 
/// Transition is performed in TIMER0_OVF_vect and takes FADETIME cycles.
void fadeto(uint16_t t) { 
    uint16_t raw = getrawdigits_bcd(t); // takes time
    cli();
    timef = t; 
    rawfadeto = raw;
    fadetime = -1;
    sei();
}

inline uint16_t get_display_value() {
    return timef;
}

void mode_next() {
    display_mode = (display_mode + 1) % NDISPLAYMODES;
    switch (display_mode) {
        case HHMM:  set_fadespeed(FADE_SLOW);
                    break;
        case MMSS:  set_fadespeed(FADE_ON);
                    break;
        case VOLTAGE: set_fadespeed(FADE_OFF);
                    break;
    }
}

uint8_t mode_get() {
    return display_mode;
}

/// Start timer 0. Timer0 runs at 1MHz and overflows at 3906 Hz.
void timer0_init() {
    TIMSK |= _BV(TOIE0);    // enable Timer0 overflow interrupt
    TCNT0 = 256-TIMERCOUNT;
    TCCR0 = BV2(CS01,CS00);   // clk/64 = 125000Hz, full overflow rate 488Hz
}


ISR(TIMER0_OVF_vect) {
    uint16_t toDisplay = time;
    static uint8_t odd = 0;
    
    odd += 1;
    
    // Reload the timer
    TCNT0 = 256-TIMERCOUNT;
    
    // In blink modes: increment the counter and activate "blinktick" for button autorepeat
    if (blinkmode != BLINK_NONE) {
        blinkctr = (blinkctr + 1) % BLINKTIME;
        if (blinkctr == BLINKTIME/4) {
            blinktick = 1;
        }
    } 
    
    if (fadetime == -1) {
        if (fademode == FADE_OFF) {
            fadeduty = 1;
            fadetime = 1;
        } else {
            // start teh fade
            fadetime = fadetime_full;
            fadeduty = 4;
            fadectr = 0;
        }
    }
    
    if (fadetime != 0) {
        fadetime--;

        if (fadetime % fadetime_quart == 0) {
            fadeduty--;
        }
        
        if (fadetime == 0) {
            fadectr = 0;
            time = timef; // end fade
            rawfadefrom = rawfadeto;
        }
    } 
    

    if (savingmode && (fadectr>>3) < 2) {
        toDisplay = 0xffff;
    } else if ((fadectr>>3) < fadeduty) {
        toDisplay = rawfadefrom;
    } 
    else {
        toDisplay = rawfadeto;
    }
    fadectr = (fadectr + 1) & 037;

    if (blinkmode != BLINK_NONE && (blinkmode & 0200) == 0 && blinkctr > BLINKTIME/2) {
        switch (blinkmode) {
            case BLINK_HH:
                toDisplay |= 0xff00;
                break;
            case BLINK_MM:
                toDisplay |= 0x00ff;
                break;
            case BLINK_ALL:
                toDisplay |= 0xffff;
                break;
            default:
                break;
        }
    }

    digitsraw = toDisplay;
    
    if (odd & 1) {
        display_selectdigit(digitmux);
        digitmux = (digitmux + 1) & 3;
    } else {
        switch (PORTDIGIT & 017) {
            case 0x0a: // 3 stands out too much
                display_selectdigit(0377);
                break;
        }
    }
}


/// Update DST: 
/// 02:00->03:00 on the last sunday of March
/// 03:00->02:00 on the last sunday of October
/// 
/// And try to do this only once..
void update_daylight(uint16_t time) {
    static uint8_t daylight_adjusted = 0;

    if (time == 0x0000) daylight_adjusted = 0;
    
    if (daylight_adjusted) return;
    
    if (time == 0x0200 || time == 0x0300) {
        if (rtc_xdow(-1) == 0) {
            switch (rtc_xmonth(-1)) {
                case 3:
                    if (rtc_xday(-1) > 0x24) {
                        // last sunday of march
                        if (time == 0x0200) {
                            rtc_xhour(3);
                            daylight_adjusted = 1;
                        }
                    } else {
                        daylight_adjusted = 1;
                    }
                    break;
                case 10:
                    if (rtc_xday(-1) > 0x24) {
                        // last sunday of october
                        if (time == 0x0300) {
                            rtc_xhour(2);
                            daylight_adjusted = 1;
                        }
                    } else {
                        daylight_adjusted = 1;
                    }  
                    break;
                default:
                    daylight_adjusted = 1; 
                    break;
            }
        }
    }
}

/// Program main
int main() {
    uint8_t i;
    uint16_t rtime;
    uint8_t byte;
    volatile uint16_t skip = 0;
    uint8_t uart_enabled = 0;
    
    pump_nomoar();
    
    usart_init(F_CPU/16/19200-1);
    
    printf_P(PSTR("\033[2J\033[HB%s WHAT DO YOU MEAN? %02x\n"), BUILDNUM, MCUCSR);

    sei();

    pump_init();
    
    adc_init();
    
    
    initdisplay();
    
    rtc_init();
    
    buttons_init();
 
    set_fadespeed(FADE_ON);
    
    rtime = time = timef = 0xffff;   

    fadeto(0x1838);
    
    timer0_init();

    _delay_ms(500);

    set_fadespeed(FADE_SLOW);    
    fadeto(0xffff);
    
    _delay_ms(500);
    
    wdt_enable(WDTO_250MS);
    
    for(i = 0;;i++) {
        wdt_reset();
        
        // handle keyboard commands
        if (uart_available()) {
            byte = uart_getchar();
            switch (uart_enabled) {
                case 0: if (byte == 'z') 
                            uart_enabled = 1;
                        else
                            uart_enabled = 0;
                        break;
                case 1: if (byte == 'c') 
                            uart_enabled = 2;
                        else
                            uart_enabled = 0;
                        break;
                case 2:
                        switch (byte) { 
            			case 'q':   pump_faster(-1);
                				    break;
            			case 'w':   pump_faster(+1);
                				    break;
            			case 'Q':   ocr1a_reload--;
                				    break;
            			case 'W':   ocr1a_reload++;
                				    break;
                        case '`':   pump_nomoar();
                                    break;
                        case 'r':   rtc_dump();
                                    break;
                        case 't':   printf_P(PSTR("time=%04x\n"), time);
                                    break;
                        case 'v':   voltage_setpoint++;
                                    break;
                        case 'V':   voltage_setpoint--;
                                    break;
                        case 'n':   rtime = 0x1838;
                                    fadeto(rtime);
                                    skip = 32768;
                                    break;
                        case 'b':   blinkmode = (blinkmode + 1) % 4;
                                    blinkduty = 0;
                                    blinkctr = BLINKTIME;
                                    break;
                        case '.':   break;
                        case '=':   // die
                                    for(;;);
                                    break;
                        default:
                                    break;
                        }
            
                        if (byte >= '0' && byte <= '9') {
                            byte = byte - '0';
                            fadeto((byte<<12)+(byte<<8)+(byte<<4)+byte);
                            skip = 255;
                        }
                        
                        printf_P(PSTR("OCR1A=%d ICR1=%d S=%d V=%d, Time=%04x, voltagebcd=%04x\n"), OCR1A, ICR1, voltage_setpoint, voltage, time, voltage_getbcd());
                        break;
            }
            
        }
        
        display_selectdigit(digitmux);
        digitmux = (digitmux + 1) & 3;

        buttonry_tick();
    
        if (blinktick) {        
            blinktick = 0;
            if (blinkhandler != NULL) {
                blinkhandler(1);
            }
        }
        
        if (skip != 0) {
            skip--;
        } else {
            rtime = rtc_gettime(0);
            update_daylight(rtime);
            
            switch (mode_get()) {
                case HHMM:
                    rtime = rtc_gettime(0);
                    break;
                case MMSS:
                    rtime = rtc_gettime(1);
                    break;
                case VOLTAGE:
                    rtime = voltage_getbcd();
                    //skip = 64;
                    break;
            }
            
            //cli();
            if (!is_setting() && rtime != time && rtime != timef) {
                fadeto(rtime);
            }     
            //sei();       
        }

        _delay_ms(80);
    }
}

