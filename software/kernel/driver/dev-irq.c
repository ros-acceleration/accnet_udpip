// SPDX-License-Identifier: GPL-2.0+
/* dev-irq.c
*
* Loadable module for udp-ip core userspace IRQs
*
* Copyright (C) Accelerat S.r.l.
*/

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/types.h>

/* -------------------------------------------------------------------------- */

MODULE_AUTHOR		("Accelerat S.r.l.");
MODULE_DESCRIPTION	("dev-irq - Loadable module for udp-ip core userspace IRQs");
MODULE_LICENSE		("GPL");

/* -------------------------------------------------------------------------- */

#define DEVICE_ADDRESS      0xA0010000  // address of device reg in physical mem
#define DEVICE_SIZE         0x1000      // size of mapped region

/* -------------------------------------------------------------------------- */

#define RBTC_CTRL_ADDR_ISR0     (0x00000050)
#define RBTC_CTRL_ADDR_IER0     (0x00000058)
#define RBTC_CTRL_ADDR_GIE      (0x00000060)

/* -------------------------------------------------------------------------- */

#define DRIVER_NAME         "dev-irq"
#define DEV_NAME_SIZE       (32)
#define MAX_TIMESTAMP_SIZE  (16)

/* -------------------------------------------------------------------------- */

#define NANO_TO_MICRO(ns)					(ns / 1000)

/* -------------------------------------------------------------------------- */

#define WRITE_ISR0(devbase, val) \
    writel(val, ((u8*) devbase) + RBTC_CTRL_ADDR_ISR0)

#define WRITE_IER0(devbase, val) \
    writel(val, ((u8*) devbase) + RBTC_CTRL_ADDR_IER0)

#define WRITE_GIE(devbase, val) \
    writel(val, ((u8*) devbase) + RBTC_CTRL_ADDR_GIE)

/* -------------------------------------------------------------------------- */

struct dev_irq 
{
    unsigned int irqn;
    char 						dev_name[DEV_NAME_SIZE];
    struct timespec64 			timestamp;
    struct wait_queue_head  	irq_wq;
    bool 						irq_arrived;
    struct miscdevice 			misc_cdev;
    void __iomem                *reg_base;
};

/* -------------------------------------------------------------------------- */

static ssize_t dev_read (struct file *file, char __user *buf, size_t len, loff_t *ppos);

static int dev_open (struct inode *inode, struct file *file);
static int dev_release (struct inode *inode, struct file *file);

static const struct file_operations cdev_fops = 
{
    .owner		= THIS_MODULE,
    .read		= dev_read,
    .open 		= dev_open,
    .release 	= dev_release
};

/* -------------------------------------------------------------------------- */

static int dev_irq_probe(struct platform_device *pdev);
static int dev_irq_remove(struct platform_device *pdev);

#ifdef CONFIG_OF
static struct of_device_id dev_irq_of_match[] = 
{
    { 
        .compatible = "accelerat,udp-core" 
    },
    { 
        /* end of list */ 
    },
};
MODULE_DEVICE_TABLE(of, dev_irq_of_match);
#endif

static struct platform_driver dev_irq_driver = 
{
    .driver = 
    {
        .name 				= DRIVER_NAME,
        .owner 				= THIS_MODULE,
        #ifdef CONFIG_OF
        .of_match_table		= dev_irq_of_match,
        #endif 
    },
    .probe		= dev_irq_probe,
    .remove		= dev_irq_remove,
};

static struct dev_irq drv_data = 
{
    .irqn           = 0,
    .dev_name       = "udp-core-irq",
    .irq_arrived    = false,
    .reg_base       = NULL,
};

/* -------------------------------------------------------------------------- */

static void dev_clean(struct dev_irq *drv_data)
{
    WRITE_IER0(drv_data->reg_base, 0);
    WRITE_GIE(drv_data->reg_base, 0);

    misc_deregister(&(drv_data->misc_cdev));

    pr_info("dev-irq: removed: irq: %u, dev: %s.\n", drv_data->irqn,
        drv_data->dev_name);
}

static int dev_init(struct dev_irq *drv_data)
{
    int retval;

    pr_info("dev-irq: initialization of irq misc dev.\n");
   
    /* Init misc device name and fops */
    drv_data->misc_cdev.minor = MISC_DYNAMIC_MINOR;
    drv_data->misc_cdev.name = drv_data->dev_name;
    drv_data->misc_cdev.fops = &cdev_fops;

    pr_info("dev-irq: misc registering: irq: %u, name: %s.\n",
        drv_data->irqn,
        drv_data->dev_name);

    retval = misc_register(&drv_data->misc_cdev);
    
    if (retval != 0) 
    {
        pr_err("dev-irq: cannot register the device.\n");
        goto out_clean;
    }

    init_waitqueue_head(&drv_data->irq_wq);

    WRITE_IER0(drv_data->reg_base, 1);
    WRITE_GIE(drv_data->reg_base, 1);

    retval = 0;
    goto out;

out_clean:
    dev_clean(drv_data);

out:
    return retval;
}

/* -------------------------------------------------------------------------- */

static int dev_open (struct inode *inode, struct file *file)
{
    pr_info("dev-irq: opened misc dev: %s\n", file->f_path.dentry->d_iname);
    file->private_data = &drv_data;
    return 0;
}

static int dev_release (struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}

static ssize_t f_read(struct dev_irq* drv_data_p, char __user *buf, size_t len, loff_t *ppos)
{
	struct timespec64 ts;
	struct tm broken;
	char timestamp[MAX_TIMESTAMP_SIZE];
	loff_t pos;
	size_t ret;

	ts.tv_sec = drv_data_p->timestamp.tv_sec;
	ts.tv_nsec = drv_data_p->timestamp.tv_nsec;
	
	time64_to_tm(ts.tv_sec, 0, &broken);
	
	snprintf(timestamp, MAX_TIMESTAMP_SIZE, "%02d:%02d:%02d.%06lu", 
		broken.tm_hour, 
		broken.tm_min, 
		broken.tm_sec, 
		NANO_TO_MICRO(ts.tv_nsec));

	pos = *ppos;
	
	if (pos < 0)
		return -EINVAL;
	
	if (pos >= MAX_TIMESTAMP_SIZE || len == 0)
		return 0;
	
	if (len > MAX_TIMESTAMP_SIZE - pos)
		len = MAX_TIMESTAMP_SIZE - pos;

	ret = copy_to_user(buf, timestamp + pos, len);
	
	if (ret == len)
		return -EFAULT;
	
	len -= ret;
	
	return len;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
    struct dev_irq* drv_data_p;
    
    drv_data_p = (struct dev_irq*)file->private_data;

    pr_info("dev-irq: read misc file for irqn: %u.\n", drv_data_p->irqn);
    
    drv_data_p->irq_arrived = false;

    wait_event_interruptible(drv_data_p->irq_wq, drv_data_p->irq_arrived);
    
    return f_read(drv_data_p, buf, len, ppos);
}


/* -------------------------------------------------------------------------- */

static irqreturn_t dev_irq_irq(int irq, void *dev)
{
    struct dev_irq* drv_data_p;
    struct timespec64 ts;
    //struct tm broken;

    // pr_info("dev-irq: interrupt with irq %d.\n", irq);
    ktime_get_ts64(&ts);

    drv_data_p = dev_get_drvdata(dev);
    
    drv_data_p->irq_arrived = true;
    drv_data_p->timestamp.tv_sec = ts.tv_sec;
    drv_data_p->timestamp.tv_nsec = ts.tv_nsec;
            
    // time64_to_tm(ts.tv_sec, 0, &broken);
    // pr_info("dev-irq: received at: %d:%d:%d:%ld \n", 
    //         broken.tm_hour, 
    //         broken.tm_min, 
    //         broken.tm_sec, 
    //         ts.tv_nsec
    //     );

    wake_up_all(&drv_data_p->irq_wq);

    WRITE_ISR0(drv_data_p->reg_base, 0);
    
    return IRQ_HANDLED;
}

static int dev_irq_register_irq(struct platform_device *pdev)
{
    int irqn;
    int req;

    irqn = platform_get_irq(pdev, 0);

    if (irqn < 0)
    {
        pr_warn("dev-irq: unable to retrieve remapped irq.\n");
        return irqn;
    }

    pr_info("dev-irq: remapped irq -> %d.\n", irqn);
    
    req = request_irq(irqn, &dev_irq_irq, 0, DRIVER_NAME, (void*) &(pdev->dev));

    if (req) 
    {
        pr_err("dev-irq: could not allocate interrupt %d.\n", irqn);
        return req;
    }

    pr_info("dev-irq: registered handler for irq %d.\n", irqn);

    return irqn;
}

static int dev_irq_probe(struct platform_device *pdev)
{
    int irqn;						// number of re-mapped irq 
    int req;						// interrupt handler registration request
    int num_irq;					// total number of declared irq in the tree

    irqn = 0;
    req = 0;
    num_irq = 0;

    pr_info("dev-irq: device tree probing.\n");

    if (pdev->name != NULL)
    {
        pr_err("dev-irq: found device with name: %s.\n", pdev->name);
    }

    platform_set_drvdata(pdev, &drv_data);
    
    num_irq = platform_irq_count(pdev);
    
    if (num_irq < 1)
    {
        pr_err("dev-irq: no irqs available in device-tree.\n");
        goto error_irqs_not_found;
    }
    else if (num_irq > 1)
    {
        pr_warn("dev-irq: multiple irqs in device-tree, the 1st will be used.\n");
    }

    irqn = dev_irq_register_irq(pdev);

    if (irqn < 0)
        goto error_irq_not_available;

    drv_data.irqn = irqn;

    return 0;

error_irq_not_available:
    dev_irq_remove(pdev);
    dev_clean(&drv_data);
    return irqn;

error_irqs_not_found:
    platform_set_drvdata(pdev, NULL);
    return -EINVAL;
}

static int dev_irq_remove(struct platform_device *pdev)
{
    struct dev_irq *drv_data_p;			// pointer to es data structure
    
    drv_data_p = platform_get_drvdata(pdev);

    if (drv_data_p == NULL)
    {
        pr_warn("dev-irq: failed driver data retrieval, irq won't be freed.\n");
        return -ENXIO;
    }

    if (free_irq(drv_data_p->irqn, &pdev->dev) == NULL)
    {
        pr_warn("dev-irq: unable to free irq %d.\n", drv_data_p->irqn);
    }
    else
    {
        pr_info("dev-irq: %d irq freed.\n", drv_data_p->irqn);
    }

    platform_set_drvdata(pdev, NULL);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int __init dev_irq_mod_init(void)
{
    int retval;

    pr_info("dev-irq: initializing kernel module.\n");
    
    /**
     * When probe fails, platform_driver_register(..) returns 0
     * Probe error is always dropped because the kernel keep trying to bind 
     * devices to other drivers
     */
    
    retval = platform_driver_register(&dev_irq_driver);

    if (retval < 0)
    {
        pr_err("dev-irq: unable to register platform driver.");
        return retval;
    }

    drv_data.reg_base = ioremap(DEVICE_ADDRESS, DEVICE_SIZE);
    
    if (drv_data.reg_base == NULL) 
    {
        pr_err("dev-irq: failed to ioremap register\n");
        return -ENOMEM;
    }

    retval = dev_init(&drv_data);

    if (retval < 0)
    {
        pr_err("dev-irq: unable to register misc devices.");
        return retval;
    }
        
    return 0;
}

static void __exit dev_irq_mod_exit(void)
{
    platform_driver_unregister(&dev_irq_driver);
    dev_clean(&drv_data);
    pr_info("dev-irq: unregistered event-signaling module.\n");
}

/* -------------------------------------------------------------------------- */

module_init(dev_irq_mod_init);
module_exit(dev_irq_mod_exit);