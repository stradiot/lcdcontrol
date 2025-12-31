#ifndef LCDCONTROL_H
#define LCDCONTROL_H

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * struct lcd_config - Configuration options for the LCD
 * @display_on: 1 = On, 0 = Off
 * @cursor_on:  1 = Show Underline, 0 = Hide
 * @blink_on:   1 = Blinking Block, 0 = Steady
 *
 * This structure is shared between userspace and the kernel.
 * We use __s32 to ensure consistent size (32-bit signed integer)
 * across different architectures.
 */
struct lcd_config {
	__s32 display_on;
	__s32 cursor_on;
	__s32 blink_on;
};

/* --- IOCTL Commands --- */
#define LCD_MAGIC 'L'

/* Clears the display and resets cursor */
#define LCD_IOCTL_CLEAR			_IO(LCD_MAGIC, 1)

/* Sets the display configuration flags */
#define LCD_IOCTL_SET_CONFIG		_IOW(LCD_MAGIC, 2, struct lcd_config)

#endif
