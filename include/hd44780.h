#ifndef HD44780_H
#define HD44780_H

#define GPIO_BASE  512

// Control Pins
#define GPIO_RS     (GPIO_BASE + 25)  // Physical Pin 22
#define GPIO_EN     (GPIO_BASE + 24)  // Physical Pin 18

// Data Pins (4-bit mode)
#define GPIO_D4     (GPIO_BASE + 23)  // Physical Pin 16
#define GPIO_D5     (GPIO_BASE + 17)  // Physical Pin 11
#define GPIO_D6     (GPIO_BASE + 18)  // Physical Pin 12
#define GPIO_D7     (GPIO_BASE + 22)  // Physical Pin 15

#define LCD_SEND_DATA 1
#define LCD_SEND_CMD  0

int hd44780_init(void);
void hd44780_release(void);
void lcd_send_byte(unsigned char data, int mode);
void lcd_clear(void);
void lcd_set_cursor_row(int row);

#endif // HD44780_H
