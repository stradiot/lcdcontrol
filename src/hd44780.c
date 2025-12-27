#define DEBUG
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "hd44780.h"

static int lcd_pins[] = {
    GPIO_RS,
    GPIO_EN,
    GPIO_D4,
    GPIO_D5,
    GPIO_D6,
    GPIO_D7
};

int hd44780_init(void)
{
	int i, result;
	char label[20];

	pr_info("HD44780: Initializing GPIO hardware...\n");

	for (i = 0; i < ARRAY_SIZE(lcd_pins); i++)
	{
		snprintf(label, sizeof(label), "lcd_pin_%d", lcd_pins[i]);

		result = gpio_request(lcd_pins[i], label);
		if (result) {
		    pr_err("HD44780: Failed to request GPIO %d (Error %d)\n", lcd_pins[i], result);
		    // Cleanup: Free any pins that were already successfully requested
		    while (i > 0) {
			i--;
			gpio_free(lcd_pins[i]);
		    }
		    return result;
		}

		result = gpio_direction_output(lcd_pins[i], 0);
		if (result) {
		    pr_err("HD44780: Failed to set GPIO %d to output\n", lcd_pins[i]);
		    gpio_free(lcd_pins[i]);
		    return result;
		}
	}

	return 0;
}

void hd44780_release(void)
{
    for (int i = 0; i < ARRAY_SIZE(lcd_pins); i++) {
        gpio_set_value(lcd_pins[i], 0);
        gpio_free(lcd_pins[i]);
    }
    pr_info("HD44780: GPIO hardware freed.\n");
}

void led_blink(void)
{
    int i;
    pr_info("HD44780: Running Blink Test...\n");

    for (i = 0; i < 3; i++) {
        gpio_set_value(GPIO_EN, 1);
        gpio_set_value(GPIO_RS, 1);
        msleep(500); // 500ms ON

        gpio_set_value(GPIO_EN, 0);
        gpio_set_value(GPIO_RS, 0);
        msleep(500); // 500ms OFF
    }
    
    pr_info("HD44780: Blink Test Done.\n");
}
