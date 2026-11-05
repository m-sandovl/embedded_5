/*
 * Author: Marc Sandoval
 * Purpose: Public interface for the button driver: press types, configuration,
 * the press-detecting FSM callback, and the getter for the last press.
 */

#ifndef BUTTON_H
#define BUTTON_H

typedef enum { NO_PRESS, SHORT_PRESS, PRESS, LONG_PRESS } button_press_t;

void configure_button();
button_press_t get_button_state();
void button_press_fsm();

#endif
