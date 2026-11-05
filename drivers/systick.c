/*
 * Author: Marc Sandoval
 * Purpose: Interrupt-based Systick driver implementation. Keeps a system time
 * tick counter and calls a configurable number of registered callbacks each tick.
 */

#include <rp2350/m33.h>
#include <rp2350/ticks.h>
#include <systick.h>
#include <interrupt.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef SYSTICK_FREQ_HZ
#define SYSTICK_FREQ_HZ 1000
#endif

#ifndef EXT_CLK_FREQ_HZ
#define EXT_CLK_FREQ_HZ 12000000
#endif

#define SYSTICK_TOP (EXT_CLK_FREQ_HZ/SYSTICK_FREQ_HZ - 1)

#ifndef NUM_SYSTICK_CALLBACKS
#define NUM_SYSTICK_CALLBACKS 4
#endif

static void(*callback[NUM_SYSTICK_CALLBACKS])();
static uint8_t num_registered_callbacks;


bool register_systick_callback( void (*pf)() ) {
	if( pf == 0 || num_registered_callbacks >= NUM_SYSTICK_CALLBACKS ) 
		return false; 
	callback[num_registered_callbacks++] = pf;
	return true; 
}

static volatile uint64_t tick_counter;

void __attribute__((interrupt)) SYSTICK_Handler() {
	tick_counter++; 
	for ( uint8_t i = 0; i < num_registered_callbacks; i++ )
		callback[i]();
}

uint64_t get_system_time() {	
	uint64_t retval;
	uint32_t primask;
	__get_primask(&primask);
	__disable_irq();
	retval = tick_counter; 
	__enable_irq(); 
	__set_primask(primask);
	return retval;
}

void configure_systick() {
	TICKS_PROC0_CYCLES = 1;
	TICKS_PROC0_CTRL = TICKS_PROC0_CTRL_ENABLE(1);
	M33_SYST_RVR = SYSTICK_TOP;
	M33_SYST_CVR = 0;
	M33_SYST_CSR = 
		M33_SYST_CSR_CLKSOURCE(0) 
		| M33_SYST_CSR_TICKINT(1) 
		| M33_SYST_CSR_ENABLE(1);
}





