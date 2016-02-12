/// \file
/// \brief Button setup-related stuff
///
#ifndef _BUTTONRY_H
#define _BUTTONRY_H

void debounce(uint8_t port, uint8_t* state, void (*handler)(uint8_t));
void buttons_init();
void button1_handler(uint8_t on);
void button2_handler(uint8_t on);
uint8_t is_setting();
void buttonry_tick();

#endif
