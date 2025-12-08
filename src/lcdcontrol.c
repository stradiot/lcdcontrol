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

static int lcdcontrol_major;
static int lcdcontrol_minor;

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
	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;

	if (count > 1024) {
		return -EINVAL;
	}

	char *write_buffer;

	write_buffer = kzalloc(count + 1, GFP_KERNEL);

	if(copy_from_user(write_buffer, buff, count))
	{
		kfree(write_buffer);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&lcd_dev->lock))
		return -ERESTARTSYS;

	pr_debug("Received from user: %s\n", write_buffer);

	mutex_unlock(&lcd_dev->lock);

	kfree(write_buffer);
	return count;
}

static ssize_t lcdcontrol_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	pr_debug("Read: count %zu, f_pos %lld\n", count, *f_pos);
	struct lcdcontrol_dev *lcd_dev = (struct lcdcontrol_dev *) filp->private_data;

	if (mutex_lock_interruptible(&lcd_dev->lock))
		return -ERESTARTSYS;

	static const char msg[] = "LCDControl read\n";
	size_t msg_len = strlen(msg);
	size_t read_size = ((msg_len - *f_pos) > count) ? count : (msg_len - *f_pos);

	// Access my_dev->buffer ...

	mutex_unlock(&lcd_dev->lock);

	if (*f_pos >= msg_len) {
		return 0;
	}

	if (copy_to_user(buff, msg + *f_pos, read_size)){
		return -EINTR;
	}

	*f_pos += read_size;

	return read_size;
}

static struct file_operations lcdcontrol_fops = {
	.owner = THIS_MODULE,
	.open = lcdcontrol_open,
	.release = lcdcontrol_release,
	.read = lcdcontrol_read,
	.write = lcdcontrol_write,
	//.llseek = lcdcontrol_llseek,
	//.unlocked_ioctl = lcdcontrol_ioctl,
};

static int lcdcontrol_setup_cdev(struct lcdcontrol_dev *dev)
{
	int err, devno = MKDEV(lcdcontrol_major, lcdcontrol_minor);

	cdev_init(&dev->cdev, &lcdcontrol_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &lcdcontrol_fops;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		pr_err("Error %d adding lcdcontrol cdev\n", err);
	}

	return err;
}

static int lcdcontrol_setup_class(void)
{
	dev_t dev = MKDEV(lcdcontrol_major, lcdcontrol_minor);
	struct device *device_ret;

	/* Note: In newer kernels (6.4+), class_create takes only 1 argument.
	 * If you are on < 6.4, use: class_create(THIS_MODULE, "lcdcontrol");
	 */
	lcdcontrol_class = class_create("lcdcontrol");
	if (IS_ERR(lcdcontrol_class)) {
		pr_err("Failed to create class\n");
		return PTR_ERR(lcdcontrol_class);
	}

	device_ret = device_create(lcdcontrol_class, NULL, dev, NULL, "lcdcontrol");
	if (IS_ERR(device_ret)) {
		pr_err("Failed to create device\n");
		class_destroy(lcdcontrol_class);
		return PTR_ERR(device_ret);
	}

	return 0;
}

static int __init lcdcontrol_init(void)
{
	dev_t dev = 0;
	int result;

	result = alloc_chrdev_region(&dev, lcdcontrol_minor, 1, "lcdcontrol");
	lcdcontrol_major = MAJOR(dev);
	if (result < 0) {
		pr_warn("Can't get major %d\n", lcdcontrol_major);
		return result;
	}

	pr_info("Major=%d, Minor=%d\n", MAJOR(dev), MINOR(dev));

	mutex_init(&lcdcontrol_device.lock);

	result = lcdcontrol_setup_cdev(&lcdcontrol_device);
	if (result) {
		pr_warn("cdev setup failed\n");
		goto err_unregister;
	}

	pr_info("cdev setup successful\n");

	result = lcdcontrol_setup_class();
	if (result) {
		pr_warn("Failed to create device file\n");
		goto err_del_cdev;
	}

	pr_info("Class %s created\n", lcdcontrol_class->name);

	return 0;

err_del_cdev:
	cdev_del(&lcdcontrol_device.cdev);
err_unregister:
	unregister_chrdev_region(dev, 1);
	return result;
}

static void __exit lcdcontrol_exit(void)
{
	dev_t dev = MKDEV(lcdcontrol_major, lcdcontrol_minor);

	pr_info("Cleaning the driver artifacts\n");

	device_destroy(lcdcontrol_class, dev);
	cdev_del(&lcdcontrol_device.cdev);
	class_destroy(lcdcontrol_class);
	unregister_chrdev_region(dev, 1);
}

module_init(lcdcontrol_init);
module_exit(lcdcontrol_exit);
