#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#define MAJOR_NUMBER 61 	// You can also try to get the device number automatically
#define DEV_SIZE	 4194304	/* aka 4MB */
#define SET_SIZE	 512
#define NUM_SETS	 DEV_SIZE/SET_SIZE
#define DEBUG
#define MESSAGE_LEN  20
#define FOURMB_DEBUG1

#ifdef FOURMB_DEBUG1
#define FOURMB_DEBUG2
#endif /* !FOURMB_DEBUG1 */

/* for ioctl test */
#define FOURMB_IOC_MAGIC	'k'
#define FOURMB_IOC_HELLO 	_IO(FOURMB_IOC_MAGIC,1)
#define FOURMB_IOC_STM		_IOW(FOURMB_IOC_MAGIC,2,unsigned long) /* write a message */
#define FOURMB_IOC_LDM		_IOR(FOURMB_IOC_MAGIC,3,unsigned long) /* read a message*/
#define FOURMB_IOC_LDSTM	_IOWR(FOURMB_IOC_MAGIC,4,unsigned long) /* Do both */
#define FOURMB_IOC_MAXNR	14

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
	 * 3. lseek increases it
	 * 4. How do we clear this then ? 
	 */
	unsigned long size;
	struct cdev cdev;
	char dev_msg[MESSAGE_LEN];	// used in ioctl method.
};

struct fourmb_dev* fourmb_device; /* Device Instance */

/* forward declaration */
int fourmb_open(struct inode* inode, struct file* filep);
int fourmb_release(struct inode* inode, struct file* filep);
ssize_t fourmb_read(struct file* filep, char* buf, size_t count, loff_t* f_pos);
ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos);
loff_t fourmb_lseek(struct file* filep, loff_t, int whence);
long fourmb_ioctl(struct file* filep, unsigned int, unsigned long);
int fourmb_device_clean(struct fourmb_dev*);

/* definition of file operation structure */
struct file_operations fourmb_fops = {
	.read 			= fourmb_read,
	.write 			= fourmb_write,
	.open 			= fourmb_open,
	.release 		= fourmb_release,
	.llseek			= fourmb_lseek,
	.unlocked_ioctl	= fourmb_ioctl,
};

int fourmb_open(struct inode* inode, struct file* filep) {
	struct fourmb_dev *dev;
	dev = container_of(inode->i_cdev, struct fourmb_dev, cdev);
	filep->private_data = dev;

	if((filep->f_flags & O_ACCMODE) == O_WRONLY) {
		fourmb_device_clean(dev);
	}
	#ifdef DEBUG
	printk(KERN_INFO "fourmb_device: Device Successfully opened");
	#endif
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
			if (ll->next == NULL) {
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
	unsigned int list_idx, set_off, file_pos;
	struct fourmb_ll* list_idx_ptr;
	struct fourmb_dev *dev = filep->private_data;

	file_pos 	= (unsigned long)(*f_pos);

	#ifdef DEBUG
	char written;
	printk(KERN_DEBUG "fourmb_device: file_pos = %d, count = %d for reads\n",file_pos,count);
	#endif
	
	if(file_pos > dev->size) {
		printk(KERN_ERR "fourmb_device: Offset out of bound\n");
		goto out;
	}

	/* trim the count value */
	if(file_pos + count > dev->size) {
		count = dev->size - file_pos;
	}
	
	/* resolve the indices */
	list_idx = file_pos / SET_SIZE;
	set_off  = file_pos % SET_SIZE;
	list_idx_ptr = compute_dev_idx_ptr(dev,list_idx);
	
	if(!list_idx_ptr || !list_idx_ptr->data) {
		printk(KERN_ERR "fourmb_device: Holes encountered while read, How ??\n");
		goto out;
	}

	/* re-evaluate the count value */
	if(set_off + count > SET_SIZE) {
		count = SET_SIZE - set_off;
	}
		
	if (copy_to_user(buf, list_idx_ptr->data + set_off, count)) {
		printk(KERN_ERR "fourmb_device: Copy to user failure\n");
		retval = count;
		goto out;
	}

	*f_pos += count;
	retval = count;
	out:
		return retval;
}

ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos) {
	
	ssize_t retval = 0;
	unsigned int list_idx, set_off;
	struct fourmb_dev* dev = filep->private_data;
	struct fourmb_ll* list_idx_ptr;
	
	/* file offset bounds */
	unsigned long file_pos 	  = (unsigned long)(*f_pos);

	/* Do a bounds checking */
	if(file_pos > DEV_SIZE) {
		printk(KERN_ERR "fourmb_device: Write limit to device exceeded\n");
		goto out;
	}

	#ifdef DEBUG
	char written;
	printk(KERN_DEBUG "fourmb_device: file_pos = %d, count = %d\n",file_pos,count);
	#endif

	/* resolve the indices */
	list_idx 	 = file_pos / SET_SIZE;
	set_off  	 = file_pos % SET_SIZE;
	list_idx_ptr = compute_dev_idx_ptr(dev,list_idx);
	
	if(list_idx_ptr == NULL) {
		printk(KERN_ERR "fourmb_device: Unable to create sets while writing\n");
		goto out;
	}

	if(!list_idx_ptr->data) {
		list_idx_ptr->data = kmalloc(SET_SIZE,GFP_KERNEL);
		if(!list_idx_ptr->data) {
			printk(KERN_ERR "fourmb_device: Unable to create set offsets while writing\n");
			goto out;
		}
		memset(list_idx_ptr->data,0,SET_SIZE);
	}

	/* Re-evaluate count, (you cannot write to more than one set) */
	if ((set_off + count) > SET_SIZE) {
		count = SET_SIZE - set_off;
	}
	
	if(copy_from_user(list_idx_ptr->data + set_off,buf,count)) {
		printk(KERN_ERR "fourmb_device: Unable to create copy from user while writing\n");
		retval = -EFAULT;
		goto out;
	}
	#ifdef DEBUG
	int j;
	for(j = 0; j < count; j++) {
		written = *(char *)(list_idx_ptr->data + j);
		printk(KERN_DEBUG "fourmb_device: character written  to the device = %c\n",written);
	}
	#endif

	*f_pos += count;
	retval = count;
	if ((file_pos + count - 1) > dev->size)
		dev->size = file_pos + count;
	
	#ifdef DEBUG
	printk(KERN_DEBUG "fourmb_device: Resultant file offset %d\n",*f_pos);
	printk(KERN_DEBUG "fourmb_device: Bytes stored in the device %d\n",dev->size);
	printk(KERN_DEBUG "fourmb_device: Bytes written %d\n",count);
	#endif
	
	out:
		return retval;
}

loff_t fourmb_lseek(struct file* filep, loff_t off, int whence) {
	struct fourmb_dev *dev = filep->private_data;
	loff_t newpos;

	switch(whence) {
		case SEEK_SET :
			newpos = off;
			break;

		case SEEK_CUR :
			newpos = filep->f_pos + off;
			break;

		case SEEK_END :
			newpos = dev->size + off;
			break;
	}

	if(newpos < 0) return -EINVAL;
	filep->f_pos = newpos;
	return newpos;
}

long fourmb_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
	struct fourmb_dev *dev = filep->private_data;
	int retval, err = 0;
	char tmp_msg[MESSAGE_LEN];

	/* check for appropriate commands */
	if (_IOC_TYPE(cmd) != FOURMB_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > FOURMB_IOC_MAXNR) return -ENOTTY;

	/* check for appropriate direction */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;

	switch(cmd) {
		case FOURMB_IOC_HELLO:
			printk(KERN_INFO "fourmb_device: hello ioctl usage \n");
			break;

		case FOURMB_IOC_STM: /* write the device string */
			//retval = __get_user(dev_msg, (void __user *)arg); // is this correct
			//if(retval) {
			//	printk(KERN_INFO "fourmb_device: ioctl device naming failed\n");
			//	return retval;
			//}

			/* 
			 * I seriously dont know why it works vis-a-vis the commented
			 * code above and below, but never mind
			 */
			//dev->dev_msg	= (char *)arg;

			if(copy_from_user(dev->dev_msg,(char *)arg,MESSAGE_LEN)) {
				printk(KERN_ERR "fourmb_device: ioctl failed to name the device\n");
				return -ENOTTY;
			}
			printk(KERN_INFO "fourmb_device: ioctl naming the device as %s\n",dev->dev_msg);
			//strcpy(dev->dev_msg,dev_msg);
			break;

		case FOURMB_IOC_LDM: /* read the device string */
			if(copy_to_user((char *)arg,dev->dev_msg,MESSAGE_LEN)) {
				printk(KERN_ERR "fourmb_device: ioctl failed to retrieve the device name\n");
				return -ENOTTY;	
			}
			break;
		//	retval = __put_user(dev_msg,(char __user *)arg);
		//	if(retval) {
		//		printk(KERN_INFO "fourmb_device: ioctl device name retrieval failed\n");
		//		return retval;
		//	}
		//	break;
		case FOURMB_IOC_LDSTM:
			if(copy_from_user(tmp_msg,(char *)arg,MESSAGE_LEN)) {
				printk(KERN_ERR "fourmb_device: ioctl failed to tmp_msg store the name for swap\n");
				return -ENOTTY;
			}
			printk(KERN_INFO "fourmb_device: ioctl temporarily stored the name for swap %s\n",tmp_msg);
			if(copy_to_user((char *)arg,dev->dev_msg,MESSAGE_LEN)) {
				printk(KERN_ERR "fourmb_device: ioctl failed to swap the device name\n");
				return -ENOTTY;
			}
			printk(KERN_INFO "fourmb_device: ioctl successfuly swapped the device name\n");
			strcpy(dev->dev_msg,tmp_msg);
			printk(KERN_INFO "fourmb_device: ioctl new device name after swap %s\n",dev->dev_msg);
			break;

		default:
			return -ENOTTY;
	}
}

static int __exit fourmb_device_exit(void) {
	dev_t dev_num = MKDEV(fourmb_major,fourmb_minor);

	/* Get rid of our char dev entries */
	if(fourmb_device) {
		fourmb_device_clean(fourmb_device);
		kfree(fourmb_device);
	}
	unregister_chrdev_region(dev_num,1);
	printk(KERN_INFO "fourmb_device: Device removed successfully\n");
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
	strcpy(fourmb_device->dev_msg,"anonymous");
	cdev_init(&(fourmb_device->cdev),&fourmb_fops);
	fourmb_device->cdev.owner = THIS_MODULE;
	fourmb_device->cdev.ops	  = &fourmb_fops;
	retval = cdev_add(&(fourmb_device->cdev),dev_num,1);
	if(retval) {
		printk(KERN_ERR "fourmb_device: Registration failed\n");
	}

	printk(KERN_INFO "fourmb_device: Device initialized and registered successfully\n");
	return 0;
	fail:
		fourmb_device_exit();
		return retval;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arka Maity");
MODULE_DESCRIPTION("First Device Driver: 24.03.2017");

module_init(fourmb_device_init);
module_exit(fourmb_device_exit);
