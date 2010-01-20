/// \file
/// \brief Boost converter closed-loop control
///
/// Setup routines for boost converter pulse generation and 
/// implementation of the closed control loop.
///

#ifndef _VOLTAGE_H
#define _VOLTAGE_H

#define VOLTAGE_WASTE   405                     //!< ~199V
#define VOLTAGE_SAVE    367                     //!< ~180V

extern volatile uint16_t voltage;               //!< voltage (magic units)
extern volatile uint16_t voltage_setpoint;      //!< voltage setpoints (magic units)


/// Setup PWM output for the boost converter
void pump_init();

/// Init ADC converter and ADC interrupt
void adc_init();


/// Terminate boost converter operation 
void pump_nomoar();

uint16_t voltage_getbcd();

#endif
