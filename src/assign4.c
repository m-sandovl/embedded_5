/*
 * Author: Marc Sandoval
 * Purpose: Electronic lock application. A keypad code unlocks
 * the lock, any key locks it again, and it locks itself after 10 s of inactivity.
 * The LED is on while the lock is locked.
 */
#include "led.h"
#include "systick.h"
#include "keypad.h"
#include <stdbool.h>
#include <stdint.h>

#define UNLOCK_CODE_LENGTH 4
#define LOCK_TIMEOUT_MS 10000

bool unlock_sequence_completed( char key );

int main( void ) {
	configure_led();
	configure_keypad();
	if( !register_systick_callback( keypad_scan_fsm ) )
		return 1;
	configure_systick();

	bool locked = true;
	uint64_t last_time = get_system_time();
	uint64_t last_key_time = last_time;

	while( 1 ) {
		uint64_t now = get_system_time();
		if( now == last_time )
			continue;
		last_time = now;

		char key = get_keypad_key();
		if( key != KEYPAD_NO_KEY ) {
			last_key_time = now;
			locked = !unlock_sequence_completed( key );
		}

		if( now - last_key_time >= LOCK_TIMEOUT_MS )
			locked = true;

		if( locked )
			turn_on_led();
		else
			turn_off_led();
	}
}

bool unlock_sequence_completed( char key ) {
	static const char unlock_code[UNLOCK_CODE_LENGTH] = { '1', '2', '0', '5' };
	static uint8_t digits_matched;

	if( digits_matched < UNLOCK_CODE_LENGTH ) {
		if( key == unlock_code[digits_matched] )
			digits_matched++;
		else
			digits_matched = 0;
		return false;
	}

	digits_matched = 0;
	return key == '#';
}
