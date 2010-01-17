#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "voltage.h"
#include "util.h"

static uint16_t voltage_delayed = 0;
static uint8_t voltage_delay_count = 0;

volatile uint16_t voltage;                  //!< voltage (magic units)
volatile uint16_t voltage_setpoint = 400;   //!< voltage setpoints (magic units)

extern uint8_t on_duty;
extern uint16_t time;

extern uint16_t ocr1a_reload;


void pump_init() {
    // set fast pwm mode
    // COM1A1:0 = 10, clear oc1a on compare match, set at top
    // COM1B1:0 = 00, normal port operation
    // no FOC
    // WGM11:10 (WGM = Fast PWM, TOP=ICR1: 1110) = 11
    TCCR1A = BV2(COM1A1, WGM11);
    TCCR1B = BV2(WGM13,WGM12);
    
    OCR1A = ocr1a_reload; 
    ICR1 = 170;
    
    TCCR1B |= _BV(CS10); // clk/1 = 8MHz
    
    DDRHVPUMP |= BV2(1,2);
}

void pump_nomoar() {
    ADCSRA = 0;
    ADCSRA |= _BV(ADIF); 

    ICR1 = 255;
    OCR1A = 0;
    OCR1B = OCR1A;
}

void pump_faster(int8_t dfreq) {
    ICR1 += dfreq;
}

inline void voltage_adjust(uint8_t flag) {
/*
    switch (flag) {
    case 0:
        if (on_duty == 4) {
            if (((time & 0x00f0)>>4) == 2) {
                voltage_setpoint = 410;
                cli();
                voltage_delayed = 380;
                voltage_delay_count = 64;
                sei();
            } else {
                voltage_setpoint = 380;
            }
        } else {
            voltage_setpoint = 400;
        }
        break;
    case 1:
        voltage_setpoint = voltage_delayed;
        break;
    }
*/    
}


void voltage_adjust_tick() {
    if (voltage_delay_count != 0) {
        voltage_delay_count--;
        if (voltage_delay_count == 0) {
            voltage_adjust(1);
        }
    }
}


void adc_init() {
    voltage = 0;
    
    // PORTA.7 is the feedback input, AREF = AREF pin
    ADMUX = 7;  
    //DDRA &= ~_BV(7);

    // ADC enable, autotrigger, interrupt enable, prescaler = 111 (divide by 32)
    ADCSRA = BV6(ADEN, ADFR, ADIE, ADPS2, ADPS1, ADPS0); 
    //ADCSRA = BV5(ADEN, ADIE, ADPS2, ADPS1, ADPS0); 

    ADCSRA |= _BV(ADSC); 
}

ISR(ADC_vect) {
    voltage = (voltage + ADC) / 2;
    if (voltage < voltage_setpoint) {
        OCR1A = ocr1a_reload;
    } else {
        OCR1A = 0;
    }
}
