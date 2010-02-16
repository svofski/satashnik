/// \file
/// \brief Boost converter closed-loop control
///
/// Setup routines for boost converter pulse generation and 
/// implementation of the closed control loop.
///

#ifndef _VOLTAGE_H
#define _VOLTAGE_H

#define VOLTAGE_WASTE   370                     //!< ~180V
#define VOLTAGE_SAVE    355                     //!< ~170V


/// Start voltage booster
void voltage_start();


/// Terminate boost converter operation 
void pump_nomoar();

uint16_t voltage_get();

uint16_t voltage_getbcd();

void voltage_set(uint16_t setpoint);    //!< set voltage setpoint

uint16_t voltage_setpoint_get();

#endif
