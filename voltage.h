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

/// Setup PWM output for the boost converter
void pump_init();

/// Init ADC converter and ADC interrupt
void adc_init();


/// Terminate boost converter operation 
void pump_nomoar();

uint16_t voltage_get();

uint16_t voltage_getbcd();

void voltage_set(uint16_t setpoint);    //!< set voltage setpoint

uint16_t voltage_setpoint_get();

#endif
