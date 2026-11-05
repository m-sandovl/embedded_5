#include "keypad.h"
#include "systick.h"
#include "usbcdc.h"

int main( void ) {
	char c;

	__asm__ volatile ("cpsid i");
	configure_usbcdc();
	__asm__ volatile ("cpsie i");

	configure_keypad();
	register_systick_callback( keypad_scan_fsm );
	configure_systick();

	while( !usbcdc_getchar( &c ) )
		continue;

	while( 1 ) {
		char key = get_keypad_key();
		if( key != KEYPAD_NO_KEY )
			usbcdc_putchar( key );
	}
}


