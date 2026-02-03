// SPDX-License-Identifier: GPL-2.0+
/* udp_core_devlink.c
 *
 * Expose device information through devlink APIs
 *
 * Copyright (C) Accelerat S.r.l.
 */

#include <net/devlink.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include "udp_core.h"

u16 default_opened_sockets[] = DEFAULT_OPENED_SOCKETS;

static int udp_core_devlink_parse_open_sockets(
    const char* str,
    struct udp_core_open_ports* open_ports
)
{
    char *tok, *cur;
    unsigned int i = 0;
    unsigned long port;
    unsigned int slen;
    char udp_core_devlink_opened_ports_buffer[__DEVLINK_PARAM_MAX_STRING_VALUE] = {0};

    slen = strlen(str);
    strscpy(udp_core_devlink_opened_ports_buffer, str, slen + 1);
    cur = udp_core_devlink_opened_ports_buffer;

    while ((tok = strsep(&cur, ",")) != NULL && i < MAX_UDP_PORTS) 
    {
        if (kstrtoul(tok, 10, &port) || port > 65535)
            return -EINVAL;

        open_ports->port_opened[i++] = (u16)port;
    }

    open_ports->port_opened_num = i;
    return 0;
}

static void udp_core_devlink_output_open_sockets(
    struct udp_core_open_ports* open_ports,
    char* str
)
{
    int i, len = 0;
    char udp_core_devlink_opened_ports_buffer[__DEVLINK_PARAM_MAX_STRING_VALUE] = {0};

    for (i = 0; i < open_ports->port_opened_num; i++) 
    {
        len += scnprintf(
            udp_core_devlink_opened_ports_buffer + len, 
            __DEVLINK_PARAM_MAX_STRING_VALUE - len,
            "%s%u", 
            i ? "," : "", 
            open_ports->port_opened[i]
        );
        
        if (len >= __DEVLINK_PARAM_MAX_STRING_VALUE)
            break;
    }

    memcpy(str, udp_core_devlink_opened_ports_buffer, __DEVLINK_PARAM_MAX_STRING_VALUE);
}

/* -------------------------------------------------------------------------- */

enum udp_core_devlink_param_id 
{
    UDP_CORE_DEVLINK_PARAM_ID_BASE = DEVLINK_PARAM_GENERIC_ID_MAX,
    UDP_CORE_DEVLINK_PARAM_ID_PORT_LOW,
    UDP_CORE_DEVLINK_PARAM_ID_PORT_HIGH,
    UDP_CORE_DEVLINK_PARAM_ID_OPENED_SOCKETS,
    UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_IP,
    UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_MAC,
};

static int udp_core_devlink_get_u16(
    struct devlink *devlink, 
    u32 id,
    struct devlink_param_gset_ctx *ctx
)
{
    struct udp_core_drv_data* drv_data_p = devlink_priv(devlink);

    switch (id) 
    {
        case UDP_CORE_DEVLINK_PARAM_ID_PORT_LOW:
            ctx->val.vu16 = drv_data_p->port_low;
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_PORT_HIGH:
            ctx->val.vu16 = drv_data_p->port_high;
            break;
        default:
            return -EOPNOTSUPP;
    }

    
    return 0;
}

static int udp_core_devlink_set_u16(
    struct devlink *devlink, 
    u32 id,
    struct devlink_param_gset_ctx *ctx
)
{
    struct udp_core_drv_data* drv_data_p = devlink_priv(devlink);

    switch (id) 
    {
        case UDP_CORE_DEVLINK_PARAM_ID_PORT_LOW:
            drv_data_p->port_low = ctx->val.vu16;
            pr_info("udp-core: port-filter low set to %d \n", drv_data_p->port_low);
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_PORT_HIGH:
            drv_data_p->port_high = ctx->val.vu16;
            pr_info("udp-core: port-filter high set to %d \n", drv_data_p->port_high);
            break;
        default:
            return -EINVAL;
    }

    udp_core_netdev_notify_change(drv_data_p->pfdev);
    
    return 0;
}

static int udp_core_devlink_validate_u16(
    struct devlink *devlink, 
    u32 id,
    union devlink_param_value val,
    struct netlink_ext_ack *extack
)
{
    if (val.vu16 > 65536)
    {
        NL_SET_ERR_MSG_MOD(extack, "udp-core: port number should be a short.");
        return -EOPNOTSUPP;
    }
    
    return 0;
}

static int udp_core_devlink_get_string(
    struct devlink *devlink, 
    u32 id,
    struct devlink_param_gset_ctx *ctx
)
{
    struct udp_core_drv_data* drv_data_p;
    unsigned int slen;

    drv_data_p = devlink_priv(devlink);

    switch (id) 
    {
        case UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_IP:
            slen = strlen(drv_data_p->gw_ip);
            strscpy(ctx->val.vstr, drv_data_p->gw_ip, slen + 1);
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_MAC:
            slen = strlen(drv_data_p->gw_mac);
            strscpy(ctx->val.vstr, drv_data_p->gw_mac, slen + 1);
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_OPENED_SOCKETS:
            udp_core_devlink_output_open_sockets(&drv_data_p->open_ports, ctx->val.vstr);
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static int udp_core_devlink_set_string(
    struct devlink *devlink, 
    u32 id,
    struct devlink_param_gset_ctx *ctx
)
{
    struct udp_core_drv_data* drv_data_p;
    unsigned int slen; 

    drv_data_p = devlink_priv(devlink);

    switch (id) 
    {
        case UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_IP:
            slen = strlen(ctx->val.vstr);
            strscpy(drv_data_p->gw_ip, ctx->val.vstr, slen + 1);
            pr_info("udp-core: gateway IP set to %s \n", drv_data_p->gw_ip);
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_MAC:
            slen = strlen(ctx->val.vstr);
            strscpy(drv_data_p->gw_mac, ctx->val.vstr, slen + 1);
            pr_info("udp-core: gateway MAC set to %s \n", drv_data_p->gw_mac);
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_OPENED_SOCKETS:
            udp_core_devlink_parse_open_sockets(ctx->val.vstr, &drv_data_p->open_ports);
            pr_info("udp-core: opened sockets %d - set %s \n", drv_data_p->open_ports.port_opened_num, ctx->val.vstr);
            break;
        default:
            return -EINVAL;
    }

    udp_core_netdev_notify_change(drv_data_p->pfdev);

    return 0;
}

static int udp_core_devlink_validate_string(
    struct devlink *devlink, 
    u32 id,
    union devlink_param_value val,
    struct netlink_ext_ack *extack
)
{
    unsigned int slen;
    
    switch (id) 
    {
        case UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_IP:
            slen = strlen(val.vstr);
            if (slen > INET_ADDRSTRLEN)
            {
                NL_SET_ERR_MSG_MOD(extack, "udp-core: gateway ip is misconfigured");
                return -EINVAL;
            }
            break;
        case UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_MAC:
            slen = strlen(val.vstr);
            if (slen > ETH_ADDR_STR_LEN)
            {
                NL_SET_ERR_MSG_MOD(extack, "udp-core: gateway mac is misconfigured");
                return -EINVAL;
            }
            break;
        default:
            return -EINVAL;
    }

    return 0;
}


static const struct devlink_param udp_core_devlink_params[] = 
{
    DEVLINK_PARAM_DRIVER(
        UDP_CORE_DEVLINK_PARAM_ID_PORT_LOW, 
        "PORT_RANGE_LOWER", 
        DEVLINK_PARAM_TYPE_U16,
        BIT(DEVLINK_PARAM_CMODE_RUNTIME),
        udp_core_devlink_get_u16,
        udp_core_devlink_set_u16, 
        udp_core_devlink_validate_u16
    ),
    DEVLINK_PARAM_DRIVER(
        UDP_CORE_DEVLINK_PARAM_ID_PORT_HIGH, 
        "PORT_RANGE_UPPER", 
        DEVLINK_PARAM_TYPE_U16,
        BIT(DEVLINK_PARAM_CMODE_RUNTIME),
        udp_core_devlink_get_u16,
        udp_core_devlink_set_u16, 
        udp_core_devlink_validate_u16
    ),
    DEVLINK_PARAM_DRIVER(
        UDP_CORE_DEVLINK_PARAM_ID_OPENED_SOCKETS, 
        "OPENED_SOCKETS", 
        DEVLINK_PARAM_TYPE_STRING,
        BIT(DEVLINK_PARAM_CMODE_RUNTIME),
        udp_core_devlink_get_string,
        udp_core_devlink_set_string,
        NULL
    ),
    DEVLINK_PARAM_DRIVER(
        UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_IP, 
        "GATEWAY_IP", 
        DEVLINK_PARAM_TYPE_STRING,
        BIT(DEVLINK_PARAM_CMODE_RUNTIME),
        udp_core_devlink_get_string,
        udp_core_devlink_set_string, 
        udp_core_devlink_validate_string
    ),
    DEVLINK_PARAM_DRIVER(
        UDP_CORE_DEVLINK_PARAM_ID_GATEWAY_MAC, 
        "GATEWAY_MAC", 
        DEVLINK_PARAM_TYPE_STRING,
        BIT(DEVLINK_PARAM_CMODE_RUNTIME),
        udp_core_devlink_get_string,
        udp_core_devlink_set_string, 
        udp_core_devlink_validate_string
    ),
};

/* -------------------------------------------------------------------------- */

static int udp_core_devlink_info_get(
        struct devlink *devlink,
        struct devlink_info_req *req, 
        struct netlink_ext_ack *extack
    )
{
    struct udp_core_drv_data* drv_data_p;   // driver-specific data structure

    drv_data_p = devlink_priv(devlink);

    pr_info("udp-core: requested devlink info\n");
    
    devlink_info_version_running_put(req, "fw", "1.0.0");

    return 0;
}

static int udp_core_devlink_region_snapshot(
        struct devlink *devlink, 
        const struct devlink_region_ops *ops, 
        struct netlink_ext_ack *extack, 
        u8 **data
    )
{
    u32 offset;
    u8* entry;
    struct udp_core_drv_data* drv_data_p;

    entry = kmalloc(RBTC_CTRL_LAST_ADDR, GFP_KERNEL);
    
    if (entry == NULL)
    {
        return -ENOMEM;
    }

    drv_data_p = devlink_priv(devlink);

    // dump and fill device registers
    for (offset = 0; offset < RBTC_CTRL_LAST_ADDR; offset = offset + REGS_STRIDE)
    {
        udp_core_devmem_read_register(
                drv_data_p->pfdev, 
                offset, 
                (u32*)(&entry[offset])
            );
    }

    *data = entry;
    return 0;
}

static const struct devlink_ops udp_core_devlink_ops = 
{
    .info_get = udp_core_devlink_info_get,
};

static struct devlink_region_ops udp_core_devlink_region_ops =
{
    .name = "registers",
    .snapshot = udp_core_devlink_region_snapshot,
    .destructor = kfree,
};

/* -------------------------------------------------------------------------- */

int udp_core_devlink_init(struct platform_device* pdev, struct udp_core_drv_data** drv_data_p)
{
    int err;
    struct device* dev;
    struct devlink* udp_core_devlink;

    err = 0;
    dev = &pdev->dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    udp_core_devlink = devlink_alloc(
            &udp_core_devlink_ops, 
            sizeof(struct udp_core_drv_data), 
            dev
        );
#else
    udp_core_devlink = devlink_alloc(
            &udp_core_devlink_ops, 
            sizeof(struct udp_core_drv_data)
        );
#endif

    if (udp_core_devlink == NULL)
    {
        pr_err("udp-core: unable to allocate devlink");
        return -EINVAL;
    }

    *drv_data_p = devlink_priv(udp_core_devlink);
    memset(*drv_data_p, 0, sizeof(struct udp_core_drv_data));

    /**
     * Set user-modifiable params with their default values.
     * Afterwards, they can be changed via devlink params
     */
    (*drv_data_p)->port_low = DEFAULT_PORT_RANGE_LOWER;
    (*drv_data_p)->port_high = DEFAULT_PORT_RANGE_UPPER;
    (*drv_data_p)->open_ports.port_opened_num = (sizeof(default_opened_sockets) / sizeof(u16));
    memcpy((*drv_data_p)->open_ports.port_opened, default_opened_sockets, sizeof(default_opened_sockets));
    memcpy((*drv_data_p)->gw_ip, GW_IP, sizeof(GW_IP));
    memcpy((*drv_data_p)->gw_mac, GW_MAC, sizeof(GW_MAC));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
    devlink_register(udp_core_devlink);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    err = devlink_register(udp_core_devlink);
#else
    err = devlink_register(udp_core_devlink, dev);
#endif

    if (err) 
    {
        pr_err("udp-core: unable to register devlink");
        devlink_free(udp_core_devlink);
        return -EINVAL;
    }

    err = devlink_params_register(
        udp_core_devlink, 
        udp_core_devlink_params, 
        ARRAY_SIZE(udp_core_devlink_params)
    );

    if (err) 
    {
        pr_err("udp-core: unable to register devlink params");
        devlink_unregister(udp_core_devlink);
        devlink_free(udp_core_devlink);
        return -EINVAL;
    }

    #if LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0)
    devlink_params_publish(udp_core_devlink);
    #endif

    return 0;
}

int udp_core_devlink_init_region(struct platform_device* pdev)
{
    struct udp_core_drv_data* drv_data_p;
    struct devlink* udp_core_devlink;

    drv_data_p = platform_get_drvdata(pdev);
    udp_core_devlink = priv_to_devlink(drv_data_p);

    udp_core_devlink_region_ops.priv = (void*)drv_data_p;

    drv_data_p->region = devlink_region_create(
            udp_core_devlink, 
            &udp_core_devlink_region_ops,
            1, 
            RBTC_CTRL_LAST_ADDR
        );

    if (drv_data_p->region == NULL) 
    {
        pr_err("udp-core: unable to create devlink region");
        return -EINVAL;
    }

    return 0;
}

void udp_core_devlink_deinit(struct platform_device* pdev)
{
    struct devlink* udp_core_devlink;
    struct udp_core_drv_data* drv_data_p;

    drv_data_p = platform_get_drvdata(pdev);

    if (drv_data_p == NULL)
    {
        return;
    }
    
    udp_core_devlink = priv_to_devlink(drv_data_p);

    if (drv_data_p->region != NULL)
    {
        devlink_region_destroy(drv_data_p->region);
    }

    if (udp_core_devlink != NULL)
    {
        devlink_params_unregister(
            udp_core_devlink, 
            udp_core_devlink_params, 
            ARRAY_SIZE(udp_core_devlink_params)
        );
        devlink_unregister(udp_core_devlink);
        devlink_free(udp_core_devlink);
    }
}
