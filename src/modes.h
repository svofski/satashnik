#ifndef _MODES_H_
#define _MODES_H_

#define FADETIME    128        //<! Transition time for xfading digits, in tmr0 overflow-counts

#define FADETIME_S  256        //<! Slow transition time

/// Fade modes. 
/// Fade is off for in setup and voltmeter modes
enum _fademode {
    FADE_OFF = 0,
    FADE_ON,
    FADE_SLOW
};

extern volatile uint16_t fadetime_full;
extern volatile uint16_t fadetime_quart;

void fade_set(uint8_t mode);
enum _fademode fade_get();

/// Dot modes
enum _dotmode {
    DOT_BLINK = 0,
    DOT_ON,
    DOT_OFF
};

extern volatile uint8_t dotmode;       //!< dot blinking mode \see _dotmode

void dotmode_set(uint8_t mode);

/// Display modes
#define NDISPLAYMODES 3
enum _displaymode {
    HHMM = 0,               //!< Normal mode, HH:MM
    MMSS,                   //!< Minutes:Seconds mode, set button resets seconds to zero
    VOLTAGE                 //!< Voltmeter mode
};

void mode_next();
inline uint8_t mode_get();

/// Blinking modes, see timer0 overflow interrupt
enum _blinkmode {
    BLINK_NONE = 0,
    BLINK_HH = 1,
    BLINK_MM = 2,
    BLINK_ALL = 3,
    BLINK_SUPPRESS = 0200,  //!< To be OR'ed with current mode
};


void blinkmode_set(uint8_t mode);

uint8_t blinkmode_get();

/// Saving modes
enum _savinmode {
    WASTE = 0,              //!< Full-on all the time
    SAVE,                   //!< constantly preserve
    SAVENIGHT,              //!< preserve 00:00-08:00
};

void savingmode_set(uint8_t s);
uint8_t savingmode_get();
void savingmode_next();



#endif
