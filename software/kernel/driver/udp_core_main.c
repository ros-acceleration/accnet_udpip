// SPDX-License-Identifier: GPL-2.0+
/* udp-core.c
 *
 * Loadable module for controlling UDP Ethernet Stack in FPGA
 *
 * Copyright (C) Accelerat S.r.l.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include "udp_core.h"

/* -------------------------------------------------------------------------- */

MODULE_AUTHOR		("Accelerat S.r.l.");
MODULE_DESCRIPTION	("udp-core - Loadable module for controlling UDP Ethernet Stack in FPGA");
MODULE_LICENSE		("GPL");

/* -------------------------------------------------------------------------- */

static int udp_core_probe(struct platform_device *pdev);
static int udp_core_remove(struct platform_device *pdev);

#ifdef CONFIG_OF

// device tree match table for platform device
static struct of_device_id udp_core_of_match[] = 
{
    { 
        .compatible = "accelerat,udp-core" 
    },
    { 
        /* end of list */ 
    },
};
MODULE_DEVICE_TABLE(of, udp_core_of_match);

#endif

static struct platform_driver udp_core_driver = 
{
    .driver = 
    {
        .name 				= DRIVER_NAME,
        .owner 				= THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table		= udp_core_of_match,
#endif  
    },
    .probe		= udp_core_probe,  // called by the kernel when a matching platform device is found
    .remove		= udp_core_remove, // called when the device is removed or when the driver is unloaded
};

/* -------------------------------------------------------------------------- */

/**
 * The probe function is called when a device matching the compatible node in
 * the device tree is detected (the module should have been already loaded and
 * registered).
 */
static int udp_core_probe(struct platform_device *pdev)
{
    int retval;
    struct udp_core_drv_data* drv_data_p;
    
    pr_info("udp-core: device tree probing.\n");

    if (pdev->name != NULL)
    {
        pr_info("udp-core: found device with name: %s.\n", pdev->name);
    }

    // initialize driver devlink structure
    retval = udp_core_devlink_init(pdev, &drv_data_p);

    if (retval < 0)
    {
        pr_err("udp-core: unable to initialize devlink. abort.\n");
        goto init_fail;
    }

    // set driver specific data struct in pdev
    platform_set_drvdata(pdev, drv_data_p);
    drv_data_p->pfdev = pdev;

    // initialize the register memory map
    retval = udp_core_devmem_init(pdev);

    if (retval < 0)
    {
        pr_err("udp-core: initialization of device i/o failed. abort.\n");
        goto init_fail;
    }

    // initialize driver devlink memregion for debugging
    retval = udp_core_devlink_init_region(pdev);

    if (retval < 0)
    {
        pr_err("udp-core: unable to initialize devlink region. abort.\n");
        goto init_fail;
    }

    // initialize the interrupt subsys (register IRQs with handlers)    
    retval = udp_core_irq_init(pdev);

    if (retval < 0)
    {
        pr_err("udp-core: initialization of IRQ subsys failed. abort.\n");
        goto init_fail;
    }

    // allocate and initialize network device
    retval = udp_core_netdev_init(pdev);

    if (retval < 0)
    {
        pr_err("udp-core: unable to initialize netdev. abort.\n");
        goto init_fail;
    }

    pr_info("udp-core: probe succeeded.\n");
    return 0;

init_fail:
    udp_core_remove(pdev);
    return retval;
}

/**
 * The remove function is called when the device is removed or when the driver
 * is unloaded. It cleans up all data structure and frees all resources, such as
 * IRQs and netdev.
 */
static int udp_core_remove(struct platform_device* pdev)
{
    pr_info("udp-core: removing device.\n");
   
    udp_core_netdev_deinit(pdev);
    udp_core_irq_deinit(pdev);
    udp_core_devlink_deinit(pdev);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int __init udp_core_mod_init(void)
{
    int retval;

    pr_info("udp-core: initializing kernel module.\n");
    
    /**
     * Registering the platform driver for udp_core.
     * When probe fails, platform_driver_register(..) returns 0
     * Probe error is always dropped because the kernel keep trying to bind 
     * devices to other drivers
     */
    retval = platform_driver_register(&udp_core_driver);

    if (retval < 0)
    {
        pr_err("udp-core: unable to register platform driver.");
        return retval;
    }

    return 0;
}

static void __exit udp_core_mod_exit(void)
{
    platform_driver_unregister(&udp_core_driver);
    pr_info("udp-core: unregistered kernel module.\n");
}

/* -------------------------------------------------------------------------- */

module_init(udp_core_mod_init);
module_exit(udp_core_mod_exit);
