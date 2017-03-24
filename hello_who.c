#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

static char* who = "whoami?...guess!";

module_param(who, charp, 0644);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arka Maity");
MODULE_DESCRIPTION("Intro to linux kernel programming");
MODULE_PARM_DESC(who,"Who Am I ?");

static int __init hello_init(void)
{
	printk(KERN_INFO "Hello, World greetings from %s\n",who);
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_INFO "Goodbye, world, regards %s\n",who);
}

module_init(hello_init);
module_exit(hello_exit);
