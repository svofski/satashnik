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
#include "modes.h"
#include "cal.h"

volatile uint16_t time = 0;         //!< current display value
volatile uint16_t timef = 0;        //!< fadeto display value

volatile uint8_t digitmux = 0;              //!< displayed digit, 0..3
volatile uint16_t digitsraw = 0;            //!< raw port values
volatile uint16_t rawfadefrom = 0177777;    //!< digitsraw fade from
volatile uint16_t rawfadeto = 0177777;      //!< digitsraw fade to

volatile uint8_t blinktick = 0;     //!< 1 when a pressed button is autorepeated

volatile uint16_t blinkctr;     //!< blinkmode counter
volatile uint8_t blinkduty;     //!< blinkmode duty 

volatile uint8_t fadeduty, fadectr; //!< crossfade counters
volatile int16_t fadetime;      //!< crossfade time and trigger, write "-1" to start fade to timef

volatile uint8_t halfbright;    //!< keep low duty

#define TIMERCOUNT  25          //<! Timer reloads with 256-TIMERCOUNT


/// Values for blinking, calibrated to quarters of a second at startup
uint16_t bcq1;
uint16_t bcq2;
uint16_t bcq3;

void savingmode_keep(uint16_t hhmm) {
    switch (savingmode_get()) {
        case SAVENIGHT:
            if (hhmm > 0x0100 && hhmm < 0x0700) {
                voltage_set(VOLTAGE_SAVE);
                halfbright = 2;                     // darkest
            } else if (hhmm < 0x0800) {
                voltage_set(VOLTAGE_SAVE);
                halfbright = 1;                     // dark
            } else {
                voltage_set(VOLTAGE_WASTE);
                halfbright = 0;                     // normal
            }
            break;
        case SAVE:
            voltage_set(VOLTAGE_SAVE);
            halfbright = 1;
            break;
        case WASTE:
            voltage_set(VOLTAGE_WASTE);
            halfbright = 0;
            break;
    }
}

/// Init display-related DDRs.
void initdisplay() {
    DDRDIGIT |= BV4(0,1,2,3);
    DDRDOT |= _BV(DOT);
    DDRSA1 |= _BV(0);
    DDRSA234 |= BV3(5,6,7);
}

/// Rehash bits to match the schematic
uint8_t swapbits(uint8_t x) {
    return ((x & 1) << 3) | (x & 2) | ((x & 4) >> 2) | ((x & 8) >> 1);
}

/// get raw digits from hh:mm
uint16_t getrawdigits(uint8_t h1, uint8_t h2, uint8_t m1, uint8_t m2) {
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
                _delay_ms(0.01); //ghosting prevention
                if (display_currentdigit(n)) {
                    PORTSA1 |= _BV(0);
                }
                break;
        case SA2:
        case SA3:
        case SA4:
                PORTSA1 &= ~_BV(0);
                PORTSA234 &= ~BV3(5,6,7);
                _delay_ms(0.01); //ghosting prevention
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

/// Return current BCD display value 
inline uint16_t get_display_value() {
    return timef;
}

/// Start timer 0. Timer0 runs at 1MHz
/// The speed is dictated by the need to keep the neon dot ionized at all times
void timer0_init() {
    TIMSK |= _BV(TOIE0);    // enable Timer0 overflow interrupt
    TCNT0 = 256-TIMERCOUNT;
    TCCR0 = _BV(CS01);
}

ISR(TIMER0_OVF_vect) {
    uint16_t toDisplay = time;
    static uint8_t odd = 0;
    
    TCNT0 = 256-TIMERCOUNT;

    odd += 1;

    // Handle the dot, which must be sustained at all times
    // High-frequency short-duty seems to be an acceptable way
    // of keeping the gas ionized, yet practically invisible
    if ((odd & 7) < (( (dotmode == DOT_OFF || blinkctr>bcq2) && !(dotmode == DOT_ON)) ? 0:1) 
        || ((dotmode == DOT_BLINK) && ((blinkctr <= 4) || ((odd & 0x7f) == 0)))) {
        PORTDOT |= _BV(DOT);
    } else {
        PORTDOT &= ~_BV(DOT);
    }
    
    // Signal the main loop to continue rolling
    if ((odd & 0x3f) == 0) {
        blinktick |= _BV(2);
    }
    
    // A "slow" cycle every 32 fast cycles, display business
    if ((odd & 0x1f) == 0) {
        // keep blinkctr for things that happen on 1/4ths of a second
        blinkctr++;
        if (blinkctr > (bcq2<<1)) {
            blinkctr = 0;
        }
        
        // signal the main loop to autorepeat buttons when needed
        if (blinkmode_get() != BLINK_NONE) {
            if (blinkctr == bcq1 || blinkctr == bcq2 || blinkctr == bcq3 || blinkctr == 1) {
                blinktick |= _BV(1);
            }
        }
        
        // fadetime == -1 indicates start of fade
        if (fadetime == -1) {
            if (fade_get() == FADE_OFF) {
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
        
        if ((fadectr>>3) < fadeduty) {
            toDisplay = rawfadefrom;
        } else {
            toDisplay = rawfadeto;
        }
        fadectr = (fadectr + 1) & 037;
    
        // blinking (blinkmode & 0200 temporarily disables blinking)
        if (blinkmode_get() != BLINK_NONE && (blinkmode_get() & 0200) == 0 && blinkctr > bcq2) {
            switch (blinkmode_get()) {
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
    }
    
    // every other "slow" cycle, select the next digit...
    if ((odd & 0x1f) == 0) {
        display_selectdigit(digitmux);
        digitmux = (digitmux + 1) & 3;
    } else {
        // switch bright digits earlier
        if ((odd & 0x1f) == 0x18) {
            switch (PORTDIGIT & 017) {
                case 0x0a: // "3"
                    display_selectdigit(0377);
                    break;
            }
        }
        // shorten duty cycle for cathode-preserving modes
        if (halfbright == 2 && (odd & 0x1f) == 0x8) {
            display_selectdigit(0377);
        }
        if (halfbright == 1 && (odd & 0x1f) == 0x10) {
            display_selectdigit(0377);
        }
    }
}

/// Calibrate blink counters to quarters of second
void calibrate_blinking() {
    bcq1 = bcq2 = bcq3 = 65535;
    for(bcq1 = rtc_gettime(1); bcq1 == rtc_gettime(1););
    blinkctr = 0; 
    for(bcq1 = rtc_gettime(1); bcq1 == rtc_gettime(1););
    cli();
    bcq1 = blinkctr/4;
    bcq2 = 2*blinkctr/4;
    bcq3 = 3*blinkctr/4;
    sei();
}

/// Program main
int main() {
    uint8_t i;
    uint16_t rtime;
    uint8_t byte;
    volatile uint16_t skip = 0;
    uint8_t uart_enabled = 0;
    volatile uint16_t mmss, mmss1;

    OSCCAL = 0xA6;

    pump_nomoar();
    
    usart_init(F_CPU/16/19200-1);
    
    //for(OSCCAL=0;;) {
    //printf_P(PSTR("OSCCAL=%x  \n"), OSCCAL);
    //OSCCAL++;
    //}

    
    printf_P(PSTR("\033[2J\033[HB%s WHAT DO YOU MEAN? %02x\n"), BUILDNUM, MCUCSR);

    sei();

    voltage_start();        // start HV generation    
    initdisplay();
    dotmode_set(DOT_OFF);
    rtc_init();
    buttons_init();

    // display greeting
    fade_set(FADE_SLOW);
    rtime = time = timef = 0xffff;   
    timer0_init();
    fadeto(0x1838);

    // calibrate blink quarters and fadeout
    calibrate_blinking();
    fade_set(FADE_SLOW);    
    fadeto(0xffff);
    _delay_ms(500);
    
    dotmode_set(DOT_BLINK);
    
    wdt_enable(WDTO_250MS);
    
    set_sleep_mode(SLEEP_MODE_IDLE);
    
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
                        case '`':   pump_nomoar();
                                    break;
                        case '.':   break;
                        case '=':   // die
                                    for(;;);
                                    break;
                        case 'q':   voltage_set(voltage_setpoint_get()-1);
                                    break;
                        case 'w':   voltage_set(voltage_setpoint_get()+1);
                                    break;
                        default:
                                    break;
                        }
            
                        if (byte >= '0' && byte <= '9') {
                            byte = byte - '0';
                            fadeto((byte<<12)+(byte<<8)+(byte<<4)+byte);
                            skip = 255;
                        }
                        
                        printf_P(PSTR("OCR1A=%d ICR1=%d S=%d V=%d, Time=%04x\n"), OCR1A, ICR1, voltage_setpoint_get(), voltage_get(), time);
                        break;
            }
        }
        
        buttonry_tick();
    
        if ((blinktick & _BV(1)) != 0) {        
            blinktick &= ~_BV(1);
            if (blinkhandler != NULL) {
                blinkhandler(1);
            }
        }
        
        if (skip != 0) {
            skip--;
        } else {
            mmss = rtc_gettime(1);
            if (!is_setting() && mmss != mmss1) {
                mmss1 = mmss;
                cli(); blinkctr = 0; sei();
            }
            
            rtime = rtc_gettime(0);
            
            update_daylight(rtime);
            
            savingmode_keep(rtime);
            
            switch (mode_get()) {
                case HHMM:
                    rtime = rtc_gettime(0);
                    break;
                case MMSS:
                    rtime = mmss;
                    break;
                case VOLTAGE:
                    rtime = voltage_getbcd();
                    break;
            }
            
            if (!is_setting() && rtime != time && rtime != timef) {
                fadeto(rtime);
            }     
        }
        
        // just waste time
        while((blinktick & _BV(2)) == 0) {
            sleep_enable();
            sleep_cpu();
        }
        blinktick &= ~_BV(2);
    }
}

