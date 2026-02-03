// SPDX-License-Identifier: GPL-2.0+

/* udp-core-regs.c
 *
 * Handling of device memory map and registers
 *
 * Copyright (C) Accelerat S.r.l.
 */

#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "udp_core.h"

#define REG_DUMP(map, regname)                              \
    {u32 val;                                               \
    regmap_read(map, regname, &val);                        \
    pr_info("%-50s: %u\n", #regname, val);}

static const struct regmap_config udp_core_regmap_config = 
{
    .reg_bits = REGS_BITS,           // 4 bytes registers
    .val_bits = REGS_VAL_BITS,       // size of the data values being transferred to/from the registers
    .reg_stride = REGS_STRIDE,       // total length between start of registers
};

void udp_core_devmem_dump_registers(struct platform_device* pdev)
{
    struct udp_core_drv_data* drv_data_p;
    
    drv_data_p = platform_get_drvdata(pdev);
    
    if (drv_data_p == NULL || drv_data_p->map == NULL) 
    {
        pr_err("udp-core: dump failed, unable to read drv data or device map.\n");
        return;
    }

    pr_info("udp-core: devmem register status \n");

    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_AP_CTRL_0_N_P);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_RES_0_Y_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_MAC_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_MAC_1_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_GW_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_SNM_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_IP_LOC_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_SHMEM_0_N_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFRX_OFFSET_0_N_I);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFRX_PUSH_IRQ_0_IRQ);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFTX_TAIL_0_N_I);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFTX_EMPTY_0_N_I);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_BUFTX_POPPED_0_N_I);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_ISR0);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_IER0);
    REG_DUMP(drv_data_p->map, RBTC_CTRL_ADDR_GIE);
}

int udp_core_devmem_read_register(struct platform_device* pdev, u32 reg, u32* value)
{
    struct udp_core_drv_data* drv_data_p;
    
    drv_data_p = platform_get_drvdata(pdev);
    
    if (drv_data_p == NULL || drv_data_p->map == NULL) 
    {
        pr_err("udp-core: invalid driver data or regmap.\n");
        return -EINVAL;
    }

    return regmap_read(drv_data_p->map, reg, value);
}

int udp_core_devmem_write_register(struct platform_device* pdev, u32 reg, u32 value)
{
    struct udp_core_drv_data* drv_data_p;
    
    drv_data_p = platform_get_drvdata(pdev);
    
    if (drv_data_p == NULL || drv_data_p->map == NULL) 
    {
        pr_err("udp-core: invalid driver data or regmap.\n");
        return -EINVAL;
    }

    return regmap_write(drv_data_p->map, reg, value);
}

int udp_core_devmem_init(struct platform_device* pdev)
{
    void* base;
    struct udp_core_drv_data* drv_data_p;

    base = devm_platform_ioremap_resource(pdev, 0); 

    if (IS_ERR(base))
    {
        pr_err("udp-core: unable to remap device base address.\n");
        return -EIO;
    }

    drv_data_p = platform_get_drvdata(pdev);
    
    drv_data_p->map = devm_regmap_init_mmio(
        &pdev->dev, 
        base, 
        &udp_core_regmap_config
    );

    if (IS_ERR(drv_data_p->map)) 
    {
        pr_err("udp-core: failed to initialize regmap.\n");
        return -EINVAL;
    }

    return 0;
}