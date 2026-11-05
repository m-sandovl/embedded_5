/*
 * Author: Marc Sandoval
 * Purpose: Keypad driver implementation. Scans a 3x4 matrix keypad with a
 * debounced FSM that stores the most recent key press for the application.
 */

#include "keypad.h"
#include <stdint.h>
#include <rp2350/resets.h>
#include <rp2350/sio.h>
#include <rp2350/io_bank0.h>
#include <rp2350/pads_bank0.h>
#include <interrupt.h>

#define ROW0_PIN 28
#define ROW1_PIN 27
#define ROW2_PIN 26
#define ROW3_PIN 22
#define COL0_PIN 2
#define COL1_PIN 3
#define COL2_PIN 4

#define NUM_ROWS 4
#define NUM_COLUMNS 3
#define NO_COLUMN 255
#define KEYPAD_SCAN_PERIOD_TICKS 5

#define ALL_ROWS_MASK ( (1u << ROW0_PIN) | (1u << ROW1_PIN) | (1u << ROW2_PIN) | (1u << ROW3_PIN) )
#define KEYPAD_RESETS (RESETS_RESET_IO_BANK0_MASK | RESETS_RESET_PADS_BANK0_MASK)

static void drive_row( uint8_t row );
static void drive_all_rows();
static uint8_t read_column();

static const uint32_t row_masks[NUM_ROWS] = {
	1u << ROW0_PIN, 1u << ROW1_PIN, 1u << ROW2_PIN, 1u << ROW3_PIN
};
static const uint32_t column_masks[NUM_COLUMNS] = {
	1u << COL0_PIN, 1u << COL1_PIN, 1u << COL2_PIN
};
static const char key_characters[NUM_ROWS * NUM_COLUMNS] = "123456789*0#";

static volatile char last_key = KEYPAD_NO_KEY;

void configure_keypad() {
	resets.reset_clr = KEYPAD_RESETS;
	while( !(resets.reset_done & KEYPAD_RESETS) )
		continue;

	pads_bank0.gpio28 = PADS_BANK0_GPIO28_DRIVE(1);
	pads_bank0.gpio27 = PADS_BANK0_GPIO27_DRIVE(1);
	pads_bank0.gpio26 = PADS_BANK0_GPIO26_DRIVE(1);
	pads_bank0.gpio22 = PADS_BANK0_GPIO22_DRIVE(1);
	pads_bank0.gpio2 = PADS_BANK0_GPIO2_IE(1) | PADS_BANK0_GPIO2_PUE(1);
	pads_bank0.gpio3 = PADS_BANK0_GPIO3_IE(1) | PADS_BANK0_GPIO3_PUE(1);
	pads_bank0.gpio4 = PADS_BANK0_GPIO4_IE(1) | PADS_BANK0_GPIO4_PUE(1);

	io_bank0.gpio28_ctrl = IO_BANK0_GPIO28_CTRL_FUNCSEL(5);
	io_bank0.gpio27_ctrl = IO_BANK0_GPIO27_CTRL_FUNCSEL(5);
	io_bank0.gpio26_ctrl = IO_BANK0_GPIO26_CTRL_FUNCSEL(5);
	io_bank0.gpio22_ctrl = IO_BANK0_GPIO22_CTRL_FUNCSEL(5);
	io_bank0.gpio2_ctrl = IO_BANK0_GPIO2_CTRL_FUNCSEL(5);
	io_bank0.gpio3_ctrl = IO_BANK0_GPIO3_CTRL_FUNCSEL(5);
	io_bank0.gpio4_ctrl = IO_BANK0_GPIO4_CTRL_FUNCSEL(5);

	SIO_GPIO_OUT_CLR = ALL_ROWS_MASK;
	drive_all_rows();
}

void keypad_scan_fsm() {
	static enum { WAIT_PRESS, SCAN_ROWS, WAIT_RELEASE } state = WAIT_PRESS;
	static uint8_t scan_row;
	static uint8_t scan_tick_counter;

	if( ++scan_tick_counter < KEYPAD_SCAN_PERIOD_TICKS )
		return;
	scan_tick_counter = 0;

	uint8_t column = read_column();

	switch( state ) {
	case WAIT_PRESS:
		if( column != NO_COLUMN ) {
			scan_row = 0;
			drive_row( scan_row );
			state = SCAN_ROWS;
		}
		break;
	case SCAN_ROWS:
		if( column != NO_COLUMN ) {
			last_key = key_characters[ scan_row * NUM_COLUMNS + column ];
			drive_all_rows();
			state = WAIT_RELEASE;
		} else if( ++scan_row < NUM_ROWS ) {
			drive_row( scan_row );
		} else {
			drive_all_rows();
			state = WAIT_RELEASE;
		}
		break;
	case WAIT_RELEASE:
		if( column == NO_COLUMN )
			state = WAIT_PRESS;
		break;
	}
}

char get_keypad_key() {
	char retval;
	__disable_irq();
	retval = last_key;
	last_key = KEYPAD_NO_KEY;
	__enable_irq();
	return retval;
}

static void drive_row( uint8_t row ) {
	SIO_GPIO_OE_CLR = ALL_ROWS_MASK;
	SIO_GPIO_OE_SET = row_masks[row];
}

static void drive_all_rows() {
	SIO_GPIO_OE_SET = ALL_ROWS_MASK;
}

static uint8_t read_column() {
	uint32_t pin_levels = SIO_GPIO_IN;
	for( uint8_t column = 0; column < NUM_COLUMNS; column++ ) {
		if( (pin_levels & column_masks[column]) == 0 )
			return column;
	}
	return NO_COLUMN;
}
