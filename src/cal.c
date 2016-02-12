#include <inttypes.h>

#include "util.h"
#include "cal.h"
#include "rtc.h"

static uint8_t daylight_adjusted = 0; //!< a flag that tells that DST adjustment took place already

/// Update DST: 
/// 02:00->03:00 on the last sunday of March
/// 03:00->02:00 on the last sunday of October
/// 
/// And try to do this only once..
void update_daylight(uint16_t time) {
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
                case 0x10:
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
