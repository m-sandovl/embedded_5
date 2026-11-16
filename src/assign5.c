/*
 * Author: Marc Sandoval
 * Purpose: Assignment 5. Shows a BCD number X and its hex value Y
 * on the LCD. 'e' enters entry mode, digits fill X, '*' discards, Enter accepts.
 */
#include "led.h"
#include "lcd.h"
#include "systick.h"
#include "usbcdc.h"
#include "interrupt.h"
#include <stdbool.h>
#include <stdint.h>

#define NUM_DIGITS 8
#define BITS_PER_HEX_DIGIT 4
#define HEX_DIGIT_MASK 0xF
#define LABEL_LENGTH 3
#define LINE_1_ADDRESS 0x00
#define LINE_2_ADDRESS 0x40
#define X_FIRST_DIGIT_ADDRESS (LINE_1_ADDRESS + LABEL_LENGTH)
#define Y_FIRST_DIGIT_ADDRESS (LINE_2_ADDRESS + LABEL_LENGTH)
#define LCD_SET_ADDRESS(address) ((uint8_t)(0x80 | (address)))
#define LCD_CURSOR_OFF 0x0C
#define LCD_CURSOR_BLINKING 0x0F
#define LCD_FUNCTION_SET_4BIT_HANDSHAKE 0x32
#define LCD_FUNCTION_SET_4BIT_2LINE 0x28
#define LCD_CURSOR_SHIFT_LEFT 0x10
#define LCD_ENTRY_MODE_INCREMENT 0x06
#define LCD_CLEAR_DISPLAY 0x01
#define MAX_LCD_ENTRIES_PER_KEY 19

static void bcd_entry_fsm( char key );
static void show_initial_screen( void );
static void show_text( const char *text );
static void show_digits( const uint8_t digits[NUM_DIGITS] );
static void show_hex( uint32_t value );
static void start_digit_entry( void );
static uint32_t digits_to_number( const uint8_t digits[NUM_DIGITS] );
static char hex_digit_character( uint8_t value );

int main( void ) {
	__disable_irq();
	configure_led();
	configure_usbcdc();
	configure_lcd();
	bool lcd_scheduled = register_systick_callback( lcd_fsm );
	if( lcd_scheduled )
		configure_systick();
	show_initial_screen();
	__enable_irq();

	if( !lcd_scheduled ) {
		turn_on_led();
		while( 1 )
			continue;
	}

	while( 1 ) {
		char key;
		if( lcd_free_fifo_entries() >= MAX_LCD_ENTRIES_PER_KEY && usbcdc_getchar( &key ) )
			bcd_entry_fsm( key );
	}
}

static void bcd_entry_fsm( char key ) {
	static enum { DISPLAY_MODE, ENTRY_MODE } mode = DISPLAY_MODE;
	static uint8_t entered_digits[NUM_DIGITS];
	static uint8_t accepted_digits[NUM_DIGITS];
	static uint8_t cursor_index;

	if( key == 'e' ) {
		for( uint8_t i = 0; i < NUM_DIGITS; i++ )
			entered_digits[i] = 0;
		cursor_index = 0;
		start_digit_entry();
		mode = ENTRY_MODE;
		return;
	}
	if( mode == DISPLAY_MODE )
		return;

	if( key == '*' ) {
		lcd_command( LCD_CURSOR_OFF );
		show_digits( accepted_digits );
		mode = DISPLAY_MODE;
	} else if( key == '\r' || key == '\n' ) {
		for( uint8_t i = 0; i < NUM_DIGITS; i++ )
			accepted_digits[i] = entered_digits[i];
		lcd_command( LCD_CURSOR_OFF );
		show_digits( accepted_digits );
		show_hex( digits_to_number( accepted_digits ) );
		mode = DISPLAY_MODE;
	} else if( key >= '0' && key <= '9' ) {
		entered_digits[cursor_index] = (uint8_t)( key - '0' );
		lcd_putchar( key );
		if( ++cursor_index == NUM_DIGITS ) {
			cursor_index = 0;
			lcd_command( LCD_SET_ADDRESS( X_FIRST_DIGIT_ADDRESS ) );
		}
	}
}

static void show_initial_screen( void ) {
	static const uint8_t lcd_init_commands[] = { LCD_FUNCTION_SET_4BIT_HANDSHAKE, LCD_FUNCTION_SET_4BIT_2LINE, LCD_CURSOR_SHIFT_LEFT,
		LCD_CURSOR_OFF, LCD_ENTRY_MODE_INCREMENT, LCD_CLEAR_DISPLAY };
	static const uint8_t zero_digits[NUM_DIGITS];

	for( uint8_t i = 0; i < sizeof lcd_init_commands; i++ )
		lcd_command( lcd_init_commands[i] );
	lcd_command( LCD_SET_ADDRESS( LINE_1_ADDRESS ) );
	show_text( "X: " );
	show_digits( zero_digits );
	lcd_command( LCD_SET_ADDRESS( LINE_2_ADDRESS ) );
	show_text( "Y: " );
	show_hex( 0 );
}

static void show_text( const char *text ) {
	for( ; *text != '\0'; text++ )
		lcd_putchar( *text );
}

static void show_digits( const uint8_t digits[NUM_DIGITS] ) {
	lcd_command( LCD_SET_ADDRESS( X_FIRST_DIGIT_ADDRESS ) );
	for( uint8_t i = 0; i < NUM_DIGITS; i++ )
		lcd_putchar( (char)( '0' + digits[i] ) );
}

static void show_hex( uint32_t value ) {
	lcd_command( LCD_SET_ADDRESS( Y_FIRST_DIGIT_ADDRESS ) );
	for( uint8_t i = 0; i < NUM_DIGITS; i++ ) {
		uint8_t shift = BITS_PER_HEX_DIGIT * ( NUM_DIGITS - 1 - i );
		lcd_putchar( hex_digit_character( ( value >> shift ) & HEX_DIGIT_MASK ) );
	}
}

static void start_digit_entry( void ) {
	lcd_command( LCD_CURSOR_BLINKING );
	lcd_command( LCD_SET_ADDRESS( X_FIRST_DIGIT_ADDRESS ) );
	for( uint8_t i = 0; i < NUM_DIGITS; i++ )
		lcd_putchar( ' ' );
	lcd_command( LCD_SET_ADDRESS( X_FIRST_DIGIT_ADDRESS ) );
}

static uint32_t digits_to_number( const uint8_t digits[NUM_DIGITS] ) {
	uint32_t number = 0;
	for( uint8_t i = 0; i < NUM_DIGITS; i++ )
		number = number * 10 + digits[i];
	return number;
}

static char hex_digit_character( uint8_t value ) {
	return ( value < 10 ) ? (char)( '0' + value ) : (char)( 'A' + ( value - 10 ) );
}
