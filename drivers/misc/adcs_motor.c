#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/mxc_icap.h>
static int icapChan = 0;
static int evtCount = 0;
static struct timespec startTV;
static void mxc_icap_handler(int chan, void * dev_id, struct timespec * tv)
{
		evtCount++;
		memcpy(&startTV, tv, sizeof(startTV));
}

static int motor_open(struct inode *inode, struct file *file)
{
	  void *dev_id =0;
    int ret ;
    evtCount = 0;
    ret = mxc_request_input_capture(icapChan, mxc_icap_handler, IRQF_TRIGGER_RISING, dev_id);
	  
	  pr_info("motor device opened ret=%d\n",ret);
    return ret;
}

static int motor_close(struct inode *inodep, struct file *filp)
{
	 void *dev_id =0;
	  mxc_free_input_capture(icapChan, dev_id);
		pr_info("motor device close\n");	      
    return 0;
}

static ssize_t motor_write(struct file *file, const char __user *buf,
		       size_t len, loff_t *ppos)
{
    pr_info("Yummy - I just write %d bytes\n", len);
    mxc_dump_gpt_reg();
    return len; /* But we don't actually do anything with the data */
}
static ssize_t motor_read(struct file *file, char __user *buf,
		       size_t len, loff_t *ppos)
{
		int rlen = 0;
		if(len>=sizeof(int)){
			memcpy(buf, &evtCount,sizeof(int));
			rlen=sizeof(int);
		}
		if(len>=sizeof(int)+sizeof(startTV)){
			memcpy(buf+4, &startTV, sizeof(startTV));
			rlen+=sizeof(startTV);
		}
    pr_info("read %d bytes\n", rlen);
    return rlen; /* But we don't actually do anything with the data */
}

static const struct file_operations sample_fops = {
    .owner			= THIS_MODULE,
    .write			= motor_write,
    .read			= motor_read,
    .open				= motor_open,
    .release		= motor_close,
    .llseek 		= no_llseek,
};

struct miscdevice sample_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "motor",
    .fops = &sample_fops,
};

static int __init misc_init(void)
{
    int error;

    error = misc_register(&sample_device);
    if (error) {
        pr_err("can't misc_register :(\n");
        return error;
    }

    pr_info("I'm in\n");
    return 0;
}

static void __exit misc_exit(void)
{
    misc_deregister(&sample_device);
    pr_info("I'm out\n");
}

module_init(misc_init)
module_exit(misc_exit)

MODULE_DESCRIPTION("Motor Speed Driver");
MODULE_AUTHOR("Kyle Yan <kyleyan1997@gmail.com>");
MODULE_LICENSE("GPL");