/// \file
/// \brief Boost converter closed-loop control
///
/// Setup routines for boost converter pulse generation and 
/// implementation of the closed control loop.
///

#ifndef _VOLTAGE_H
#define _VOLTAGE_H

extern volatile uint16_t voltage;               //!< voltage (magic units)
extern volatile uint16_t voltage_setpoint;      //!< voltage setpoints (magic units)


/// Setup PWM output for the boost converter
void pump_init();

/// Init ADC converter and ADC interrupt
void adc_init();


/// Terminate boost converter operation 
void pump_nomoar();

/// Adjust output voltage for specific display contents and duty cycle.
/// flag = 0 initiates adjustment. Logic is magic.
void voltage_adjust(uint8_t flag);

/// Must be called every tick from the main timer interrupt routine.
/// After voltage delay is expired, sets voltage setpoint to postponed value.
void voltage_adjust_tick();

#endif
