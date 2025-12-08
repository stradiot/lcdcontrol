#ifndef HD44780_H
#define HD44780_H

#define GPIO_RS     25  // Register Select
#define GPIO_EN     23  // Enable
#define GPIO_D4     22  // Data 4
#define GPIO_D5     27  // Data 5
#define GPIO_D6     17  // Data 6
#define GPIO_D7     4   // Data 7

int hd44780_init(void);

void hd44780_release(void);

void led_blink(void);

#endif // HD44780_H
