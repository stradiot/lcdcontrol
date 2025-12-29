#ifndef LCDCONTROL_H
#define LCDCONTROL_H

#include <linux/ioctl.h>
#include <linux/types.h>

// --- The Data Structure (Shared) ---
// Defined here so both Userspace and Kernel know the layout
struct lcd_config {
    int display_on; // 1 = On, 0 = Off
    int cursor_on;  // 1 = Show Underline, 0 = Hide
    int blink_on;   // 1 = Blinking Block, 0 = Steady
};

// --- IOCTL Commands ---
#define LCD_MAGIC 'L'
#define LCD_IOCTL_CLEAR      _IO(LCD_MAGIC, 1)
#define LCD_IOCTL_SET_CONFIG _IOW(LCD_MAGIC, 2, struct lcd_config)

#endif
