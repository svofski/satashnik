#include <inttypes.h>
#include "util.h"
#include "modes.h"

////
//// Fade mode
////

static volatile uint8_t fademode;      //!< \see _fademode

volatile uint16_t fadetime_full;
volatile uint16_t fadetime_quart;

void fade_set(uint8_t mode) {
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

inline enum _fademode fade_get() { return fademode; }


////
//// Dot mode
//// 

volatile uint8_t dotmode;

void dotmode_set(uint8_t mode) {
    dotmode = mode;
}


////
//// Display mode
////

volatile uint8_t display_mode = HHMM;   //!< current display mode. \see _displaymode


void mode_next() {
    display_mode = (display_mode + 1) % NDISPLAYMODES;
    switch (display_mode) {
        case HHMM:  fade_set(FADE_SLOW);
                    dotmode_set(DOT_BLINK);
                    break;
        case MMSS:  fade_set(FADE_ON);
                    dotmode_set(DOT_BLINK);
                    break;
        case VOLTAGE: fade_set(FADE_OFF);
                    dotmode_set(DOT_OFF);
                    break;
    }
}

uint8_t mode_get() {
    return display_mode;
}

////
//// Blink mode
////
volatile uint8_t blinkmode;     //!< current blinking mode

void blinkmode_set(uint8_t mode) {
    blinkmode = mode;
}

inline uint8_t blinkmode_get() { return blinkmode; }


////
//// Saving mode
////

static volatile uint8_t savingmode;    //!< nixe preservation mode


void savingmode_set(uint8_t s) {
    savingmode = s; 
}

inline uint8_t savingmode_get() { return savingmode; }

void savingmode_next() {
    savingmode_set((savingmode_get() + 1) % 3);
}


