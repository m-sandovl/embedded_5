/*
 * Author: Marc Sandoval
 * Purpose: Public interface for the Systick driver: configuration, callback
 * registration, and the system time getter.
 */

#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>
#include <stdbool.h>

#ifndef SYSTICK_FREQ_HZ	
#define SYSTICK_FREQ_HZ 1000000
#endif

uint64_t get_system_time();
void configure_systick();
bool register_systick_callback( void (*pf)() );

#endif
