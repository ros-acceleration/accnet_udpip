// SPDX-License-Identifier: GPL-2.0+

/* udp-core-irq.c
 *
 * Management of the IRQ submodule of the driver
 *
 * Copyright (C) Accelerat S.r.l.
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/time64.h>
#include <linux/netdevice.h>

#include "udp_core.h"

static irqreturn_t udp_core_irq_handler(int irq, void *dev)
{
    struct udp_core_drv_data* drv_data_p;
    struct udp_core_netdev_priv* priv;
    
    drv_data_p = dev_get_drvdata(dev);
    priv = netdev_priv(drv_data_p->ndev);

    /**
     * Device interrupt generation is disabled. NAPI, when budget is 
     * exhausted, will enable it again.
     */
    udp_core_devmem_write_register(drv_data_p->pfdev, RBTC_CTRL_ADDR_GIE, 0);

    napi_schedule(&priv->napi);

    udp_core_devmem_write_register(drv_data_p->pfdev, RBTC_CTRL_ADDR_ISR0, 0);

    return IRQ_HANDLED;
}

static int udp_core_register_irq(struct platform_device* pdev)
{
    int irqn;
    int req;

    irqn = platform_get_irq(pdev, 0);

    if (irqn < 0)
    {
        pr_warn("udp-core: unable to retrieve remapped irq.\n");
        return irqn;
    }

    pr_info("udp-core: remapped hw irq -> %d.\n", irqn);
    
    req = request_irq(
            irqn, 
            &udp_core_irq_handler, 
            IRQF_SHARED, 
            DRIVER_NAME, 
            (void*) &(pdev->dev)
        );

    if (req) 
    {
        pr_err("udp-core: could not allocate interrupt %d.\n", irqn);
        return req;
    }

    pr_info("udp-core: registered handler for irq %d.\n", irqn);

    return irqn;
}

/* -------------------------------------------------------------------------- */

int udp_core_irq_init(struct platform_device* pdev)
{
    int irqn;						        // number of re-mapped irq 
    int req;						        // interrupt handler registration request
    int num_irq;					        // total number of declared irq in the tree
    struct udp_core_drv_data* drv_data_p;   // driver-specific data structure

    irqn = 0;
    req = 0;
    num_irq = 0;
    drv_data_p = platform_get_drvdata(pdev);

    // retrieve the number of interrupts from the device tree
    num_irq = platform_irq_count(pdev);
    
    if (num_irq < 1)
    {
        pr_err("udp-core: no irqs available in device-tree.\n");
        return -EINVAL;
    }
    else if (num_irq > 1)
    {
        pr_warn("udp-core: multiple irqs in device-tree, the 1st will be used.\n");
    }

    irqn = udp_core_register_irq(pdev);
    drv_data_p->irq_descriptor.irqn = irqn;

    if (irqn < 0)
    {
        pr_err("udp-core: unable to register irq.\n");
        goto error_irq_not_available;
    }

    return 0;

error_irq_not_available:
    udp_core_irq_deinit(pdev);
    return -EINVAL;
}

void udp_core_irq_deinit(struct platform_device* pdev)
{
    struct udp_core_drv_data* drv_data_p;

    drv_data_p = platform_get_drvdata(pdev);

    if (drv_data_p == NULL)
    {
        return;
    }

    if (drv_data_p->irq_descriptor.irqn != 0) 
    {
        pr_info("udp-core: removing irq: %u.\n",
            drv_data_p->irq_descriptor.irqn);

        free_irq(
            drv_data_p->irq_descriptor.irqn, 
            (void*) &(pdev->dev)
        );        
    }
}
