#include "systick.h"
#include "button.h"
#include "led.h"

static volatile int blinks;

int main( void )
{
	configure_led();
	configure_button();
	register_systick_callback( button_press_fsm );
	configure_systick();

	while(1)
	{
		button_press_t p = get_button_state();
		if( p == SHORT_PRESS )     turn_on_led();
		else if( p == PRESS )      toggle_led();
		else if( p == LONG_PRESS ) turn_off_led();
	}
	return(0);
}
