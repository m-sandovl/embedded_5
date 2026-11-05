#include "systick.h"
#include "led.h"
#include <stdint.h>

static void blink_task()
{
	static uint16_t cnt; 
	if( ++cnt == 500 )
	{
		toggle_led();
		cnt = 0;
	}
}
int main(void) {
	configure_led();
	register_systick_callback( blink_task );
	configure_systick();

	while(1)
		continue;
	return 0;
}
