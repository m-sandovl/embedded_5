/*
 * Author: Marc Sandoval
 * Purpose: Public interface for the FIFO-based LCD driver
 */
#ifndef LCD_H
#define LCD_H
 
#include <stdint.h>
#include <stdbool.h>
 
void configure_lcd(void);
void lcd_fsm(void);
bool lcd_command(uint8_t command);
bool lcd_putchar(char data);
uint8_t lcd_free_fifo_entries(void);
 
#endif
 
