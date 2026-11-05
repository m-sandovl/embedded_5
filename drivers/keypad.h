/*
 * Author: Marc Sandoval
 * Purpose: Public interface for the 3x4 keypad driver: configuration, the
 * scanning FSM to register as a Systick callback, and the last-key getter.
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#define KEYPAD_NO_KEY '\0'

void configure_keypad();
void keypad_scan_fsm();
char get_keypad_key();

#endif
