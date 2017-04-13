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
#define DEV_SIZE	 4194304	/* aka 4MB */
#define SET_SIZE	 512
#define NUM_SETS	 DEV_SIZE/SET_SIZE

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
 * array of size SET_SIZE bytes
 */
struct fourmb_ll {
	void* data;
	struct fourmb_ll* next;
};

struct fourmb_dev {
	struct fourmb_ll* buf_list;
	/* 
	 * Amount of (useful) bytes 
	 * stored here.
	 * 
	 * 1. reads do not modify
	 * 2. writes increase it
	 * 3. lseek doesn't modify it
	 * 4. How do we clear this then ? 
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
	return 0; 
}

struct fourmb_ll *compute_dev_idx_ptr(struct fourmb_dev *dev, int idx) {

	if(idx >= NUM_SETS) {
		printk(KERN_ERR "fourmb_device: Maximum Number of sets reached\n");
		return NULL;
	}

	struct fourmb_ll *ll = dev->buf_list;

	if(!ll) {
		ll = dev->buf_list = kmalloc(sizeof(struct fourmb_ll), GFP_KERNEL);
		if (ll == NULL) {
			printk(KERN_ERR "fourmb_device: kmalloc failed to allocate a set 1\n");
			return NULL;
		}
		memset(ll,0,sizeof(struct fourmb_ll));
	}

	/* follow the list */
	while(idx--) {
		if(!ll->next) {
			ll->next = kmalloc(sizeof(struct fourmb_ll), GFP_KERNEL);
			if (ll->next = NULL) {
				printk(KERN_ERR "fourmb_device: kmalloc failed to allocate a set 2\n");
				return NULL;
			}
			memset(ll->next,0,sizeof(struct fourmb_ll));
		}
		ll = ll->next;
	}
	return ll;
}

ssize_t fourmb_read(struct file* filep, char* buf, size_t count, loff_t* f_pos) {
	ssize_t retval = 0;
	unsigned int list_idx, set_off, file_pos, upper_bound;
	struct fourmb_ll* list_idx_ptr;
	struct fourmb_dev *dev = filep->private_data;

	file_pos = (unsigned long)(*f_pos);
	upper_bound = (unsigned long)(*f_pos) + count - 1;

	if(file_pos > dev->size) {
		printk(KERN_INFO "fourmb_device: Nothing written in the offser\n");
		goto out;
	}

	if(upper_bound > dev->size)
		upper_bound = dev->size;
	
	/* reset the count value */
	count = 0;	
	while(file_pos <= upper_bound) {
		/* resolve the indices */
		list_idx = file_pos / SET_SIZE;
		set_off  = file_pos % SET_SIZE;
		list_idx_ptr = compute_dev_idx_ptr(dev,list_idx);
	
		if(list_idx_ptr || !list_idx_ptr->data) {
			printk(KERN_ERR "fourmb_device: Holes encountered while read, How ??\n");
			retval = count;
			goto out;
		}
		
		if (copy_to_user(buf, list_idx_ptr->data + set_off, 1)) {
			printk(KERN_ERR "fourmb_device: Copy to user failure\n");
			retval = count;
			goto out;
		}
		file_pos++;
		count++;
	}
	*f_pos = file_pos;
	retval = count;
	out:
		return retval;
}

ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos) {
	ssize_t retval = 0;
	unsigned int list_idx, set_off;
	struct fourmb_dev* dev = filep->private_data;
	struct fourmb_ll* list_idx_ptr;	
	unsigned long file_pos = (unsigned long)(*f_pos);
	unsigned long upper_bound = (unsigned long)(*f_pos) + count - 1;

	/* Do a bounds checking */
	if(upper_bound > DEV_SIZE) {
		printk(KERN_INFO "fourmb_device: Write limit to device exceeded\n");
		upper_bound = DEV_SIZE;
	}

	/* reset the count value */
	count = 0;
	while(file_pos <= upper_bound) {
		/* resolve the indices */
		list_idx = file_pos / SET_SIZE;
		set_off  = file_pos % SET_SIZE;
		list_idx_ptr = compute_dev_idx_ptr(dev,list_idx);
		
		if(list_idx_ptr == NULL) {
			printk(KERN_INFO "fourmb_device: Unable to create sets while writing\n");
			retval = count;
			goto out;
		}

		if(!list_idx_ptr->data) {
			list_idx_ptr->data = kmalloc(SET_SIZE,GFP_KERNEL);
			if(!list_idx_ptr->data) {
				printk(KERN_INFO "fourmb_device: Unable to create set offsets while writing\n");
				retval = count;
				goto out;
			}
			memset(list_idx_ptr->data,0,SET_SIZE);
		}

		/* 
		 * Does it make sense to copy character one by one
		 * You might get unexpected results.
		 */
		if(copy_from_user(list_idx_ptr->data + set_off,buf,1)) {
			printk(KERN_ERR "fourmb_device: Unable to create copy from user while writing\n");
			retval = -EFAULT;
			goto out;
		}
		file_pos++;
		count++;
	}

	*f_pos = file_pos;
	retval = count;
	if(upper_bound > dev->size)
		dev->size = upper_bound;

	out:
		return retval;
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
		printk(KERN_ERR "fourmb_device: Initialization failed\n");
		retval = -ENOMEM;
		goto fail;
	}
	memset(fourmb_device,0,sizeof(struct fourmb_dev));

	/* Device Initialization */
	cdev_init(&(fourmb_device->cdev),&fourmb_fops);
	fourmb_device->cdev.owner = THIS_MODULE;
	fourmb_device->cdev.ops	  = &fourmb_fops;
	retval = cdev_add(&(fourmb_device->cdev),dev_num,1);
	if(retval) {
		printk(KERN_ERR "fourmb_device: Registration failed\n");
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
