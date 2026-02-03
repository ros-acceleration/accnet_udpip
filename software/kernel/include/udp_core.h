// SPDX-License-Identifier: GPL-2.0+
/* udp-core.c
 *
 * Loadable module for controlling UDP Ethernet Stack in FPGA
 *
 * Copyright (C) Accelerat S.r.l.
 */

#ifndef UDP_CORE_H
#define UDP_CORE_H

#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/inet.h>

#include "udp_core_regs.h"

/* Miscellaneous ------------------------------------------------------------ */

#define DRIVER_NAME "udp-core"
#define IF_NAME     "udpip0"
#define IF_DEFAULT_MAC_ADDR                 {0x02, 0x00, 0x00, 0x00, 0x00, 0x00}

/**
 * NOTE: Allows the driver to send/receive packets using classic AF_INET 
 * sockets. Enabling the macro will cause a minor perfomance penalty, because 
 * the SKB structure should be well-formed and the IP checksum valid.
 */
#define NON_RAW_USAGE_ENABLED 1

/* Macros ------------------------------------------------------------------- */

#define ETH_ALEN	        6		        /* Octets in one ethernet addr */
#define ETH_ADDR_STR_LEN    18              /* "xx:xx:xx:xx:xx:xx" + '\0' */
#define ETH_MTU             1500            /* MTU supported on physical IF */

#define IPV4_HLEN (sizeof(struct iphdr))
#define UDP_HLEN (sizeof(struct udphdr))
#define PKT_HLEN (ETH_HLEN + IPV4_HLEN + UDP_HLEN)

#define MAX_PAYLOAD_SIZE    (1500 - IPV4_HLEN - UDP_HLEN)

/* Devlink params default values - Changeable via devlink ------------------- */

#define DEFAULT_PORT_RANGE_LOWER 7400
#define DEFAULT_PORT_RANGE_UPPER 7500
#define DEFAULT_OPENED_SOCKETS {0, 1, 10, 11}

#define GW_IP "192.168.1.2"
#define GW_MAC "02:00:00:00:00:01"
#define GW_MAC_OCTETS {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}

/* Data structures ---------------------------------------------------------- */

struct udp_core_irq_d 
{
    unsigned int 				irqn;
};

struct udp_core_open_ports
{
    u16 port_opened_num;
    u16 port_opened[MAX_UDP_PORTS];
};

struct udp_core_drv_data 
{
    struct device*              dev;
    struct platform_device*     pfdev;
    struct net_device*          ndev;

    struct udp_core_irq_d		irq_descriptor;
    struct miscdevice*          misc_cdev;

    struct regmap*              map;
    struct devlink_region*      region;

    u16                         port_low;
    u16                         port_high;
    struct udp_core_open_ports  open_ports;
    char                        gw_ip[INET_ADDRSTRLEN];
    char                        local_ip[INET_ADDRSTRLEN];
    char                        gw_mac[ETH_ADDR_STR_LEN];
};

struct udp_core_netdev_priv 
{
    struct device*              dev;
	struct net_device*          ndev;
    struct platform_device*     pfdev;

    dma_addr_t                  phys_dma_area;
    void*                       virt_dma_area;
    struct napi_struct          napi;
};

/* Standard packets --------------------------------------------------------- */

struct __attribute__((packed)) udp_packet
{
    unsigned char dest_mac[ETH_ALEN];
    unsigned char src_mac[ETH_ALEN];
    unsigned short ether_type;
    unsigned char ihl : 4;
    unsigned char version : 4;
    unsigned char tos;
    unsigned short total_len;
    unsigned short ident;
    unsigned char flags : 4;
    unsigned short frag_offset : 12;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short checksum;
    unsigned int source_ip;
    unsigned int dest_ip;
    unsigned short source_port;
    unsigned short dest_port;
    unsigned short payload_len;
    unsigned short udp_checksum;
    unsigned char payload[MAX_PAYLOAD_SIZE];
};

struct __attribute__((packed)) eth_packet
{
    unsigned char dest_mac[ETH_ALEN];
    unsigned char src_mac[ETH_ALEN];
    unsigned short ether_type;
    unsigned char payload[MAX_PAYLOAD_SIZE];
};

/* Forward decls ------------------------------------------------------------ */

struct sk_buff;

/* Devlink ------------------------------------------------------------------ */

/**
 * @brief Initialize devlink submodule
 * 
 * This function initialize devlink structure to expose driver information 
 * through netlink APIs. 
 */
int udp_core_devlink_init(struct platform_device* pdev, struct udp_core_drv_data** drv_data_p);

/**
 * @brief Initialize driver devlink memregion for register dumping / debugging
 * 
 * This function initializes a devlink region to be used as a space for the
 * dump of device registers.
 */
int udp_core_devlink_init_region(struct platform_device* pdev);

/**
 * @brief Clean up devlink submodule
 * 
 * This function deallocate devlink structure. 
 */
void udp_core_devlink_deinit(struct platform_device* pdev);

/* IRQs --------------------------------------------------------------------- */

/**
 * @brief Initialize the IRQ submodule of the driver.
 * 
 * The function retrieves all interrupts declared in the device tree and
 * registers them. For each interrupt registered, a descriptor is allocated,
 * containing the mapping between hw irq and irq number in the Linux domain.
 */

int udp_core_irq_init(struct platform_device* pdev);

/**
 * @brief Deinitialize the IRQ submodule of the driver.
 * 
 * For each interrupt descriptor allocated, the function frees it.
 */

void udp_core_irq_deinit(struct platform_device* pdev);

/* Registers ---------------------------------------------------------------- */

/**
 * @brief Map the base address from device tree and initialize the regmap
 * 
 * The function maps the device base address taken from device tree into the
 * kernel address space. Furthermore, configure the device regmap.
 * 
 */
int udp_core_devmem_init(struct platform_device* pdev);

/**
 * @brief Write the given value into the specified register
 * 
 * The function writes the given value into the specified register. Returns a
 * value of zero on success, a negative errno in case of errors.
 * 
 */
int udp_core_devmem_write_register(struct platform_device* pdev, u32 reg, u32 value);

/**
 * @brief Read the specified register and leave it into the given variable
 * 
 * The function reads the specified register and leavs the read value into the
 * given variable. Returns a value of zero on success, a negative errno in case 
 * of errors.
 * 
 */
int udp_core_devmem_read_register(struct platform_device* pdev, u32 reg, u32* value);

/**
 * @brief Read all registers of the device and dump them
 * 
 * The function reads all registers of the device and prints out the value
 * of each register.
 */
void udp_core_devmem_dump_registers(struct platform_device* pdev);

/* Network device ----------------------------------------------------------- */

/**
 * @brief Allocate a etherdev and initialize netdev ops
 * 
 * This function allocates and initialize a network device. Then populate
 * and registers the netdev ops.
 *  
 */
int udp_core_netdev_init(struct platform_device* pdev);

/**
 * @brief Notifies netdev about changes in param configuration
 * 
 * This function should be called to notify the netdev subsystem of the driver
 * that one or more params (i.e. port filter) changed and, therefore, changes
 * should be applied.
 */
void udp_core_netdev_notify_change(struct platform_device* pdev);

/**
 * @brief Deregister the netdev and free the memory
 * 
 * This function deregister the net device and deallocate the descriptor.
 */
void udp_core_netdev_deinit(struct platform_device* pdev);

/**
 * @brief Start data read from device
 * 
 * This function should be called from a IRQ handler to copy data from device 
 * and pass it to userspace.
 */
int udp_core_start_receive(struct platform_device* pdev);

/* Packet management -------------------------------------------------------- */

/**
 * @brief Reads data from skb structure and compose a UDP packet for FPGA
 * 
 * This function should be called when a valid socket buffer structure is 
 * available and before L2 trasmission. It reads data from socket buffer and 
 * composes a valid UDP packet header to be sent out via FPGA device.
 */
int udp_core_pkt_compose(struct sk_buff* skb, struct udp_core_raw_packet* udp_packet);

/**
 * @brief Reads data of a UDP from FPGA device and decompose it for a skb
 * 
 * This function should be called when a UDP packet coming from FPGA is received
 * and available. It reads data from the UDP packet and decompose all info
 * for a socket buffer structure which can be forwarded to upper layers.
 */
void udp_core_pkt_decompose(struct sk_buff* skb, struct udp_core_raw_packet* raw_udp_packet);

#endif /* UDP_CORE_H */