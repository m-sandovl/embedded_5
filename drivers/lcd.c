/*
 * Author: Marc Sandoval
 * Purpose: FIFO-based low-level driver for the TC1602A-09T LCD in 4-bit mode.
 * Callers queue commands and characters with lcd_command()/lcd_putchar();
 * lcd_fsm() is registered as a systick callback (1 us tick) and drains the FIFO.
 */
#include "lcd.h"
#include "interrupt.h"
#include <rp2350/resets.h>
#include <rp2350/sio.h>
#include <rp2350/io_bank0.h>
#include <rp2350/pads_bank0.h>
#include <stdint.h>
#include <stdbool.h>

#define DB4_LOC 18
#define DB5_LOC 19
#define DB6_LOC 20
#define DB7_LOC 21
#define E_LOC   14
#define RS_LOC  15

#define DELAY_BOOT         41000
#define DELAY_WAIT_4_1_MS  4100
#define DELAY_WAIT_100_US  100
#define DELAY_TSP1         1
#define DELAY_TPW          1
#define DELAY_T_CYCLE      1
#define LCD_DELAY_SHORT    38
#define LCD_DELAY_LONG     1520
#define LCD_SHORT_COMMAND_MASK 0xFC
#define LCD_NIBBLE_MASK    0xF
#define LCD_NIBBLE_BITS    4
#define LCD_FUNCTION_SET_8BIT_NIBBLE 0x3

#define CONCAT2(x,y) x##y
#define CONCAT3(x,y,z) x##y##z
#define PADS_BANK0_GPIO(x) CONCAT2(PADS_BANK0_GPIO,x)
#define IO_BANK0_GPIO_CTRL(x) CONCAT3(IO_BANK0_GPIO,x,_CTRL)
#define LCD_RESETS ( RESETS_RESET_IO_BANK0_MASK | RESETS_RESET_PADS_BANK0_MASK)

#define OUTPUT_PAD_CONFIG ( \
        PADS_BANK0_GPIO0_OD(0) | PADS_BANK0_GPIO0_IE(0) | PADS_BANK0_GPIO0_DRIVE(0) \
      | PADS_BANK0_GPIO0_PUE(0) | PADS_BANK0_GPIO0_PDE(0) | PADS_BANK0_GPIO0_SCHMITT(0) \
      | PADS_BANK0_GPIO0_SLEWFAST(0) )
#define OUTPUT_GPIO_CTRL_CONFIG ( \
        IO_BANK0_GPIO0_CTRL_IRQOVER(0) | IO_BANK0_GPIO0_CTRL_INOVER(0) \
      | IO_BANK0_GPIO0_CTRL_OEOVER(0) | IO_BANK0_GPIO0_CTRL_OUTOVER(0) \
      | IO_BANK0_GPIO0_CTRL_FUNCSEL(5) )

#ifndef LCD_FIFO_SIZE
#define LCD_FIFO_SIZE 40
#endif

static void lcd_output_nibble(uint8_t data);
static void lcd_set_e(void);
static void lcd_clr_e(void);
static void lcd_set_rs(void);
static void lcd_clr_rs(void);
static uint8_t next_head(void);
static uint8_t next_tail(void);
static bool lcd_fifo_empty(void);
static bool lcd_fifo_full(void);
static bool lcd_command_or_data(bool is_cmd, uint8_t value, uint16_t delay);

static struct {
	bool is_cmd;
	uint8_t value;
	uint16_t delay;
} lcd_fifo[LCD_FIFO_SIZE];

static volatile uint8_t head, tail;

void configure_lcd(void) {
	RESETS_RESET_CLR = LCD_RESETS;
	while ((RESETS_RESET_DONE & LCD_RESETS) != LCD_RESETS)
		continue;

	PADS_BANK0_GPIO(E_LOC)   = OUTPUT_PAD_CONFIG;
	PADS_BANK0_GPIO(DB4_LOC) = OUTPUT_PAD_CONFIG;
	PADS_BANK0_GPIO(DB5_LOC) = OUTPUT_PAD_CONFIG;
	PADS_BANK0_GPIO(DB6_LOC) = OUTPUT_PAD_CONFIG;
	PADS_BANK0_GPIO(DB7_LOC) = OUTPUT_PAD_CONFIG;
	PADS_BANK0_GPIO(RS_LOC)  = OUTPUT_PAD_CONFIG;
	IO_BANK0_GPIO_CTRL(RS_LOC)  = OUTPUT_GPIO_CTRL_CONFIG;
	IO_BANK0_GPIO_CTRL(E_LOC)   = OUTPUT_GPIO_CTRL_CONFIG;
	IO_BANK0_GPIO_CTRL(DB4_LOC) = OUTPUT_GPIO_CTRL_CONFIG;
	IO_BANK0_GPIO_CTRL(DB5_LOC) = OUTPUT_GPIO_CTRL_CONFIG;
	IO_BANK0_GPIO_CTRL(DB6_LOC) = OUTPUT_GPIO_CTRL_CONFIG;
	IO_BANK0_GPIO_CTRL(DB7_LOC) = OUTPUT_GPIO_CTRL_CONFIG;

	SIO_GPIO_OUT_CLR = (LCD_NIBBLE_MASK << DB4_LOC) | (1 << RS_LOC) | (1 << E_LOC);
	SIO_GPIO_OE_SET  = (LCD_NIBBLE_MASK << DB4_LOC) | (1 << RS_LOC) | (1 << E_LOC);
}

bool lcd_command(uint8_t command) {
	return lcd_command_or_data(true, command,
			(command & LCD_SHORT_COMMAND_MASK) ? LCD_DELAY_SHORT : LCD_DELAY_LONG);
}

bool lcd_putchar(char data) {
	return lcd_command_or_data(false, (uint8_t)data, LCD_DELAY_SHORT);
}

uint8_t lcd_free_fifo_entries(void) {
	uint8_t current_head = head;
	uint8_t current_tail = tail;
	uint8_t used_entries = (current_head >= current_tail)
		? current_head - current_tail
		: LCD_FIFO_SIZE - current_tail + current_head;
	return LCD_FIFO_SIZE - 1 - used_entries;
}

void lcd_fsm(void) {
	static enum { RESET1, RESET2, RESET3, RESET4, IDLE,
	              UPPER_BITS, CLR_E_1, LOWER_BITS, CLR_E_2 } state = RESET1;
	static uint16_t fsm_delay = DELAY_BOOT;
	static uint8_t data;
	static uint16_t cmd_delay;

	if (fsm_delay) {
		fsm_delay--;
		return;
	}

	switch (state) {
	case RESET1:
		lcd_output_nibble(LCD_FUNCTION_SET_8BIT_NIBBLE); lcd_set_e();
		fsm_delay = DELAY_TPW; state = RESET2; break;
	case RESET2:
		lcd_clr_e();
		fsm_delay = (DELAY_T_CYCLE - DELAY_TPW) + DELAY_WAIT_4_1_MS; state = RESET3; break;
	case RESET3:
		lcd_set_e();
		fsm_delay = DELAY_TPW; state = RESET4; break;
	case RESET4:
		lcd_clr_e();
		fsm_delay = (DELAY_T_CYCLE - DELAY_TPW) + DELAY_WAIT_100_US; state = IDLE; break;
	case IDLE:
		if (!lcd_fifo_empty()) {
			data = lcd_fifo[tail].value;
			if (lcd_fifo[tail].is_cmd)
				lcd_clr_rs();
			else
				lcd_set_rs();
			cmd_delay = lcd_fifo[tail].delay;
			tail = next_tail();
			fsm_delay = DELAY_TSP1;
			state = UPPER_BITS;
		}
		break;
	case UPPER_BITS:
		lcd_output_nibble(data >> LCD_NIBBLE_BITS); lcd_set_e();
		fsm_delay = DELAY_TPW; state = CLR_E_1; break;
	case CLR_E_1:
		lcd_clr_e();
		fsm_delay = (DELAY_T_CYCLE - DELAY_TPW); state = LOWER_BITS; break;
	case LOWER_BITS:
		lcd_output_nibble(data); lcd_set_e();
		fsm_delay = DELAY_TPW; state = CLR_E_2; break;
	case CLR_E_2:
		lcd_clr_e();
		fsm_delay = (DELAY_T_CYCLE - DELAY_TPW) + cmd_delay; state = IDLE; break;
	}
}

static void lcd_output_nibble(uint8_t data) {
	SIO_GPIO_OUT = (SIO_GPIO_OUT & ~(LCD_NIBBLE_MASK << DB4_LOC)) | ((data & LCD_NIBBLE_MASK) << DB4_LOC);
}

static void lcd_set_e(void)  { SIO_GPIO_OUT_SET = (1 << E_LOC); }
static void lcd_clr_e(void)  { SIO_GPIO_OUT_CLR = (1 << E_LOC); }
static void lcd_set_rs(void) { SIO_GPIO_OUT_SET = (1 << RS_LOC); }
static void lcd_clr_rs(void) { SIO_GPIO_OUT_CLR = (1 << RS_LOC); }

static uint8_t next_head(void) { return (head + 1 == LCD_FIFO_SIZE) ? 0 : head + 1; }
static uint8_t next_tail(void) { return (tail + 1 == LCD_FIFO_SIZE) ? 0 : tail + 1; }
static bool lcd_fifo_empty(void) { return head == tail; }
static bool lcd_fifo_full(void)  { return next_head() == tail; }

static bool lcd_command_or_data(bool is_cmd, uint8_t value, uint16_t delay) {
	bool queued = false;
	uint32_t primask;
	__get_primask(&primask);
	__disable_irq();
	if (!lcd_fifo_full()) {
		lcd_fifo[head].is_cmd = is_cmd;
		lcd_fifo[head].value  = value;
		lcd_fifo[head].delay  = delay;
		head = next_head();
		queued = true;
	}
	__set_primask(primask);
	return queued;
}
