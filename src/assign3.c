/*
 * Author: Marc Sandoval
 * Purpose: Main application for Assignment 3. Flashes the LED with a
 * button-controlled duty cycle and frequency using a 1 ms Systick-paced loop.
 */
#include "led.h"
#include "systick.h"
#include "button.h"
#include <stdint.h>

#define DUTY_CYCLE_MAX 32
#define DEFAULT_DUTY_CYCLE (DUTY_CYCLE_MAX / 2)
#define DEFAULT_FREQUENCY_HZ 1 
#define MAX_FREQUENCY_HZ 64
#define MS_PER_SECOND 1000
#define IDLE_TIMEOUT_MS 10000

void led_fsm( uint8_t m, uint8_t frequency );

int main( void ) {
	configure_led();
	configure_button();
	register_systick_callback( button_press_fsm ); 
	configure_systick(); 
	
	uint8_t m = DEFAULT_DUTY_CYCLE;
	uint8_t frequency = DEFAULT_FREQUENCY_HZ;
	uint64_t last_time = get_system_time(); 
	uint64_t last_event = last_time; 

	while( 1 ) {
		uint64_t now = get_system_time(); 
		if( now == last_time ) 
			continue;
		last_time = now;
		
		button_press_t press = get_button_state();
		if( press != NO_PRESS )
			last_event = now;

		if( press == SHORT_PRESS ) {
			if( ++m > DUTY_CYCLE_MAX ) 
				m = 0;
		}
		if( press == PRESS ) {
			frequency *= 2; 
			if( frequency > MAX_FREQUENCY_HZ ) 
				frequency = DEFAULT_FREQUENCY_HZ; 
		}
		if( press == LONG_PRESS ) { 
			m = DEFAULT_DUTY_CYCLE; 
			frequency = DEFAULT_FREQUENCY_HZ;
		}
		
		if( now - last_event >= IDLE_TIMEOUT_MS ) {
			m = DEFAULT_DUTY_CYCLE;
			frequency = DEFAULT_FREQUENCY_HZ ;
			last_event = now; 
		}

		led_fsm( m, frequency );
	}
}

void led_fsm( uint8_t m, uint8_t frequency ) {
	static uint16_t count; 	
	static uint8_t count_increment = DEFAULT_FREQUENCY_HZ;

	if( m > DUTY_CYCLE_MAX ) 
		m = DUTY_CYCLE_MAX; 
	
	switch( frequency ) {
		case 1: 
		case 2: 
		case 4: 
		case 8:
		case 16: 
		case 32: 
		case 64:
			count_increment = frequency; 
			break; 
		default:
			break; 
	}
		
	uint16_t on_ms = (m * MS_PER_SECOND) / DUTY_CYCLE_MAX; 
	
	if( count < on_ms )
		turn_on_led();
	else
		turn_off_led();
	count += count_increment;
	if( count >= MS_PER_SECOND )
		count -= MS_PER_SECOND;
}
