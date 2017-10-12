#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/types.h>
//#include <mach/regs-gpio.h>

#define SHT11_DATA_PIN	S3C2410_GPL(9)
#define SHT11_CLK_PIN	S3C2410_GPA(16)
#define DATA_1	gpio_direction_output(SHT11_DATA_PIN, 1)
#define DATA_0	gpio_direction_output(SHT11_DATA_PIN, 0)
#define SCK_1	gpio_direction_output(SHT11_CLK_PIN, 1)
#define SCK_0	gpio_direction_output(SHT11_CLK_PIN, 0)
#define DATA_SET_INPUT	gpio_direction_input(SHT11_DATA_PIN)
#define DATA_GET_VALUE	gpio_get_value(SHT11_DATA_PIN)

#define noACK	0
#define ACK		1
#define STATUS_REG_W	0x06
#define STATUS_REG_R	0x07
#define MEASURE_TEMP	0x03
#define MEASURE_HUMI	0x05
#define RESET			0x1e
enum {TEMP, HUMI};
 
/*
 * generates a transmission start
 */  
static void s_transstart(void)
{
	DATA_1;
	SCK_0;
	udelay(1);
	SCK_1;
	udelay(1);
	DATA_0;
	udelay(1);
	SCK_0;
	udelay(3);
	SCK_1;
	udelay(1);
	DATA_1;
	udelay(1);
	SCK_0;
}	

/*
 * communication reset
 */
static void s_connectionreset(void)
{
	unsigned char i;
	
	DATA_1;
	SCK_0;
	for (i = 0; i < 9; i++) {
		SCK_1;
		udelay(1);
		SCK_0;
		udelay(1);
	}
	s_transstart();
}
 
/*
 * write a byte and check the acknowledge
 */ 
static unsigned char s_write_byte(unsigned char value)
{
	unsigned char i, error = 0;
	
	for (i = 0x80; i > 0; i /= 2) {
		if (i & value)
			DATA_1;
		else
			DATA_0;
		udelay(1);
		SCK_1;
		udelay(3);
		SCK_0;
		udelay(1);
	}
	DATA_1;
	udelay(1);
	SCK_1;
	DATA_SET_INPUT;
	error = DATA_GET_VALUE;		// check ack(DATA will be pulled down)
	SCK_0;
	return error;
}	
 
/*
 * read a byte and send an acknowledge
 */
static unsigned char s_read_byte(unsigned char ack)
{
	unsigned char i, val = 0;
	
	DATA_1;
	for (i = 0x80; i > 0; i /= 2) {
		SCK_1;		
		DATA_SET_INPUT;
		udelay(1);	//
		if (DATA_GET_VALUE) 
			val |= i;
		SCK_0;
		udelay(1);	//
	}
	(!ack) ? DATA_1 : DATA_0;
	udelay(1);
	SCK_1;
	udelay(3);
	SCK_0;
	udelay(1);
	DATA_1;
	return val;
}	
 
/*
 * make a measurement with checksum
 */
static unsigned char sht11_read_data(unsigned char *p_value, unsigned char *p_checksum, unsigned char mode)
{
	unsigned char error = 0;
	unsigned long i;
	
	s_transstart();
	switch (mode) {
		case TEMP:
		error += s_write_byte(MEASURE_TEMP);
		break;
		case HUMI:
		error += s_write_byte(MEASURE_HUMI);
		break;
		default: break;
	}
	DATA_SET_INPUT;
	for (i = 0; i < 400*1000*5; i++)	// if frequency = 400M, then wait for (1us)*1000*5
		if (DATA_GET_VALUE == 0) break;
	if (DATA_GET_VALUE) { 
		error++;
		return error;
	}
	*(p_value+1) = s_read_byte(ACK);	// The first byte is MSB(little endian)
	*(p_value) = s_read_byte(ACK);
	*p_checksum = s_read_byte(noACK);
	return error;
} 

/*
 * open the device, here is null
 */ 
static ssize_t sht11_open(struct inode *inode, struct file *file)
{
	unsigned long flags;
	
	local_irq_save(flags);
	s_connectionreset();
	local_irq_restore(flags);
	
	return 0;
}
 
// read function
static ssize_t sht11_read(struct file *file, char __user *buffer, size_t size, loff_t *offset)
{
	unsigned long flags;
	unsigned char err = 0, checksum;
	unsigned short humi_val = 0, temp_val = 0;
	unsigned char sht11_data_buf[4];
	int ret;
	
	local_irq_save(flags);
	if (err == 0) err = sht11_read_data((unsigned char *)&humi_val, &checksum, HUMI);
	if (err == 0) err = sht11_read_data((unsigned char *)&temp_val, &checksum, TEMP);
	local_irq_restore(flags);
	
	if (err == 0) {
		sht11_data_buf[0] = humi_val & 0xff;
		sht11_data_buf[1] = humi_val /256;
		sht11_data_buf[2] = temp_val & 0xff;
		sht11_data_buf[3] = temp_val /256;		
		ret = copy_to_user(buffer, sht11_data_buf, sizeof(sht11_data_buf));
		if (ret < 0) {
			printk("copy to user err\n");
			return -EAGAIN;
		} else {
			return 0;
		}
	} else {
		return -EAGAIN;
	}
}

/*
 * release the device, here is null
 */
static int sht11_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * define operation struct and initialize its function pointer
 */ 
static const struct file_operations sht11_fops = {
	.owner = THIS_MODULE,
	.open = sht11_open,
	.read = sht11_read,
	.release = sht11_release,
};

#define SHT11DEV_MAJOR	239
#define SHT11DEV_MINOR	0
#define SHT11DEV_NR_DEVS	1

static int sht11_major = SHT11DEV_MAJOR;
static int sht11_minor = SHT11DEV_MINOR;
static int sht11_nr_devs = SHT11DEV_NR_DEVS;

/*
 * each device with a structure of type
 */
struct sht11_dev {
	struct cdev cdev;	/* Char device structure */
};
struct sht11_dev *sht11_devp;

static void sht11_setup_cdev(struct sht11_dev *dev, int index)
{
	int err, devno = MKDEV(sht11_major, sht11_minor + index);

	cdev_init(&dev->cdev, &sht11_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &sht11_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding sht11(%d)", err, index);
}

// entry
static int sht11_dev_init(void)
{
	dev_t dev;
	int result;
	
	if (sht11_major) {
		dev = MKDEV(sht11_major, sht11_minor);
		result = register_chrdev_region(dev, sht11_nr_devs, "sht11_dev");
	} else {
		result = alloc_chrdev_region(&dev, sht11_minor, sht11_nr_devs, "sht11_dev");
		sht11_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "sht11_dev: register fail, major = %d\n", sht11_major);
		return result;
	}

	sht11_devp = kmalloc(sizeof(struct sht11_dev), GFP_KERNEL);
	if (!sht11_devp) {
		result = -ENOMEM;
		printk(KERN_NOTICE "Error add sht11_dev");
		unregister_chrdev_region(dev, sht11_nr_devs);
		return result;
	}
	memset(sht11_devp, 0, sizeof(struct sht11_dev));
	sht11_setup_cdev(sht11_devp, 0);
	
	/* request gpio resource */
	result = gpio_request(SHT11_CLK_PIN, "sht11_dev");
	if (result) {
		printk("request sht11 CLK pin failed");
		return result;
	}
	result = gpio_request(SHT11_DATA_PIN, "sht11_dev");
	if (result) {
		printk("request sht11 DATA pin failed");
		return result;
	}

	s3c2410_gpio_pullup(S3C2410_GPL(9), 0);	// pull-up
	s3c2410_gpio_pullup(S3C2410_GPA(16), 0);
	return 0;
}

// exit
static void sht11_dev_exit(void)
{
	cdev_del(&sht11_devp->cdev);
	kfree(sht11_devp);
	unregister_chrdev_region(MKDEV(sht11_major, sht11_minor), sht11_nr_devs);
	gpio_free(SHT11_CLK_PIN);
	gpio_free(SHT11_DATA_PIN);
}

module_init(sht11_dev_init);
module_exit(sht11_dev_exit);

MODULE_LICENSE("GPL");
