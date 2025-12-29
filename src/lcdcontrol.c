#define DEBUG
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MAX_BUFFER_SIZE 1024

#define LCD_SCREEN_SIZE 32
#define LCD_LINE_SIZE 16
#define LCD_ROWS_COUNT 2

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "hd44780.h"
#include "lcdcontrol.h"

static dev_t lcdcontrol_dev_num;
static struct class *lcdcontrol_class;

MODULE_AUTHOR("Martin Stradiot");
MODULE_LICENSE("Dual BSD/GPL");

struct lcdcontrol_dev {
	struct mutex lock;
	struct cdev cdev;

	char screen_buffer[LCD_SCREEN_SIZE];
	char line_buffer[LCD_LINE_SIZE];

	int line_pos;
};

static struct lcdcontrol_dev lcdcontrol_device;

static int lcdcontrol_open(struct inode *inode, struct file *filp)
{
	pr_info("Device opened\n");

	struct lcdcontrol_dev *lcd_dev = container_of(inode->i_cdev, struct lcdcontrol_dev, cdev);
	filp->private_data = lcd_dev;

	return 0;
}

static int lcdcontrol_release(struct inode *inode, struct file *filp)
{
	pr_info("Device released\n");
	return 0;
}

static bool is_valid_char(char c)
{
	return (c >= 0x20 && c <= 0x7E);
}

static void lcd_refresh_screen(struct lcdcontrol_dev *lcd_dev)
{
    int i;
    
    // 1. Write Top Line (From Shadow Index 0-15)
    lcd_set_cursor_row(0);
    for (i = 0; i < LCD_LINE_SIZE; i++) {
        lcd_send_byte(lcd_dev->screen_buffer[i], LCD_SEND_DATA);
    }
    
    // 2. Write Bottom Line (From Shadow Index 16-31)
    lcd_set_cursor_row(1);
    for (i = 0; i < LCD_LINE_SIZE; i++) {
        lcd_send_byte(lcd_dev->screen_buffer[LCD_LINE_SIZE + i], LCD_SEND_DATA);
    }
}

static void write_line_to_lcd(struct lcdcontrol_dev *lcd_dev)
{
    // 1. SCROLL: Move Line 2 (bottom) to Line 1 (top)
    // We copy 16 bytes from offset 16 to offset 0
    memmove(lcd_dev->screen_buffer, lcd_dev->screen_buffer + LCD_LINE_SIZE, LCD_LINE_SIZE);
    
    // 2. UPDATE: Write the new line to Line 2 (bottom)
    // We loop through all 16 slots to ensure we overwrite old data (Padding)
    for (int i = 0; i < LCD_LINE_SIZE; i++) {
        if (i < lcd_dev->line_pos) {
            // Write data
            lcd_dev->screen_buffer[LCD_LINE_SIZE + i] = lcd_dev->line_buffer[i];
        } else {
            // Write padding (space)
            lcd_dev->screen_buffer[LCD_LINE_SIZE + i] = ' ';
        }
    }
    
    // 3. SYNC: Push the updated memory to the display
    lcd_refresh_screen(lcd_dev);
    
    // 4. RESET: Prepare for the next log line
    lcd_dev->line_pos = 0;
}

static ssize_t lcdcontrol_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
	pr_debug("Write: count %zu, f_pos %lld\n", count, *f_pos);

	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;

	ssize_t copy_count = (count > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : count;

	char *write_buffer = kzalloc(copy_count + 1, GFP_KERNEL);

	if (!write_buffer)
		return -ENOMEM;

	if(copy_from_user(write_buffer, buff, copy_count)) {
		kfree(write_buffer);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&lcd_dev->lock)) {
		kfree(write_buffer);
		return -ERESTARTSYS;
	}

	pr_debug("Received from user: %s\n", write_buffer);

	// Process each character and update LCD
	for (size_t i = 0; i < copy_count; i++) {
		if (lcd_dev->line_pos >= LCD_LINE_SIZE) {
			char *next_newline = memchr(write_buffer + i, '\n', copy_count - i);
			if (!next_newline)
				break;

			i = next_newline - write_buffer;
		}

		char c = write_buffer[i];

		if (c == '\n') {
			// Write the current line to the LCD
			write_line_to_lcd(lcd_dev);
			continue;
		}

		if (!is_valid_char(c)) {
			pr_debug("Ignoring invalid character: 0x%02X\n", c);
			continue;
		}

		// Place character in line buffer
		lcd_dev->line_buffer[lcd_dev->line_pos] = c;
		lcd_dev->line_pos++;

	}

	mutex_unlock(&lcd_dev->lock);

	kfree(write_buffer);

	// Return full count as the whole buffer is processed
	return count;
}

static ssize_t lcdcontrol_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	pr_debug("Read: count %zu, f_pos %lld\n", count, *f_pos);

	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;

	if (*f_pos >= LCD_SCREEN_SIZE)
		return 0;

	if (mutex_lock_interruptible(&lcd_dev->lock))
		return -ERESTARTSYS;

	char msg[LCD_SCREEN_SIZE];
	memcpy(msg, lcd_dev->screen_buffer, LCD_SCREEN_SIZE);
	size_t read_size = (count < (LCD_SCREEN_SIZE - *f_pos)) ? count : (LCD_SCREEN_SIZE - *f_pos);

	mutex_unlock(&lcd_dev->lock);

	if (copy_to_user(buff, msg + *f_pos, read_size)){
		return -EFAULT;
	}

	*f_pos += read_size;

	return read_size;
}

static long lcdcontrol_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;
	struct lcd_config config;

	// Use a mutex to prevent race conditions during configuration
	if (mutex_lock_interruptible(&lcd_dev->lock))
		return -ERESTARTSYS;

	switch (cmd) {
		case LCD_IOCTL_CLEAR:
			memset(lcd_dev->screen_buffer, ' ', LCD_SCREEN_SIZE);
			memset(lcd_dev->line_buffer, 0, LCD_LINE_SIZE);
			lcd_dev->line_pos = 0;
			
			lcd_clear();
			break;
		case LCD_IOCTL_SET_CONFIG:
			if (copy_from_user(&config, (struct lcd_config __user *)arg, sizeof(config))) {
				mutex_unlock(&lcd_dev->lock);
				return -EFAULT;
			}

			lcd_configure(&config);
			break;
		default:
			mutex_unlock(&lcd_dev->lock);
			return -ENOTTY; // Error: "Inappropriate ioctl for device"
	}

	mutex_unlock(&lcd_dev->lock);

	return 0;
}

static struct file_operations lcdcontrol_fops = {
	.owner = THIS_MODULE,
	.open = lcdcontrol_open,
	.release = lcdcontrol_release,
	.read = lcdcontrol_read,
	.write = lcdcontrol_write,
	.llseek = noop_llseek,
	.unlocked_ioctl = lcdcontrol_ioctl
};

static int lcdcontrol_setup_cdev(struct lcdcontrol_dev *dev)
{
	int err;

	cdev_init(&dev->cdev, &lcdcontrol_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &lcdcontrol_fops;

	err = cdev_add(&dev->cdev, lcdcontrol_dev_num, 1);
	if (err) {
		pr_err("Error %d adding lcdcontrol cdev\n", err);
	}

	return err;
}

static int lcdcontrol_setup_class(void)
{
	struct device *dev;

	/* Note: In newer kernels (6.4+), class_create takes only 1 argument.
	 * If you are on < 6.4, use: class_create(THIS_MODULE, "lcdcontrol");
	 */
	lcdcontrol_class = class_create("lcdcontrol");
	if (IS_ERR(lcdcontrol_class)) {
		pr_err("Failed to create class\n");
		return PTR_ERR(lcdcontrol_class);
	}

	dev = device_create(lcdcontrol_class, NULL, lcdcontrol_dev_num, NULL, "lcdcontrol");
	if (IS_ERR(dev)) {
		pr_err("Failed to create device\n");
		class_destroy(lcdcontrol_class);
		return PTR_ERR(dev);
	}

	return 0;
}

static int __init lcdcontrol_init(void)
{
	// Initialize the HD44780 LCD
	int result = hd44780_init();
	if (result) {
		pr_err("HD44780 initialization failed\n");
		return result;
	}
	pr_info("HD44780 initialized\n");

	result = alloc_chrdev_region(&lcdcontrol_dev_num, 0, 1, "lcdcontrol");
	if (result < 0) {
		pr_err("Can't get major %d\n", MAJOR(lcdcontrol_dev_num));
		goto err_release_hd44780;
	}

	pr_info("Major=%d, Minor=%d\n", MAJOR(lcdcontrol_dev_num), MINOR(lcdcontrol_dev_num));

	mutex_init(&lcdcontrol_device.lock);

	result = lcdcontrol_setup_cdev(&lcdcontrol_device);
	if (result) {
		pr_err("cdev setup failed\n");
		goto err_unregister;
	}

	pr_info("cdev setup successful\n");

	result = lcdcontrol_setup_class();
	if (result) {
		pr_err("Failed to create device file\n");
		goto err_del_cdev;
	}

	pr_info("Class %s created\n", lcdcontrol_class->name);

	// Initialize screen and line buffers and clear the display
	memset(lcdcontrol_device.screen_buffer, ' ', LCD_SCREEN_SIZE);
	memset(lcdcontrol_device.line_buffer, 0, LCD_LINE_SIZE);
	lcdcontrol_device.line_pos = 0;

	lcd_clear();

	pr_info("LCD initialized\n");

	return 0;

err_del_cdev:
	cdev_del(&lcdcontrol_device.cdev);
err_unregister:
	unregister_chrdev_region(lcdcontrol_dev_num, 1);
err_release_hd44780:
	hd44780_release();

	return result;
}

static void __exit lcdcontrol_exit(void)
{
	pr_info("Cleaning the driver artifacts\n");

	// Release the HD44780 LCD
	hd44780_release();

	device_destroy(lcdcontrol_class, lcdcontrol_dev_num);
	cdev_del(&lcdcontrol_device.cdev);
	class_destroy(lcdcontrol_class);
	unregister_chrdev_region(lcdcontrol_dev_num, 1);
}

module_init(lcdcontrol_init);
module_exit(lcdcontrol_exit);
