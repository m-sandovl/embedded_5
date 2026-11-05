/*
 * Author: Marc Sandoval
 * Purpose: Button driver implementation. Configures an active-low push-button
 * and runs a debouncing FSM that classifies presses as short, normal, or long.
 */

#include "button.h"
#include <stdint.h>
#include <rp2350/resets.h>
#include <rp2350/sio.h>
#include <rp2350/io_bank0.h>
#include <rp2350/pads_bank0.h>
#include <interrupt.h>

#define DEBOUNCE_CNT 5
#define SHORT_MAX (500 / DEBOUNCE_CNT)
#define PRESS_MAX (2000 / DEBOUNCE_CNT) 

#define BUTTON_RESETS (RESETS_RESET_IO_BANK0_MASK | RESETS_RESET_PADS_BANK0_MASK)
#define BUTTON_PIN_MASK (1u << 27)

void configure_button() {
	resets.reset_clr = BUTTON_RESETS;
	while( !(resets.reset_done & BUTTON_RESETS) )
		continue;

	pads_bank0.gpio27 = 
		PADS_BANK0_GPIO27_OD(0) | PADS_BANK0_GPIO27_IE(1) | PADS_BANK0_GPIO27_DRIVE(0) | PADS_BANK0_GPIO27_PUE(1) | PADS_BANK0_GPIO27_PDE(0) | PADS_BANK0_GPIO27_SCHMITT(0) 
		| PADS_BANK0_GPIO27_SLEWFAST(0);

	io_bank0.gpio27_ctrl =
		IO_BANK0_GPIO27_CTRL_IRQOVER(0) | IO_BANK0_GPIO27_CTRL_INOVER(0) | IO_BANK0_GPIO27_CTRL_OEOVER(0) | IO_BANK0_GPIO27_CTRL_OUTOVER(0) | IO_BANK0_GPIO27_CTRL_FUNCSEL(5);
}

static volatile button_press_t last_press = NO_PRESS; 

void button_press_fsm() {
	static enum { WAIT_PRESS, WAIT_RELEASE } state = WAIT_PRESS;
	static uint8_t debounce_cntr; 
	static uint16_t held;

	if( ++debounce_cntr < DEBOUNCE_CNT )
		return; 
	debounce_cntr = 0;

	_Bool button_is_down = ( SIO_GPIO_IN & BUTTON_PIN_MASK) == 0;

	switch( state ) {
	case WAIT_PRESS:
		if( button_is_down ) { held = 0; state = WAIT_RELEASE; }
		break;
	case WAIT_RELEASE:
		if( button_is_down ) { if(held < UINT16_MAX ) held++; }
		else {
			if( held < SHORT_MAX )
				last_press = SHORT_PRESS; 
			else if( held < PRESS_MAX )
				last_press = PRESS;
			else
				last_press = LONG_PRESS;
			state = WAIT_PRESS;
		}
		break; 
	}
}

button_press_t get_button_state() {
	button_press_t retval; 
	__disable_irq();
	retval = last_press;
	last_press = NO_PRESS;
	__enable_irq(); 
	return retval;
}
