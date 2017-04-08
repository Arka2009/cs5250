#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define MAJOR_NUMBER 61 	// You can also try to get the device number automatically

/* forward declaration */
int onebyte_open(struct inode* inode, struct file* filep);
int onebyte_release(struct inode* inode, struct file* filep);
ssize_t onebyte_read(struct file* filep, char* buf, size_t count, loff_t* f_pos);
ssize_t onebyte_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos);

/* definition of file operation structure */
struct file_operations onebyte_fops = {
	.read = onebyte_read,
	.write = onebyte_write,
	.open = onebyte_open,
	.release = onebyte_release,
};

static char* onebyte_data = NULL;		// Stores the Character
static int   onebyte_numbOpens = 0;		// Number of Opens

int onebyte_open(struct inode* inode, struct file* filep) {
	onebyte_numbOpens++;
	printk(KERN_INFO "onebyte_device: Device Successfully Opened, \
	%d time(s)\n",onebyte_numbOpens);
	return 0;
}

int onebyte_release(struct inode* inode, struct file* filep) { 
	printk(KERN_INFO "onebyte_device: Device Successfully Closed\n");
	return 0;
}

ssize_t onebyte_read(struct file* filep, char* buf, size_t count, loff_t* f_pos) {
	int err_count = 0;
	
	err_count = copy_to_user(buf,onebyte_data,1);
	
	if(err_count == 0) {
		printk(KERN_INFO "onebyte_device: User Space Application read one byte\n");
		return 0;
	}
	else {
		printk(KERN_INFO "onebyte_device: Failed to read the device\n");
		return -EFAULT;
	}
}

ssize_t onebyte_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos) {
	if(count > 1) {
		printk(KERN_INFO "onebyte_device: Failed to write device, accepts a single byte only\n");
		return -EFAULT;
	}
	//sprintf(onebyte_data,buf);
	*onebyte_data = buf[0];
	printk(KERN_INFO "onebyte_device: Write Successful\n");
	return count;
}

/* LKM init modules */
static int __init onebyte_device_init(void) {
	printk(KERN_INFO "onebyte_device: Initializing the device\n");

	// Try to register the device
	int major_number = register_chrdev(MAJOR_NUMBER,"onebyte_device",&onebyte_fops);	
	if(major_number < 0) {
		printk(KERN_ALERT "onebyte_device: failed to register the major number\n");
		return major_number;
	}
	
	onebyte_data = kmalloc(sizeof(char),GFP_KERNEL);
	if(!onebyte_data) {
		printk(KERN_ALERT "onebyte_device: Memory allocation failed during initialization\n");
		return -ENOMEM;
	}
	//sprintf(onebyte_data,"X");
	*onebyte_data = 'X';
	printk(KERN_INFO "onebyte_device: Successfully initialized one byte device\n");
	return 0;
}

static int __exit onebyte_device_exit(void) {
	printk(KERN_INFO "onebyte_device: Unloading one byte device\n");
	if(onebyte_data) {
		kfree(onebyte_data);
		onebyte_data = NULL;
	}
	unregister_chrdev(MAJOR_NUMBER,"onebyte_device");
	printk(KERN_INFO "onebyte_device: Successfuly unloaded\n");
	return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arka Maity");
MODULE_DESCRIPTION("First Device Driver: 24.03.2017");

module_init(onebyte_device_init);
module_exit(onebyte_device_exit);
