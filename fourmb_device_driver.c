#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#define MAJOR_NUMBER 61 	// You can also try to get the device number automatically
#define SET_SIZE	 512

#define FOURMB_DEBUG1

#ifdef FOURMB_DEBUG1
#define FOURMB_DEBUG2
#endif /* !FOURMB_DEBUG1 */

int fourmb_major = MAJOR_NUMBER;
int fourmb_minor = 0;

/* The Device Structure :
 * ----------------------
 * 
 * The Device has a maximum
 * size of 4MB. Storage is
 * as a (singly) linked list
 * of nodes each pointing to an
 * array of size SET_SIZE KB
 */
struct fourmb_ll {
	void* data;
	struct fourmb_ll* next;
};

struct fourmb_dev {
	struct fourmb_ll* buf_list;
	/* 
	 * Amount of (useful) Data 
	 * stored here. This field
	 * is also used to get the
	 * file offset
	 */
	unsigned long size;
	struct cdev cdev;
};

struct fourmb_dev* fourmb_device; /* Device Instance */

/* forward declaration */
int fourmb_open(struct inode* inode, struct file* filep);
int fourmb_release(struct inode* inode, struct file* filep);
ssize_t fourmb_read(struct file* filep, char* buf, size_t count, loff_t* f_pos);
ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos);
int fourmb_device_clean(struct fourmb_dev*);

/* definition of file operation structure */
struct file_operations fourmb_fops = {
	.read 			= fourmb_read,
	.write 			= fourmb_write,
	.open 			= fourmb_open,
	.release 		= fourmb_release,
	//.llseek			= fourmb_lseek,
	//.unlocked_ioctl	= fourmb_ioctl,
};

int fourmb_open(struct inode* inode, struct file* filep) {
	struct fourmb_dev *dev;
	dev = container_of(inode->i_cdev, struct fourmb_dev, cdev);
	filep->private_data = dev;

	if((filep->f_flags & O_ACCMODE) == O_WRONLY) {
		fourmb_device_clean(dev);
	}
	return 0;
}

int fourmb_release(struct inode* inode, struct file* filep) { 
}

ssize_t fourmb_read(struct file* filep, char* buf, size_t count, loff_t* f_pos) {
}

ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos) {
}

static int __exit fourmb_device_exit(void) {
	dev_t dev_num = MKDEV(fourmb_major,fourmb_minor);

	/* Get rid of our char dev entries */
	if(fourmb_device) {
		fourmb_device_clean(fourmb_device);
		kfree(fourmb_device);
	}
	unregister_chrdev_region(dev_num,1);
	return 0;	
}

int fourmb_device_clean(struct fourmb_dev* dev) {
	struct fourmb_ll *itr, *next;
	for(itr = dev->buf_list; itr; itr = next) {
		if(itr->data) {
			kfree(itr->data);
			itr->data = NULL;
		}
		next = itr->next;
		kfree(itr);
	}
	dev->buf_list = NULL;
	dev->size = 0;
	return 0;
}


/* LKM init modules */
static int __init fourmb_device_init(void) {
	int retval = 0;
	dev_t dev_num = MKDEV(fourmb_major,fourmb_minor);
	
	/* Allocate the Device */
	fourmb_device = kmalloc(sizeof(struct fourmb_dev),GFP_KERNEL);
	if(!fourmb_device) {
		retval = -ENOMEM;
		goto fail;
		/* printk error message */
	}
	memset(fourmb_device,0,sizeof(struct fourmb_dev));

	/* Device Initialization */
	cdev_init(&(fourmb_device->cdev),&fourmb_fops);
	fourmb_device->cdev.owner = THIS_MODULE;
	fourmb_device->cdev.ops	  = &fourmb_fops;
	retval = cdev_add(&(fourmb_device->cdev),dev_num,1);
	if(retval) {
		/* printk error message */
	}

	fail:
		fourmb_device_exit();
		return retval;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arka Maity");
MODULE_DESCRIPTION("First Device Driver: 24.03.2017");

module_init(fourmb_device_init);
module_exit(fourmb_device_exit);
