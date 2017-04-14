#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int lcd;

/* for ioctl test */
#define FOURMB_IOC_MAGIC	'k'
#define FOURMB_IOC_HELLO 	_IO(FOURMB_IOC_MAGIC,1)
#define FOURMB_IOC_STM		_IOW(FOURMB_IOC_MAGIC,2,unsigned long) /* write a message */
#define FOURMB_IOC_LDM		_IOR(FOURMB_IOC_MAGIC,3,unsigned long) /* read a message*/
#define FOURMB_IOC_LDSTM	_IOWR(FOURMB_IOC_MAGIC,4,unsigned long) /* Do both */
#define FOURMB_IOC_MAXNR	4

void test() {
	int k, i, sum;
	char s[3], user_msg[30] = "jkfvfvb";
	memset(s,'2',sizeof(s));

	printf("ioctl_test: writing to device\n");
	k = write(lcd,s,sizeof(s));
	printf("ioctl_test: written = %d\n",k);	
	
	k = ioctl(lcd,FOURMB_IOC_HELLO);

	printf("ioctl_test: writing a message to device\n");
	k = ioctl(lcd,FOURMB_IOC_STM,"lkncvn");
	printf("ioctl_test: written = %d\n",k);

	//printf("ioctl_test: retrieving a message from the device\n");
	//k = ioctl(lcd,FOURMB_IOC_LDM,user_msg);
	//printf("ioctl_test: read = %s\n",user_msg);
	
	printf("ioctl_test: swapping  user_msg %s with dev_msg \n",user_msg);
	k = ioctl(lcd,FOURMB_IOC_LDSTM,user_msg);
	printf("ioctl_test: after swap user_msg %s\n",user_msg);
}

int main(int argc, char** argv) {
	lcd = open("/dev/fourmb_device_driver",O_RDWR);
	if(lcd == -1) {
		perror("unable to open lcd");
		exit(EXIT_FAILURE);
	}

	test();
	close(lcd);
	
	return 0;
}
