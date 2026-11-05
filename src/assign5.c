/*
 * Author: Marc Sandoval
 * Purpose: Assignment 5 (EECE 558). Shows a BCD number X and its hex value Y
 * on the LCD. 'e' enters entry mode, digits fill X, '*' discards, Enter accepts.
 * lcd_fsm() runs from the systick callback; the application FSM is
 * asynchronous and runs every main-loop iteration.
 */
#include "led.h"
#include "lcd.h"
#include "systick.h"
#include "usbcdc.h"
#include "interrupt.h"
#include <stdbool.h>
#include <stdint.h>

#define NUM_DIGITS 8
#define X_FIRST_ADDR 0x03  
#define Y_FIRST_ADDR 0x43
#define LCD_SET_ADDR(a) ((uint8_t)(0x80 | (a)))
#define LCD_DISPLAY_NO_CURSOR 0x0C
#define LCD_DISPLAY_BLINK_CURSOR 0x0F
#define PENDING_MAX 32

static struct { bool is_cmd; uint8_t value; } pending[PENDING_MAX];
static uint8_t pending_len, pending_pos;

static void put_cmd( uint8_t c )  { pending[pending_len].is_cmd = true;  pending[pending_len++].value = c; }
static void put_char( char c )    { pending[pending_len].is_cmd = false; pending[pending_len++].value = (uint8_t)c; }
static char hex_char( uint8_t v ) { return v < 10 ? (char)('0' + v) : (char)('A' + (v - 10)); }

static void flush_pending( void ) {
	while( pending_pos < pending_len ) {
		bool ok = pending[pending_pos].is_cmd
			? lcd_command( pending[pending_pos].value )
			: lcd_putchar( (char)pending[pending_pos].value );
		if( !ok )
			return;
		pending_pos++;
	}
	pending_len = 0;
	pending_pos = 0;
}
 
static void queue_x_digits( uint32_t value ) {
	char digits[NUM_DIGITS];
	for( int8_t i = NUM_DIGITS - 1; i >= 0; i-- ) {
		digits[i] = (char)('0' + value % 10);
		value /= 10;
	}
	put_cmd( LCD_SET_ADDR( X_FIRST_ADDR ) );
	for( uint8_t i = 0; i < NUM_DIGITS; i++ )
		put_char( digits[i] );
}
 
static void queue_y_hex( uint32_t value ) {
	put_cmd( LCD_SET_ADDR( Y_FIRST_ADDR ) );
	for( uint8_t i = 0; i < NUM_DIGITS; i++ )
		put_char( hex_char( (value >> (28 - 4 * i)) & 0xF ) );
}
 
static void queue_start_entry( void ) {
	put_cmd( LCD_DISPLAY_BLINK_CURSOR );
	put_cmd( LCD_SET_ADDR( X_FIRST_ADDR ) );
	for( uint8_t i = 0; i < NUM_DIGITS; i++ )
		put_char( ' ' );
	put_cmd( LCD_SET_ADDR( X_FIRST_ADDR ) );
}
 
static void app_fsm( void ) {
	static enum { DISPLAY_MODE, ENTRY_MODE } mode = DISPLAY_MODE;
	static uint8_t input[NUM_DIGITS];
	static uint8_t index;
	static uint32_t stored;
	char c;
 
	flush_pending();
	if( pending_pos < pending_len )
		return;
	if( !usbcdc_getchar( &c ) )
		return;
 
	if( c == 'e' ) {                          
		for( uint8_t i = 0; i < NUM_DIGITS; i++ )
			input[i] = 0;
		index = 0;
		queue_start_entry();
		mode = ENTRY_MODE;
		return;
	}
	if( mode == DISPLAY_MODE )
		return;                               
 
	if( c == '*' ) {                          
		put_cmd( LCD_DISPLAY_NO_CURSOR );
		queue_x_digits( stored );
		mode = DISPLAY_MODE;
	} else if( c == '\r' || c == '\n' ) {      
		stored = 0;
		for( uint8_t i = 0; i < NUM_DIGITS; i++ )
			stored = stored * 10 + input[i];
		put_cmd( LCD_DISPLAY_NO_CURSOR );
		queue_x_digits( stored );
		queue_y_hex( stored );
		mode = DISPLAY_MODE;
	} else if( c >= '0' && c <= '9' ) {
		input[index] = (uint8_t)(c - '0');
		put_char( c );
		if( ++index == NUM_DIGITS ) {          
			index = 0;
			put_cmd( LCD_SET_ADDR( X_FIRST_ADDR ) );
		}
	}
}
 
int main( void ) {
	static const uint8_t init_cmds[] = { 0x32, 0x28, 0x10, LCD_DISPLAY_NO_CURSOR, 0x06, 0x01 };
 
	__disable_irq();
	configure_led();
	configure_usbcdc();
	configure_lcd();
	bool ok = register_systick_callback( lcd_fsm );
	if( ok )
		configure_systick();
 
	for( uint8_t i = 0; i < sizeof init_cmds; i++ )
		lcd_command( init_cmds[i] );
	lcd_command( LCD_SET_ADDR( 0x00 ) );
	for( const char *p = "X: 00000000"; *p; p++ )
		lcd_putchar( *p );
	lcd_command( LCD_SET_ADDR( 0x40 ) );
	for( const char *p = "Y: 00000000"; *p; p++ )
		lcd_putchar( *p );
	__enable_irq();
 
	if( !ok ) {                                
		turn_on_led();
		while( 1 )
			continue;
	}
 
	while( 1 )
		app_fsm();
}
