// SPDX-License-Identifier: GPL-2.0+

/* udp-core-netdev.c
 *
 * Management of Network operations
 *
 * Copyright (C) Accelerat S.r.l.
 */

#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/ip.h>
#include <linux/dma-mapping.h>
#include <linux/ethtool.h>
#include <linux/string.h>
#include <net/route.h>
#include <net/addrconf.h>
#include <linux/inet.h>

#include "udp_core.h"

static void update_arp_table(struct platform_device* pdev)
{
    struct neighbour *neigh;
    struct udp_core_drv_data* drv_data_p;
    u32 resolved_ip;
    u8 resolved_mac[ETH_ALEN];

    drv_data_p = platform_get_drvdata(pdev);

    resolved_ip = in_aton(drv_data_p->gw_ip);
    mac_pton(drv_data_p->gw_mac, resolved_mac);

    // add entry to the kernel ARP table
    neigh = neigh_lookup(&arp_tbl, &resolved_ip, drv_data_p->ndev);

    if (!neigh)
        neigh = neigh_create(&arp_tbl, &resolved_ip, drv_data_p->ndev);

    if (neigh) 
    {
        neigh_update(neigh, resolved_mac, NUD_REACHABLE,
                     NEIGH_UPDATE_F_OVERRIDE | NEIGH_UPDATE_F_WEAK_OVERRIDE, 0);
        neigh_release(neigh);
    }
}

static int udp_core_find_default_gateway(struct net_device* dev, struct in_ifaddr* ifa, u32* gw4)
{
    struct flowi4 fl4;
    struct rtable *rt;

    // clear the flowi4 structure
    memset(&fl4, 0, sizeof(fl4));

    // set destination to 0.0.0.0 for default route lookup
    fl4.daddr = htonl(0x00000000);
    fl4.saddr = ifa->ifa_address;
    fl4.flowi4_oif = dev->ifindex;

    // route lookup using ip_route_output_key() function
    rt = ip_route_output_key(dev_net(dev), &fl4);

    if (IS_ERR(rt)) 
    {
        pr_info("udp-core: failed to find route for 0.0.0.0/0\n");
        return -ENOENT;
    }

    pr_info("udp-core: search: dst %pI4, src %pI4 \n", 
        &fl4.daddr, &fl4.saddr);
    pr_info("udp-core: found route: dst %pI4, gw %pI4, uses_gateway: %d, gw_family: %d\n", 
        &rt->dst, &rt->rt_gw4, rt->rt_uses_gateway, rt->rt_gw_family);

    if (rt->rt_uses_gateway && rt->rt_gw_family == AF_INET) 
    {
        pr_info("udp-core: gateway for device is: %pI4\n", &rt->rt_gw4);
        ip_rt_put(rt);
        *gw4 = rt->rt_gw4;
        return 0;
    }

    pr_info("udp-core: no default gateway ipv4 for device\n");
    ip_rt_put(rt);
    return -ENOENT;
}

/* -------------------------------------------------------------------------- */

static void udp_core_netdev_clear_socket(struct net_device* netdev, uint32_t buffer_id) 
{
    struct udp_core_netdev_priv* priv;
    uint32_t value;
    uint32_t mask_clear;

    priv = netdev_priv(netdev);
    mask_clear = ~(1 << BUFFER_OPENSOCK_OFFSET);
    
    udp_core_devmem_read_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        &value
    );

    udp_core_devmem_write_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        (value & mask_clear)
    );
}

static void udp_core_netdev_open_socket(struct net_device* netdev, uint32_t buffer_id) 
{
    struct udp_core_netdev_priv* priv;
    uint32_t value;
    uint32_t mask_set;

    priv = netdev_priv(netdev);
    mask_set = 1 << BUFFER_OPENSOCK_OFFSET;
    
    udp_core_devmem_read_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        &value
    );

    udp_core_devmem_write_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        (value | mask_set)
    );

    udp_core_devmem_read_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        &value
    );

    pr_info("udp-core: opened socket %d \n", buffer_id);
}

static void udp_core_netdev_notify_pop_rx(struct net_device* netdev, uint32_t buffer_id) 
{
    struct udp_core_netdev_priv* priv;
    uint32_t value;
    uint32_t mask_clear, mask_set;

    priv = netdev_priv(netdev);

    mask_clear = ~(1 << BUFFER_POPPED_OFFSET);
    mask_set = 1 << BUFFER_POPPED_OFFSET;

    udp_core_devmem_read_register(
            priv->pfdev, 
            BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
            &value
        );
    
    // clear pop
    udp_core_devmem_write_register(
            priv->pfdev, 
            BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
            (value & mask_clear)
        );
    
    // set pop
    udp_core_devmem_write_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        (value | mask_set)
    );
    
    // clear pop
    udp_core_devmem_write_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        (value & mask_clear)
    );
}

static void get_buffer_rx_param(struct net_device* netdev, u32 buffer_id, struct RBTC_CTRL_BUFRX* reg) 
{
    struct udp_core_netdev_priv* priv;

    priv = netdev_priv(netdev);

    udp_core_devmem_read_register(
        priv->pfdev, 
        BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), 
        (u32*)reg
    );
}

static void udp_core_netdev_free_memory(struct platform_device* pdev)
{
    struct udp_core_netdev_priv* priv;
    struct udp_core_drv_data* drv_data;

    drv_data = platform_get_drvdata(pdev);
    priv = netdev_priv(drv_data->ndev);

    dma_free_coherent(&pdev->dev, BUFFERS_TOTAL_SIZE, priv->virt_dma_area, priv->phys_dma_area);
}

static int udp_core_netdev_alloc_memory(struct platform_device* pdev)
{
    void* cpu_addr;
    dma_addr_t dma_handle;
    struct udp_core_netdev_priv* priv;
    struct udp_core_drv_data* drv_data;

    drv_data = platform_get_drvdata(pdev);
    cpu_addr = dma_alloc_noncoherent(&pdev->dev, BUFFERS_TOTAL_SIZE, &dma_handle, DMA_BIDIRECTIONAL, GFP_KERNEL);
    
    if (!cpu_addr) 
    {
        pr_err("udp-core: failed to allocate DMA buffer. \n");
        return -ENOMEM;
    }

    priv = netdev_priv(drv_data->ndev);
    priv->phys_dma_area = dma_handle;
    priv->virt_dma_area = cpu_addr;

    return 0;
}

/* -------------------------------------------------------------------------- */

/**
 * The following functions are registered as netdevops handlers. 
 */

static int udp_core_ndo_open(struct net_device* netdev)
{
    struct udp_core_drv_data* drv_data_p;
    struct udp_core_netdev_priv* priv;
    unsigned int buffer_rx_index;
    unsigned int socket_index;

    priv = netdev_priv(netdev);

    // assert device reset
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 1);

    // allocate memory for the data
    if (udp_core_netdev_alloc_memory(priv->pfdev) != 0)
    {
        pr_err("udp-core: unable to allocate contiguos memory for data\n");
        return -ENOMEM;
    }
    
    drv_data_p = platform_get_drvdata(priv->pfdev);

    // write physical mem address to the device reg
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_SHMEM_0_N_O, priv->phys_dma_area);

    // open ports
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O, drv_data_p->port_low);
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O, drv_data_p->port_high);

    // empty and clear rx buffers
    for (buffer_rx_index = 0; buffer_rx_index < MAX_UDP_PORTS; buffer_rx_index++)
    {
        udp_core_netdev_notify_pop_rx(netdev, buffer_rx_index);
        udp_core_netdev_clear_socket(netdev, buffer_rx_index);
    }

    // open sockets
    for (socket_index = 0; socket_index < drv_data_p->open_ports.port_opened_num; socket_index++)
    {
        udp_core_netdev_open_socket(netdev, drv_data_p->open_ports.port_opened[socket_index]);
    }
    
    // clear tx push buffer
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);
        
    // enable interrupts
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_IER0, 1);
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_GIE, 1);
    
    // deassert device reset
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 0); 

    // netif attach
    netif_device_attach(netdev);
    netif_tx_start_all_queues(netdev);

    // enable napi
    napi_enable(&priv->napi);

    // link is up!
    netif_carrier_on(netdev);

    return 0;
}

static int udp_core_ndo_stop(struct net_device *netdev)
{
    unsigned int buffer_rx_index;
    struct udp_core_netdev_priv* priv;

    priv = netdev_priv(netdev);

    // assert device reset
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 1);

    // free dma allocated memory for the data
    udp_core_netdev_free_memory(priv->pfdev);
    
    // reset shmem address
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_SHMEM_0_N_O, 0x0);

    // empty and clear all rx buffers
    for (buffer_rx_index = 0; buffer_rx_index < MAX_UDP_PORTS; buffer_rx_index++)
    {
        udp_core_netdev_notify_pop_rx(netdev, buffer_rx_index);
        udp_core_netdev_clear_socket(netdev, buffer_rx_index);
    }
    
    // clear tx push buffer
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);
        
    // disable interrupts
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_IER0, 0);
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_GIE, 0);
    
    // deassert device reset
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 0); 

    // disable napi
    napi_disable(&priv->napi);

    // link is down!
    netif_carrier_off(netdev);

    return 0;
}

static netdev_tx_t udp_core_ndo_start_xmit(struct sk_buff* skb, struct net_device* netdev)
{
    u32 tx_slot_full;
    u32 offset;
    int pkt_composed;
    struct udp_core_raw_packet udp_packet;
    struct udp_core_netdev_priv* priv;

    priv = netdev_priv(netdev);

    #ifdef NON_RAW_USAGE_ENABLED
    /**
     * TODO: When enabling classic sockets, the kernel network stack shall know
     * the MAC address of the recipient, otherwise it will not forward the
     * packet to L2 drivers. A hotfix consists of manually update kernel ARP
     * table for known IP addresses. Find a way to avoid that (or make it 
     * stable through an external configuration).
     */
    update_arp_table(priv->pfdev);
    #endif

    // compose the packet (populate udp packet using skb)
    pkt_composed = udp_core_pkt_compose(skb, &udp_packet);

    if (pkt_composed < 0)
    {
        // pr_err("udp-core: tried to send out a non valid packet - discarded \n");
        netdev->stats.tx_dropped++;
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }


    udp_core_devmem_read_register(
            priv->pfdev, 
            RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I, 
            &tx_slot_full
        );

    if (tx_slot_full)
    {
        pr_info("udp-core: tried to send out a packet - TX is busy! \n");
        return NETDEV_TX_OK;
    }

    udp_core_devmem_read_register(
            priv->pfdev, 
            RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I, 
            &offset
        );

    offset = BUFFER_TX_OFFSET_BYTES + (offset * BUFFER_ELEM_MAX_SIZE_BYTES);

    // copy header
    memcpy(
            ((u8*)priv->virt_dma_area)+offset, 
            &udp_packet, 
            PACKET_HEADER_SIZE_BYTES
        );

    // copy payload
    memcpy(
            ((u8*)priv->virt_dma_area)+offset+PACKET_HEADER_SIZE_BYTES, 
            udp_packet.payload,
            udp_packet.payload_size_bytes
        );

    // sync 
    dma_sync_single_for_device(&(priv->pfdev->dev), (dma_addr_t)((u8*)priv->phys_dma_area)+offset, udp_packet.payload_size_bytes+PACKET_HEADER_SIZE_BYTES, DMA_TO_DEVICE);

    // transmit!
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 1);
    udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);

    // update netif stats
    netdev->stats.tx_packets++;
    netdev->stats.tx_bytes += udp_packet.payload_size_bytes;

    // free the buffer
    dev_kfree_skb(skb);

    return NETDEV_TX_OK;
}

static int udp_core_rx_poll(struct napi_struct *napi, int budget)
{
    struct udp_core_netdev_priv* priv;
    struct udp_core_drv_data* drv_data_p;

    unsigned int port;
    unsigned int buffer_id;
    struct RBTC_CTRL_BUFRX reg;
    void* packet_pointer;
    void* payload_pointer;
    struct sk_buff *skb;
    struct udp_core_raw_packet raw_udp_packet;
    int processed;
    bool packet_found;

    priv = container_of(napi, struct udp_core_netdev_priv, napi);
    drv_data_p = platform_get_drvdata(priv->pfdev);

    processed = 0;
    port = 0;

    do {
        packet_found = false;
    
        for (port = 0; port < drv_data_p->open_ports.port_opened_num; port++) 
        {
            if (processed >= budget)
                break;
    
            buffer_id = drv_data_p->open_ports.port_opened[port];
            get_buffer_rx_param(priv->ndev, buffer_id, &reg);
    
            if (reg.empty)
                continue;
    
            // copy packet from memory
            packet_found = true;
    
            packet_pointer = 
                (void*) BUFFER_RX_SLOT_HDR_DATA(buffer_id, reg.tail, priv->virt_dma_area);
            payload_pointer = 
                (void*) BUFFER_RX_SLOT_PAYLOAD_DATA(buffer_id, reg.tail, priv->virt_dma_area);            
    
            memcpy(&raw_udp_packet, packet_pointer, PACKET_HEADER_SIZE_BYTES);
            raw_udp_packet.payload = payload_pointer;
    
            skb = netdev_alloc_skb(priv->ndev, raw_udp_packet.payload_size_bytes + PKT_HLEN);
            if (!skb)
                break;
    
            udp_core_pkt_decompose(skb, &raw_udp_packet);
            napi_gro_receive(napi, skb);
    
            priv->ndev->stats.rx_packets++;
            priv->ndev->stats.rx_bytes += (raw_udp_packet.payload_size_bytes + PKT_HLEN);
    
            udp_core_netdev_notify_pop_rx(priv->ndev, buffer_id);
            processed++;
        }
    } 
    while (packet_found && processed < budget);

    if (processed < budget) 
    {
        // all packets processed, complete NAPI
        napi_complete(napi);

        // ee-enable the interrupt now that we are done processing
        udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_GIE, 1);
    }

    return processed;
}

static void udp_core_ndo_set_rx_mode(struct net_device* dev) 
{
    return; // nothing to do!
}

static int udp_core_ndo_set_mac_address(struct net_device* dev, void* addr) 
{
    struct sockaddr* sockaddr;
    struct udp_core_netdev_priv* priv;

    uint64_t mac64;
    int ret_low, ret_high;
    uint32_t mac_low, mac_high;

    sockaddr = addr;

    pr_info("udp-core: changing MAC address to %pM \n", sockaddr->sa_data);

    if (!is_valid_ether_addr((u8 *)sockaddr->sa_data))
    {
        pr_err("udp-core: MAC address not valid.\n");
        return -EADDRNOTAVAIL;
    }
    
    priv = netdev_priv(dev);
    
    mac64 = ether_addr_to_u64((u8 *)sockaddr->sa_data);
    mac_low = mac64 & 0xFFFFFFFF;
    mac_high = (mac64 >> 32) & 0xFFFFFFFF;

    // update mac address
    ret_low = udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_MAC_0_N_O, mac_low);
    ret_high = udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_MAC_1_N_O, mac_high);

    if (ret_low || ret_high)
    {
        pr_info("udp-core: unable to set device MAC address \n");
        return -EINVAL;
    }

    pr_info("udp-core: changed MAC address to %pM \n", sockaddr->sa_data);

    memcpy((void*)dev->dev_addr, sockaddr->sa_data, dev->addr_len);
    return 0;
}

static const struct net_device_ops udp_core_netdev_ops = 
{
    .ndo_open		        = udp_core_ndo_open,
    .ndo_stop		        = udp_core_ndo_stop,
    .ndo_start_xmit		    = udp_core_ndo_start_xmit,
    .ndo_set_rx_mode        = udp_core_ndo_set_rx_mode,
    .ndo_set_mac_address	= udp_core_ndo_set_mac_address,
};

/* -------------------------------------------------------------------------- */

/**
 * Register ethtool ops for the device. Ethool is heavily used by Ubuntu, 
 * especially when dealing with carrier detection. 
 */

static u32 udp_core_ethtools_get_link(struct net_device* netdev)
{
    return netif_carrier_ok(netdev) ? 1 : 0;
}

static const struct ethtool_ops udp_core_ethtool_ops = 
{
    .get_link = udp_core_ethtools_get_link,
};

/* -------------------------------------------------------------------------- */

/**
 * Register inet-notifier. When the kernel notifies change to inet addr, let the
 * driver configure proper addresses.
 */

static int udp_core_notifier_call(struct notifier_block *nb, unsigned long event, void *ptr)
{
    int ret;
    u32 gw4;
	struct in_ifaddr* if4;
	struct net_device* dev;
    struct net_device *target_dev;
    struct udp_core_netdev_priv* priv;
    struct udp_core_drv_data* drv_data_p;

    if (ptr == NULL) 
    {
        return NOTIFY_DONE;
    }

    if4 = (struct in_ifaddr*) ptr;

    if (if4->ifa_dev == NULL || if4->ifa_dev->dev == NULL)
    {
        return NOTIFY_DONE;
    }

    dev = (struct net_device *)if4->ifa_dev->dev;
    target_dev = dev_get_by_name(&init_net, IF_NAME);

    if (target_dev == NULL) 
    {
        // pr_debug("udp-core: failed to find fpga netdevice with name: %s\n", IF_NAME);
        return NOTIFY_DONE;
    }

    pr_info("udp-core: received ipv4 change notification. \n");

    priv = netdev_priv(dev);

    // pfdev still not initialized or already cleaned up
    if (priv->pfdev == NULL)
    {
        return NOTIFY_DONE;
    }

    drv_data_p = platform_get_drvdata(priv->pfdev);

    // ignore the event, it is not targetting fpga device    
    if (dev != target_dev) 
    {
        dev_put(target_dev);
        return NOTIFY_DONE;
    }

    ret = udp_core_find_default_gateway(dev, if4, &gw4);

    if (ret != 0)
    {
        // gw4 = (if4->ifa_address & if4->ifa_mask) | (1 << 25);
        in4_pton(drv_data_p->gw_ip, strlen(drv_data_p->gw_ip), (u8*)&gw4, '\0', NULL);
    }

	switch (event) 
    {
        case NETDEV_UP:

            // assert device reset
            udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 1);
            
            // set ip, mask and gw address
            udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_IP_LOC_0_N_O, ntohl(if4->ifa_address));
            udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_SNM_0_N_O, ntohl(if4->ifa_mask));
            udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_GW_0_N_O, ntohl(gw4));
    
            // deassert device reset
            udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 0);

            #ifdef NON_RAW_USAGE_ENABLED
            /**
             * TODO: When enabling classic sockets, the kernel network stack shall know
             * the MAC address of the recipient, otherwise it will not forward the
             * packet to L2 drivers. A hotfix consists of manually update kernel ARP
             * table for known IP addresses. Find a way to avoid that (or make it 
             * stable through an external configuration).
             */
            update_arp_table(priv->pfdev);
            #endif

            pr_info("udp-core: wrote local IP: %pI4 - Mask: %pI4 - GW: %pI4 \n", &if4->ifa_address, &if4->ifa_mask, &gw4);
            break;
        case NETDEV_DOWN:
            /**
             * NOTE: There is no currently way, in RTL, to keep the device off. The best option, at the moment,
             * is to keep the device reset assrted.
             */
            udp_core_devmem_write_register(priv->pfdev, RBTC_CTRL_ADDR_RES_0_Y_O, 1);
            break;
	}

    dev_put(target_dev);
    return NOTIFY_OK;
}

static struct notifier_block udp_core_inetaddr_notifier = 
{
    .notifier_call = udp_core_notifier_call,
};

/* -------------------------------------------------------------------------- */

int udp_core_netdev_init(struct platform_device* pdev)
{
    int retval;
    struct net_device* netdev;
    struct udp_core_netdev_priv* priv;
    struct udp_core_drv_data* drv_data;
    struct sockaddr addr;
    struct inet6_dev *idev;
    u8 mac_addr[ETH_ALEN] = IF_DEFAULT_MAC_ADDR;

    // allocate and initialize network device
    netdev = alloc_etherdev(sizeof(struct net_device));

    if (netdev == NULL)
    {
        pr_err("udp-core: unable to allocate etherdevice.\n");
        return -ENOMEM;
    }

    // set the interface name
    strscpy(netdev->name, IF_NAME, IFNAMSIZ);

    drv_data = platform_get_drvdata(pdev);
    priv = netdev_priv(netdev);
    memset(priv, 0, sizeof(struct udp_core_netdev_priv));

    drv_data->ndev = netdev;
    priv->ndev = netdev;
    priv->pfdev = pdev;

    SET_NETDEV_DEV(netdev, &pdev->dev);

    netdev->irq = drv_data->irq_descriptor.irqn;
    netdev->netdev_ops = &udp_core_netdev_ops;

    retval = register_netdev(netdev);

    if (retval < 0)
    {
        pr_err("udp-core: unable to register netdevice.\n");
        free_netdev(netdev);
        return retval;
    }

    // set default MAC address
    memcpy(addr.sa_data, mac_addr, ETH_ALEN);
    udp_core_ndo_set_mac_address(netdev, &addr);

    // register to netdev notifications
    register_inetaddr_notifier(&udp_core_inetaddr_notifier); 

    // disable ipv6
    idev = __in6_dev_get(netdev);

    if (idev) 
    {
        idev->cnf.disable_ipv6 = 1;
        pr_info("udp-core: IPv6 disabled on interface. \n");
    }

    // register ethtool ops
    netdev->ethtool_ops = &udp_core_ethtool_ops;

    // init napi structure
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    netif_napi_add(netdev, &priv->napi, udp_core_rx_poll);
    #else
    netif_napi_add(netdev, &priv->napi, udp_core_rx_poll, NAPI_POLL_WEIGHT);
    #endif

    // initially, set the link as off
    netif_carrier_off(netdev);

    return 0;
}

void udp_core_netdev_notify_change(struct platform_device* pdev)
{
    unsigned int socket_index;
    unsigned int gw4;
    struct udp_core_drv_data* drv_data;

    drv_data = platform_get_drvdata(pdev);

    // assert device reset
    udp_core_devmem_write_register(pdev, RBTC_CTRL_ADDR_RES_0_Y_O, 1);

    // close all sockets
    for (socket_index = 0; socket_index < MAX_UDP_PORTS; socket_index++)
    {
        udp_core_netdev_clear_socket(drv_data->ndev, socket_index);
    }

    // open needed ones
    for (socket_index = 0; socket_index < drv_data->open_ports.port_opened_num; socket_index++)
    {
        udp_core_netdev_open_socket(drv_data->ndev, drv_data->open_ports.port_opened[socket_index]);
    }

    // open ports
    udp_core_devmem_write_register(pdev, RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O, drv_data->port_low);
    udp_core_devmem_write_register(pdev, RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O, drv_data->port_high);

    // set gw
    in4_pton(drv_data->gw_ip, strlen(drv_data->gw_ip), (u8*)&gw4, '\0', NULL);
    udp_core_devmem_write_register(pdev, RBTC_CTRL_ADDR_GW_0_N_O, ntohl(gw4));
    
    // deassert device reset
    udp_core_devmem_write_register(pdev, RBTC_CTRL_ADDR_RES_0_Y_O, 0);
}

void udp_core_netdev_deinit(struct platform_device* pdev)
{
    struct udp_core_drv_data* drv_data;
    struct udp_core_netdev_priv* priv;

    drv_data = platform_get_drvdata(pdev);

    if (drv_data == NULL)
    {
        return;
    }

    priv = netdev_priv(drv_data->ndev);

    unregister_inetaddr_notifier(&udp_core_inetaddr_notifier);
    
    netif_napi_del(&priv->napi);

    // unregister and free netdev
    unregister_netdev(drv_data->ndev);
    free_netdev(drv_data->ndev);
}
