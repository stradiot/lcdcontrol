#define DEBUG
#define pr_fmt(fmt) KBUILD_MODNAME " [HD44780]: " fmt

// --- Command Constants ---
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x04
#define LCD_CMD_DISPLAY_CTRL    0x08
#define LCD_CMD_SHIFT           0x10
#define LCD_CMD_FUNCTION_SET    0x20

// --- Timing Constants ---
// Enable pulse width must be > 450ns. We use 1us to be safe.
#define PULSE_DELAY_US          1
// Most commands need > 37us to process.
#define CMD_DELAY_US            50

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

static int rpi_gpio_init(void)
{
	int i, result;
	char label[20];

	pr_info("Initializing the RPi GPIO hardware...\n");

	for (i = 0; i < ARRAY_SIZE(lcd_pins); i++)
	{
		snprintf(label, sizeof(label), "lcd_pin_%d", lcd_pins[i]);

		result = gpio_request(lcd_pins[i], label);
		if (result) {
		    pr_err("Failed to request GPIO %d (Error %d)\n", lcd_pins[i], result);
		    // Cleanup: Free any pins that were already successfully requested
		    while (i > 0) {
                i--;
                gpio_free(lcd_pins[i]);
		    }
		    return result;
		}

		result = gpio_direction_output(lcd_pins[i], 0);
		if (result) {
		    pr_err("Failed to set GPIO %d to output\n", lcd_pins[i]);
		    gpio_free(lcd_pins[i]);
		    return result;
		}
	}

	pr_info("RPi GPIO hardware initialized\n");

	return 0;
}

static void rpi_gpio_release(void)
{
    for (int i = 0; i < ARRAY_SIZE(lcd_pins); i++) {
        gpio_set_value(lcd_pins[i], 0);
        gpio_free(lcd_pins[i]);
    }
    pr_info("RPi GPIO hardware freed.\n");
}

static void lcd_pulse_enable(void)
{
	pr_debug("Generating Enable Pulse\n");
    gpio_set_value(GPIO_EN, 1);
    udelay(PULSE_DELAY_US);
    gpio_set_value(GPIO_EN, 0);
    udelay(CMD_DELAY_US);
}

/* * Writes the lower 4 bits of 'data' to the GPIO pins D4-D7.
 * Does NOT pulse Enable (allows us to stage data).
 */
static void lcd_write_nibble(unsigned char nibble)
{
	pr_debug("Writing nibble: 0x%02X\n", nibble);

    gpio_set_value(GPIO_D4, (nibble >> 0) & 0x01);
    gpio_set_value(GPIO_D5, (nibble >> 1) & 0x01);
    gpio_set_value(GPIO_D6, (nibble >> 2) & 0x01);
    gpio_set_value(GPIO_D7, (nibble >> 3) & 0x01);
}

/*
 * Sends a full byte to the LCD in 4-bit mode.
 * 1. Sends High Nibble -> Pulse
 * 2. Sends Low Nibble -> Pulse
 * * mode: 0 for Command (RS=0), 1 for Data (RS=1)
 */
void lcd_send_byte(unsigned char data, int mode)
{
    if (mode == LCD_SEND_CMD)
        pr_debug("Writing command byte: 0x%02X\n", data);
    else
        pr_debug("Writing data byte: 0x%02X (%c)\n", data, data);

    gpio_set_value(GPIO_RS, mode);

    // High Nibble (Upper 4 bits)
    lcd_write_nibble(data >> 4);
    lcd_pulse_enable();

    // Low Nibble (Lower 4 bits)
    lcd_write_nibble(data);
    lcd_pulse_enable();
    
    // Extra delay for slow commands (Clear & Home take ~2ms)
    if ((mode == LCD_SEND_CMD) && (data == LCD_CMD_CLEAR || data == LCD_CMD_HOME)) {
        pr_debug("Delaying for slow command\n");
        msleep(2);
    }
}

int hd44780_init(void)
{
	// Initialize the Raspberry Pi GPIO hardware
    int result = rpi_gpio_init();
	if (result) {
		pr_err("GPIO init failed with error %d\n", result);
        return result;
	}

    pr_info("Initializing the HD44780 display...\n");

    // 1. Wait >15ms after VCC rises to 4.5V
    msleep(20);

    // --- Soft Reset Sequence (The "Magic Ritual") ---
    // In this phase, we cannot use lcd_send_byte because the LCD
    // might still think it is in 8-bit mode. We manually send nibbles.

    gpio_set_value(GPIO_RS, LCD_SEND_CMD); // Command Mode

    // Step 1: Send 0x03
    lcd_write_nibble(0x03);
    lcd_pulse_enable();
    msleep(5); // Wait >4.1ms

    // Step 2: Send 0x03 again
    lcd_write_nibble(0x03);
    lcd_pulse_enable();
    udelay(150); // Wait >100us

    // Step 3: Send 0x03 again
    lcd_write_nibble(0x03);
    lcd_pulse_enable();
    udelay(150);

    // Step 4: Switch to 4-bit mode
    // We send 0x02. The LCD interprets this as "Configure 4-bit interface".
    lcd_write_nibble(0x02);
    lcd_pulse_enable();
    udelay(150);

    // --- Configuration (Now we can use lcd_send_byte) ---

    // Function Set: 4-bit mode, 2 lines, 5x8 font
    // 0x20 | 0x08 (2 lines) | 0x00 (5x8 dots) = 0x28
    lcd_send_byte(0x28, LCD_SEND_CMD);

    // Display Control: Display OFF, Cursor OFF, Blink OFF
    lcd_send_byte(0x08, LCD_SEND_CMD);

    // Clear Display
    lcd_send_byte(0x01, LCD_SEND_CMD);

    // Entry Mode Set: Increment cursor, No shift
    // 0x04 | 0x02 (Increment) = 0x06
    lcd_send_byte(0x06, LCD_SEND_CMD);

    // Display Control: Display ON, Cursor OFF, Blink OFF
    // 0x08 | 0x04 (Display On) = 0x0C
    lcd_send_byte(0x0C, LCD_SEND_CMD);

    pr_info("HD44780: Initialization Complete.\n");

    return 0;
}

void lcd_clear(void)
{
    pr_debug("Clearing the LCD display...\n");
    lcd_send_byte(LCD_CMD_CLEAR, LCD_SEND_CMD);
}

void lcd_set_cursor_row(int row)
{
    unsigned char address;

    switch (row) {
        case 0:
            address = 0x00;
            break;
        case 1:
            address = 0x40;
            break;
        default:
            pr_warn("Invalid row %d, defaulting to row 0\n", row);
            address = 0x00;
            break;
    }

    lcd_send_byte(0x80 | address, LCD_SEND_CMD); // Set DDRAM address command
}

/*
 * Configures the Display Control Register (0x08)
 * Bit 2: Display (D)
 * Bit 1: Cursor (C)
 * Bit 0: Blink (B)
 */
void lcd_configure(struct lcd_config *cfg)
{
    unsigned char cmd_byte = 0x08; // Base command for Display Control

    if (cfg->display_on) cmd_byte |= 0x04;
    if (cfg->cursor_on)  cmd_byte |= 0x02;
    if (cfg->blink_on)   cmd_byte |= 0x01;

    pr_debug("Applying Config: Display=%d, Cursor=%d, Blink=%d (Cmd: 0x%02X)\n",
             cfg->display_on, cfg->cursor_on, cfg->blink_on, cmd_byte);

    lcd_send_byte(cmd_byte, LCD_SEND_CMD);
}

void hd44780_release(void)
{
    pr_info("Releasing the HD44780 display...\n");

    // 1. Clear the display
    lcd_clear();
    // 2. Turn Display OFF (Hides cursor and text, keeps RAM)
    // Command 0x08 = Display Off, Cursor Off, Blink Off
    lcd_send_byte(LCD_CMD_DISPLAY_CTRL, LCD_SEND_CMD);

    // 3. NOW it is safe to free the GPIOs
    // (This calls your existing release function)
    rpi_gpio_release();
}

