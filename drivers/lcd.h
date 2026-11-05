#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>

void configure_lcd(void); 
void lcd_fsm(void); 
_Bool lcd_command(uint8_t command); 
_Bool lcd_putchar(char data); 

#endif
