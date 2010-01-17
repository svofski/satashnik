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

volatile uint16_t time = 0;     //!< current display value
volatile uint16_t timef = 0;    //!< fadeto display value

volatile uint8_t digitmux = 0;  //!< displayed digit, 0..3
volatile uint16_t digitsraw = 0;   //!< raw port values

volatile uint8_t  dispctr = 0;  //!< counts 0..15
volatile uint8_t  on_duty = 1;  //!< values 1..4, gets multiplied by 4

uint16_t ocr1a_reload = 121;

volatile uint8_t blinktick = 0;


volatile uint8_t blinkmode;     //!< current blinking mode
volatile uint16_t blinkctr;     //!< blinkmode counter
volatile uint8_t blinkduty;     //!< blinkmode duty 

volatile uint8_t fadeduty, fadectr; //!< crossfade counters
volatile int16_t fadetime;      //!< crossfade time and trigger, write "-1" to start fade to timef

#define FADETIME 512            //<! Transition time for xfading digits, in 3096 Hz clock-counts
#define BLINKTIME 2048          //<! Blink time in 3096 Hz clock-counts
#define BLINKBIT  10            //<! Flag bit, used internally by the blinker

inline void set_blinkmode(uint8_t mode) {
    blinkmode = mode;
}

/// Init display-related DDRs.
void initdisplay() {
    DDRDIGIT |= BV4(0,1,2,3);
    DDRSA1 |= _BV(0);
    DDRSA234 |= BV3(5,6,7);
}


inline uint8_t swapbits(uint8_t x) {
    return ((x & 1) << 3) | (x & 2) | ((x & 4) >> 2) | ((x & 8) >> 1);
}

/// Rehash bits to match the schematic
inline void showtime(uint8_t h1, uint8_t h2, uint8_t m1, uint8_t m2) {
    digitsraw = (swapbits(h1)<<12) | (swapbits(h2)<<8) | (swapbits(m1)<<4) | swapbits(m2);
}

/// Immediately display BCD value on nixies
void showtime_bcd(uint16_t time) {
    showtime((time & 0xf000)>>12, (time & 0x0f00)>>8, (time & 0x00f0)>>4, time & 0x000f);
}

void display_currentdigit(uint8_t n) {
    if (n < 4) {
        //PORTDIGIT &= ~BV4(0,1,2,3);
        //PORTDIGIT |= 0x0f & (digitsraw >> (n<<2));
        PORTDIGIT = (PORTDIGIT & ~BV4(0,1,2,3)) | (0x0f & (digitsraw >> (n<<2)));
    } else {
        PORTDIGIT |= 0x0f;
    }
}

void display_selectdigit(uint8_t n) {
    switch (n) {
        case SA1: 
                PORTSA234 &= ~BV3(5,6,7);
                _delay_ms(0.01);
                display_currentdigit(n);
                PORTSA1 |= _BV(0);
                break;
        case SA2:
        case SA3:
        case SA4:
                PORTSA1 &= ~_BV(0);
                PORTSA234 &= ~BV3(5,6,7);
                _delay_ms(0.01);
                display_currentdigit(n);
                PORTSA234 |= 0200 >> (n-1);
                //PORTSA234 = (PORTSA234 & ~BV3(5,6,7)) | (0200 >> (n-1));
                break;
        default:
                PORTSA234 &= ~BV3(5,6,7);
                PORTSA1 &= ~_BV(0);
                break;
    }
    //printf("\nselect digit %d, port=%02x\n", n, PORTDIGIT);
}


/// Sets tubes duty cycle. Valid values are 1..4.
void duty_set(uint8_t d) {
    on_duty = d;
    voltage_adjust(0);
}

inline uint8_t duty_get() { return on_duty; }

/// Start fading time to given value. 
/// Transition is performed in TIMER0_OVF_vect and takes FADETIME cycles.
void fadeto(uint16_t t) { 
    cli();
    timef = t; fadetime = -1;
    sei();
}

inline uint16_t get_display_value() {
    return timef;
}

/// Start timer 0. Timer0 runs at 1MHz and overflows at 3906 Hz.
void timer0_init() {
    TIMSK |= _BV(TOIE0);    // enable Timer0 overflow interrupt
    TCNT0 = 256-20;
    TCCR0 = BV2(CS01,CS00);   // clk/64 = 125000Hz, overflow rate 488Hz
}


ISR(TIMER0_OVF_vect) {
    uint16_t toDisplay = time;
    static uint8_t effDuty;
    static uint8_t odd = 0;
    
    odd += 1;
    
    TCNT0 = 256-20;
    
    voltage_adjust_tick();
    
    if (blinkmode != BLINK_NONE) {
        blinkctr = (blinkctr + 1) % BLINKTIME;
        if (blinkctr & _BV(BLINKBIT)) {
            if (effDuty != blinkduty) {
                blinktick = 1;
            }
            effDuty = blinkduty;
        } else {
            effDuty = on_duty;
        }
    } 
    
    if (fadetime == -1) {
        // start teh fade
        fadetime = FADETIME;
        fadeduty = 4;
        fadectr = 0;
    }
    
    if (fadetime != 0) {
        fadetime--;

        if (fadetime % (FADETIME/4) == 0) {
            fadeduty--;
        }
        
        if (fadetime == 0) {
            fadectr = 0;
            time = timef; // end fade
        }
    } 
    

    if (fadectr < fadeduty) {
        toDisplay = time;
    } 
    else {
        toDisplay = timef;
    }
    fadectr = (fadectr + 1) & 3;

/*
    if (dispctr < on_duty<<2) {
        switch (blinkmode) {
            case BLINK_HH:
                if (dispctr >= (effDuty<<2)) toDisplay |= 0xff00;
                break;
            case BLINK_MM:
                if (dispctr >= (effDuty<<2)) toDisplay |= 0x00ff;
                break;
            case BLINK_ALL:
                if (dispctr >= (effDuty<<2)) toDisplay |= 0xffff;
                break;
            default:
                break;
        }
    } else {
        toDisplay |= 0xffff;
    }
*/    
    showtime_bcd(toDisplay);
    
    dispctr = (dispctr + 1) & 017;
    
    if (odd & 1) {
        display_selectdigit(digitmux);
        digitmux = (digitmux + 1) & 3;
    } else {
        switch (PORTDIGIT & 017) {
            case 0x01: // 4
            case 0x04: // 8
            case 0x0a: // 3
            case 0x0b: // 7
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
    
    duty_set(1);

    rtc_init();
    
    buttons_init();
 
    time = 0xffff;   
    rtime = timef = 0x1838;
    fadetime = -1;
    fadeto(0x1838);
    
    timer0_init();

    _delay_ms(1000);

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
                        case 'd':   duty_set(on_duty % 4 + 1);
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
                        
                        printf_P(PSTR("OCR1A=%d ICR1=%d S=%d V=%d, Time=%04x, PORT=%1x\n"), OCR1A, ICR1, voltage_setpoint, voltage, time, PORTDIGIT & 0x0f);
                        break;
            }
            
        }
        
        display_selectdigit(digitmux);
        digitmux = (digitmux + 1) & 3;
        
        if (skip != 0) {
            skip--;
        } else {
            //rtime = rtc_gettime();//voltage;
            //update_daylight(rtime);
            
            skip = 512;
            rtime = bcd_increment((rtime & 0xff00) >> 8) | (bcd_increment(rtime & 0xff) << 8);
            
            if (!is_setting() && rtime != time && rtime != timef) {
                fadeto(rtime);
            }            

            //time = rtime;
        }

        buttonry_tick(PIND & _BV(3), PIND & _BV(4));
    
        if (blinktick) {        
            blinktick = 0;
            if (blinkhandler != NULL) {
                blinkhandler(1);
            }
        }
        
        _delay_ms(2);

/*

        // compensate for the brightness of the outer digits: make them burn shorter        
        _delay_ms(1);
        switch (PORTDIGIT & 017) {
            case 0x01: // 4
            case 0x04: // 8
            case 0x0a: // 3
            case 0x0b: // 7
                display_selectdigit(0377);
                break;
            
        }
        _delay_ms(1);
*/
    }
}

