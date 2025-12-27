#ifndef HD44780_H
#define HD44780_H

#define GPIO_BASE  512

#define GPIO_RS     (GPIO_BASE + 25)  // Register Select
#define GPIO_EN     (GPIO_BASE + 23)  // Enable
#define GPIO_D4     (GPIO_BASE + 22)  // Data 4
#define GPIO_D5     (GPIO_BASE + 27)  // Data 5
#define GPIO_D6     (GPIO_BASE + 17)  // Data 6
#define GPIO_D7     (GPIO_BASE + 4 )  // Data 7

int hd44780_init(void);

void hd44780_release(void);

void led_blink(void);

#endif // HD44780_H
