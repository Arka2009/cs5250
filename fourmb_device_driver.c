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
int fourmb_open(struct inode* inode, struct file* filep);
int fourmb_release(struct inode* inode, struct file* filep);
ssize_t fourmb_read(struct file* filep, char* buf, size_t count, loff_t* f_pos);
ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos);

/* definition of file operation structure */
struct file_operations fourmb_fops = {
	.read = fourmb_read,
	.write = fourmb_write,
	.open = fourmb_open,
	.release = fourmb_release,
};

static char* fourmb_data = NULL;		// Stores the Character
static int   fourmb_numbOpens = 0;		// Number of Opens

int fourmb_open(struct inode* inode, struct file* filep) {
	fourmb_numbOpens++;
	printk(KERN_INFO "fourmb_device: Device Successfully Opened, \
	%d time(s)\n",fourmb_numbOpens);
	return 0;
}

int fourmb_release(struct inode* inode, struct file* filep) { 
	printk(KERN_INFO "fourmb_device: Device Successfully Closed\n");
	return 0;
}

ssize_t fourmb_read(struct file* filep, char* buf, size_t count, loff_t* f_pos) {
	int err_count = 0;
	
	err_count = copy_to_user(buf,fourmb_data,1);
	
	if(err_count == 0) {
		printk(KERN_INFO "fourmb_device: User Space Application read one byte\n");
		return 0;
	}
	else {
		printk(KERN_INFO "fourmb_device: Failed to read the device\n");
		return -EFAULT;
	}
}

ssize_t fourmb_write(struct file* filep, const char* buf, size_t count, loff_t* f_pos) {
	if(count > 1) {
		printk(KERN_INFO "fourmb_device: Failed to write device, accepts a single byte only\n");
		return -EFAULT;
	}
	//sprintf(fourmb_data,buf);
	*fourmb_data = buf[0];
	printk(KERN_INFO "fourmb_device: Write Successful\n");
	return count;
}

/* LKM init modules */
static int __init fourmb_device_init(void) {
	printk(KERN_INFO "fourmb_device: Initializing the device\n");

	// Try to register the device
	int major_number = register_chrdev(MAJOR_NUMBER,"fourmb_device",&fourmb_fops);	
	if(major_number < 0) {
		printk(KERN_ALERT "fourmb_device: failed to register the major number\n");
		return major_number;
	}
	
	fourmb_data = kmalloc(sizeof(char),GFP_KERNEL);
	if(!fourmb_data) {
		printk(KERN_ALERT "fourmb_device: Memory allocation failed during initialization\n");
		return -ENOMEM;
	}
	//sprintf(fourmb_data,"X");
	*fourmb_data = 'X';
	printk(KERN_INFO "fourmb_device: Successfully initialized one byte device\n");
	return 0;
}

static int __exit fourmb_device_exit(void) {
	printk(KERN_INFO "fourmb_device: Unloading one byte device\n");
	if(fourmb_data) {
		kfree(fourmb_data);
		fourmb_data = NULL;
	}
	unregister_chrdev(MAJOR_NUMBER,"fourmb_device");
	printk(KERN_INFO "fourmb_device: Successfuly unloaded\n");
	return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arka Maity");
MODULE_DESCRIPTION("First Device Driver: 24.03.2017");

module_init(fourmb_device_init);
module_exit(fourmb_device_exit);
