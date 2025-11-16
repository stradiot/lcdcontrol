#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

int lcdcontrol_major = 0;
int lcdcontrol_minor = 0;

static struct class *lcdcontrol_class;

MODULE_AUTHOR("Martin Stradiot");
MODULE_LICENSE("Dual BSD/GPL");

struct lcdcontrol_dev{
	struct mutex lock;
    struct cdev cdev;
} lcdcontrol_device;

struct file_operations lcdcontrol_fops = {
    .owner =    THIS_MODULE,
//    .read =     aesd_read,
//    .write =    aesd_write,
//	.llseek =	aesd_llseek,
//    .open =     aesd_open,
//    .release =  aesd_release,
//	.unlocked_ioctl = aesd_ioctl,
};

static int lcdcontrol_setup_cdev(struct lcdcontrol_dev *dev){
    int err, devno = MKDEV(lcdcontrol_major, lcdcontrol_minor);

    cdev_init(&dev->cdev, &lcdcontrol_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &lcdcontrol_fops;

    err = cdev_add(&dev->cdev, devno, 1);
    if (err){
        printk(KERN_ERR "Error %d adding lcdcontrol cdev", err);
    }

    return err;
}

static int lcdcontrol_setup_class(void){
    dev_t dev = MKDEV(lcdcontrol_major, lcdcontrol_minor);

    lcdcontrol_class = class_create("lcdcontrol");
    if (IS_ERR(lcdcontrol_class)){
        return -1;
    }

    if (IS_ERR(device_create(lcdcontrol_class, NULL, dev, NULL, "lcdcontrol"))){
        return -2;
    }

    return 0;
}

static int __init lcdcontrol_init(void){
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, lcdcontrol_minor, 1, "lcdcontrol");
    lcdcontrol_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", lcdcontrol_major);
        return result;
    }

    printk(KERN_INFO "Major=%d, Minor=%d\n", MAJOR(dev), MINOR(dev));

    memset(&lcdcontrol_device, 0, sizeof(struct lcdcontrol_dev));
	mutex_init(&lcdcontrol_device.lock);

    result = lcdcontrol_setup_cdev(&lcdcontrol_device);
    if (result){
        unregister_chrdev_region(dev, 1);
        printk(KERN_WARNING "cdev setup failed\n");
        return result;
    }
    
    printk(KERN_INFO "cdev setup successful\n");

    result = lcdcontrol_setup_class();
    if (result){
        printk(KERN_WARNING "Failed to create device file\n");
        cdev_del(&lcdcontrol_device.cdev);
        class_destroy(lcdcontrol_class);
        unregister_chrdev_region(dev, 1);
        return result;
    }

    printk(KERN_INFO "Class %s created\n", lcdcontrol_class->name);

    return result;
}

static void __exit lcdcontrol_exit(void){
    printk(KERN_INFO "Cleaning the driver artifacts\n");

    dev_t dev = MKDEV(lcdcontrol_major, lcdcontrol_minor);

    device_destroy(lcdcontrol_class, dev);
    cdev_del(&lcdcontrol_device.cdev);
    class_destroy(lcdcontrol_class);
    unregister_chrdev_region(dev, 1);
}

module_init(lcdcontrol_init);
module_exit(lcdcontrol_exit);
