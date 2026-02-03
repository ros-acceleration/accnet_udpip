// SPDX-License-Identifier: GPL-2.0+

/* udriver.h
 *
 * Device memory map, registers and macro
 *
 * Copyright (C) Acceleration Robotics S.L.U
 * Copyright (C) Accelerat S.r.l.
 */

/****************************************************************************
* header guard
****************************************************************************/

#ifndef UDRIVER_H
#define UDRIVER_H

/****************************************************************************
* driver settings
****************************************************************************/

#define CACHEABLE_MEM   1  /* Set to 0 to use non-cacheable always coherent memory */
#define IRQ_SUPPORT     0  /* Set to 0 to disable IRQ support (needs kernel module) */

/****************************************************************************
* physical memory settings
****************************************************************************/

#define PAGE_SIZE           (64 * 1024)
#define DEVMEM              "/dev/mem"
#define DEVIRQ              "/dev/udp-core-irq"
#define DEVICE_ADDRESS      0xA0010000
#define DEVICE_PAGE_BASE    (DEVICE_ADDRESS & (~(PAGE_SIZE - 1)))
#define DEVICE_PAGE_OFFSET  (DEVICE_ADDRESS-DEVICE_PAGE_BASE)

/****************************************************************************
* general consts and structs
****************************************************************************/

#define ETH_ALEN        6               /* Octets in one ethernet addr	    */
#define INET_ALEN       4               /* Octets in one internet addr      */

#define MAX_TIMESTAMP_SIZE  (16)        /* Timestamp len IRQs device        */

#define UDRIVER_SOCKET_CLOSED   0
#define UDRIVER_SOCKET_OPEN     1

/**
 * The device registers are contiguos and separated by 8 bytes (stride = 0x8). 
 * All registers are 64-bit wide but only the LS 32 bits are used.
 * 
 * Here below you can find an example:
 * 
 *      Register A: 0x0000 (32 bits)
 *      Padding   : 0x0004 (Unused)
 *      Register B: 0x0008 (32 bits)
 *      Padding   : 0x000C (Unused)
 *      Register C: 0x0010 (32 bits)
 */

#define RBTC_CTRL_REG_NUM                   (20)
#define RBTC_CTRL_REG_STRIDE                (8)

#define RBTC_CTRL_ADDR_AP_CTRL_0_N_P        (0x00000000)
#define RBTC_CTRL_ADDR_RES_0_Y_O            (0x00000008)
#define RBTC_CTRL_ADDR_MAC_0_N_O            (0x00000010)
#define RBTC_CTRL_ADDR_MAC_1_N_O            (0x00000018)
#define RBTC_CTRL_ADDR_GW_0_N_O             (0x00000020)
#define RBTC_CTRL_ADDR_SNM_0_N_O            (0x00000028)
#define RBTC_CTRL_ADDR_IP_LOC_0_N_O         (0x00000030)
#define RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O    (0x00000038)
#define RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O    (0x00000040)
#define RBTC_CTRL_ADDR_SHMEM_0_N_O          (0x00000048)
#define RBTC_CTRL_ADDR_ISR0                 (0x00000050)
#define RBTC_CTRL_ADDR_IER0                 (0x00000058)
#define RBTC_CTRL_ADDR_GIE                  (0x00000060)
#define RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I     (0x00000068)
#define RBTC_CTRL_ADDR_BUFTX_TAIL_0_N_I     (0x00000070)
#define RBTC_CTRL_ADDR_BUFTX_EMPTY_0_N_I    (0x00000078)
#define RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I     (0x00000080)
#define RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O   (0x00000088)
#define RBTC_CTRL_ADDR_BUFTX_POPPED_0_N_I   (0x00000090)
#define RBTC_CTRL_ADDR_BUFRX_PUSH_IRQ_0_IRQ (0x00000098)
#define RBTC_CTRL_ADDR_BUFRX_OFFSET_0_N_I   (0x000000A0)

/*
 * Bit Layout of the BUFRX (buffer receive) register (one per socket):
 * 
 *  | Bit(s) | Description                  |
 *  |--------|------------------------------|       
 *  |    0   | popped                       |
 *  |    1   | pushed                       |
 *  |    2   | full                         |
 *  |    3   | empty                        |
 *  |  4-8   | tail                         |
 *  |  9-13  | head                         |
 *  |   14   | socket state (open/closed)   |
 *  |   15   | dummy                        |
 *  | 16-64  | (reserved/unused)            |
 */


#define BUFFER_POPPED_OFFSET    (0)
#define BUFFER_PUSHED_OFFSET    (1)
#define BUFFER_FULL_OFFSET      (2)
#define BUFFER_EMPTY_OFFSET     (3)
#define BUFFER_TAIL_OFFSET      (4)
#define BUFFER_TAIL_UPPER       (8)
#define BUFFER_HEAD_OFFSET      (9)
#define BUFFER_HEAD_UPPER       (13)
#define BUFFER_OPENSOCK_OFFSET  (14)

/**
 * Each RX buffer has a CTRL register. Given that each register is 8-bytes, the 
 * n-th register is located at n-th * 8 + base.
 */

#define BUFFER_RX_CTRL_BASE_OFFSET(index)   \
    (RBTC_CTRL_ADDR_BUFRX_OFFSET_0_N_I + (index) * 8) 

/**
 * Configuration of circular buffer dimension
 * 
 * The following macro should reflect the configuration of circular buffers
 * as defined in the RTL. Indeed, they are used to locate circular buffers
 * limits and offsets, as described below:
 * 
 * MAX_UDP_PORTS:
 *  > port range width (-> number of rx buffers, 1 per port)
 * BUF_*X_LENGTH: 
 *  > number of circular buffer slots
 * BUF_ELEM_MAX_SIZE_BYTES: 
 *  > circular buffer slot width in bytes
 * 
 * BUF_SIZE_BYTES: 
 *  > length of memory dedicated to each circular buffer
 * BUF_TOTAL_SIZE: 
 *  > total length of memory dedicated to all circular buffers (1 rx buffer per 
 *  > udp port / socket + 1 tx buffer)
 * 
 * BUF_RX_OFFSET_BYTES: 
 *  > offset of first rx buffer (end of reg space)
 * BUF_RX_IDX_OFFSET_BYTES: 
 *  > offset of n-th rx buffer
 * BUF_TX_OFFSET_BYTES: 
 *  > offset of tx buffer (after all rx buffers)
 *
 */

#define MAX_UDP_PORTS                   1024

#define BUF_RX_LENGTH                   32
#define BUF_TX_LENGTH                   32
#define BUF_ELEM_MAX_SIZE_BYTES         2048

#define BUF_SIZE_BYTES                  (BUF_RX_LENGTH * BUF_ELEM_MAX_SIZE_BYTES)
#define BUF_TOTAL_SIZE                  (BUF_SIZE_BYTES * (MAX_UDP_PORTS + 1)) 

#define BUF_RX_OFFSET_BYTES             0 
#define BUF_RX_IDX_OFFSET_BYTES(idx)    (BUF_RX_OFFSET_BYTES + idx * BUF_SIZE_BYTES)
#define BUF_TX_OFFSET_BYTES             (BUF_RX_OFFSET_BYTES + MAX_UDP_PORTS * BUF_SIZE_BYTES)

/****************************************************************************
* UDP Protocol - Constants and structures
****************************************************************************/

/**
 * When computing the maximum UDP payload size, we need to account header
 * size introduced by the IP, and UDP headers.
 * 
 * With ethernet frame MTU of 1500 bytes (that represents the maximum size for 
 * the payload of an Ethernet frame) and accounting 20 bytes (assuming no IP 
 * options and IPv4) for IP and 8 bytes for UDP header, it makes 1472 bytes
 * available for the UDP payload.
 */

#define ETH_MTU                 (1500)
#define IP_HDR_LEN              (20)
#define UDP_HDR_LEN             (8)
#define UDP_PAYL_MAX_LEN        (ETH_MTU-IP_HDR_LEN-UDP_HDR_LEN)

/**
 * The UDP packet structure declared below represents the structure understood
 * by the offloading device. The offloading device parses the packet header and
 * then build the UDP packet to be sent out the interface.
 * 
 * IP addresses and port numbers shall be represented in network order.
 */

#define PACKET_WORD_SIZE_BYTES      (8)
#define PACKET_HDR_LENGTH           (5)
#define PACKET_HDR_SIZE_BYTES       (PACKET_HDR_LENGTH * PACKET_WORD_SIZE_BYTES)
#define PACKET_PAYL_SIZE_MAX_LEN    (UDP_PAYL_MAX_LEN / PACKET_WORD_SIZE_BYTES)

struct udp_packet 
{
    uint64_t payload_size_bytes;
    uint64_t source_ip;
    uint64_t source_port;
    uint64_t dest_ip;
    uint64_t dest_port;
    uint64_t* payload;
};

/****************************************************************************
* Public functions
****************************************************************************/

/**
 * Initializes the offloading device with the provided parameters.
 * Returns -1 in case of initialization error (and prints out the root cause),
 * and 0 otherwise.
 */
int udriver_initialize(
    const uint8_t local_mac[ETH_ALEN],
    const uint8_t local_ip[INET_ALEN],
    const uint8_t subnet_mask[INET_ALEN],
    const uint8_t gw_ip[INET_ALEN], 
    uint16_t port_min, 
    uint16_t port_max
);

/**
 * Deinitializes the offloading device and clean up structures and memory.
 */
void udriver_destroy(void);

/**
 * Sets a given port number status (0 for closed socket, 1 for opened socket).
 * Returns -1 in case of error (invalid status / port outside allowed range) or
 * 0 otherwise.
 */
int udriver_set_socket_status(uint32_t port, uint32_t status);

/**
 * Sends a UDP packet. Returns the number of bytes sent or -1 in case of errors.
 */
int udriver_send(struct udp_packet* udp_packet);

/**
 * Receives a UDP packet from the given port. Returns the number of bytes
 * received or -1 in case of errors.
 */
int udriver_recv(struct udp_packet* udp_packet, uint32_t port);

/**
 * Probe a given port to check for data. Returns 1 if a packet is available at
 * the given port or 0 otherwise. This is a non-blocking call.
 */
int udriver_probe_port(uint32_t port);

/**
 * Prints out the offloading device registers - Use it for debugging purposes.
 */
void udriver_print_regs(uint32_t port);

/**
 * Prints out the given UDP packet in user-friendly way. - Use it for debugging
 * purposes
 */
void udriver_print_packet(struct udp_packet* packet);

/**
 * Reads the device register and returns the IP of the local interface
 * as configured (32 bit host order).
 */
uint32_t udriver_get_local_ip(void);

/**
 * Reads the device register and returns the lower port configured
 * (16 bit host order).
 */
uint16_t udriver_get_port_range_low(void);

/**
 * Reads the device register and returns the higher port configured
 * (16 bit host order).
 */
uint16_t udriver_get_port_range_high(void);


#endif  // UDRIVER_H
