#define DEBUG
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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

static dev_t lcdcontrol_dev_num;
static struct class *lcdcontrol_class;

MODULE_AUTHOR("Martin Stradiot");
MODULE_LICENSE("Dual BSD/GPL");

struct lcdcontrol_dev {
	struct mutex lock;
	struct cdev cdev;
};

static struct lcdcontrol_dev lcdcontrol_device;

static int lcdcontrol_open(struct inode *inode, struct file *filp)
{
	pr_info("Device opened\n");

	filp->private_data = container_of(inode->i_cdev, struct lcdcontrol_dev, cdev);

	return 0;
}

static int lcdcontrol_release(struct inode *inode, struct file *filp)
{
	pr_info("Device released\n");
	return 0;
}

static ssize_t lcdcontrol_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
	pr_debug("Write: count %zu, f_pos %lld\n", count, *f_pos);
	ssize_t result = count;

	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;

	if (count > 1024) {
		result = -EINVAL;
		goto out;
	}

	char *write_buffer = kzalloc(count + 1, GFP_KERNEL);

	if (!write_buffer) {
		result = -ENOMEM;
		goto out;
	}

	if(copy_from_user(write_buffer, buff, count)) {
		result = -EFAULT;
		goto buffer_free;
	}

	if (mutex_lock_interruptible(&lcd_dev->lock)) {
		result = -ERESTARTSYS;
		goto buffer_free;
	}

	pr_debug("Received from user: %s\n", write_buffer);

	mutex_unlock(&lcd_dev->lock);

buffer_free:
	kfree(write_buffer);
out:
	return result;
}

static ssize_t lcdcontrol_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	pr_debug("Read: count %zu, f_pos %lld\n", count, *f_pos);
	int result;
	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;

	if (mutex_lock_interruptible(&lcd_dev->lock))
		return -ERESTARTSYS;

	static const char msg[] = "LCDControl read\n";
	size_t msg_len = strlen(msg);
	size_t read_size = ((msg_len - *f_pos) > count) ? count : (msg_len - *f_pos);

	// Access my_dev->buffer ...

	if (*f_pos >= msg_len) {
		result = 0;
		goto mutex_unlock;
	}

	mutex_unlock(&lcd_dev->lock);

	if (copy_to_user(buff, msg + *f_pos, read_size)){
		result = -EFAULT;
		goto out;
	}

	*f_pos += read_size;
	result = read_size;

	goto out;

mutex_unlock:
	mutex_unlock(&lcd_dev->lock);
out:
	return result;
}

static struct file_operations lcdcontrol_fops = {
	.owner = THIS_MODULE,
	.open = lcdcontrol_open,
	.release = lcdcontrol_release,
	.read = lcdcontrol_read,
	.write = lcdcontrol_write,
	.llseek = noop_llseek,
	//.unlocked_ioctl = lcdcontrol_ioctl,
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
