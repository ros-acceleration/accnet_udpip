// SPDX-License-Identifier: GPL-2.0+

/* udp_core_regs.h
 *
 * Declaration for device memory map and registers
 *
 * Copyright (C) Accelerat S.r.l.
 */

#ifndef UDP_CORE_REGS_H
#define UDP_CORE_REGS_H

/* -------------------------------------------------------------------------- */

/**
 * The registers are contiguos and separated by 8 bytes (stride = 0x8). All
 * registers are 64-bit wide but only the LS 32 bits are used.
 * 
 * Here below you can find an example:
 * 
 *      Register A: 0x0000 (32 bits)
 *      Padding   : 0x0004 (Unused)
 *      Register B: 0x0008 (32 bits)
 *      Padding   : 0x000C (Unused)
 *      Register C: 0x0010 (32 bits)
 */

#define REGS_BITS       (32)
#define REGS_VAL_BITS   (32)
#define REGS_STRIDE     (8)

/* -------------------------------------------------------------------------- */

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
#define RBTC_CTRL_LAST_ADDR                 (0x000000A8)

/*
 * Bit Layout of the BUFRX Register:
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

struct RBTC_CTRL_BUFRX
{
    u64 popped        : 1;  // Bit 0
    u64 pushed        : 1;  // Bit 1
    u64 full          : 1;  // Bit 2
    u64 empty         : 1;  // Bit 3
    u64 tail          : 5;  // Bits 4-8 (5 bits)
    u64 head          : 5;  // Bits 9-13 (5 bits)
    u64 socket_state  : 1;  // Bit 14
    u64 dummy         : 1;  // Bit 15
    u64 reserved      : 49; // Bits 16-64 (reserved/unused)
};

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
 * BUFFER_*X_LENGTH: 
 *  > number of circular buffer slots
 * BUFFER_ELEM_MAX_SIZE_BYTES: 
 *  > circular buffer slot width in bytes
 * 
 * BUFFER_SIZE_BYTES: 
 *  > length of memory dedicated to each circular buffer
 * BUFFERS_TOTAL_SIZE: 
 *  > total length of memory dedicated to all circular buffers (1 rx buffer per 
 *  > udp port / socket + 1 tx buffer)
 * 
 * BUFFER_RX_OFFSET_BYTES: 
 *  > offset of first rx buffer (end of reg space)
 * BUFFER_RX_INDEX_OFFSET_BYTES: 
 *  > offset of n-th rx buffer
 * BUFFER_TX_OFFSET_BYTES: 
 *  > offset of tx buffer (after all rx buffers)
 *
 */

#define MAX_UDP_PORTS                       (1024)

#define BUFFER_RX_LENGTH                    (32)
#define BUFFER_TX_LENGTH                    (32)
#define BUFFER_ELEM_MAX_SIZE_BYTES          (2048)

#define BUFFER_SIZE_BYTES                   (BUFFER_RX_LENGTH * BUFFER_ELEM_MAX_SIZE_BYTES)
#define BUFFERS_TOTAL_SIZE                  (BUFFER_SIZE_BYTES * (MAX_UDP_PORTS + 1))

#define BUFFER_RX_OFFSET_BYTES              (0)
#define BUFFER_RX_INDEX_OFFSET_BYTES(index) (BUFFER_RX_OFFSET_BYTES + (index * BUFFER_SIZE_BYTES)) 
#define BUFFER_TX_OFFSET_BYTES              (BUFFER_RX_OFFSET_BYTES + MAX_UDP_PORTS * BUFFER_SIZE_BYTES)

/**
 * The following are helper macros. They allows to get a byte pointer to packet
 * header data and payload data for each slot in a given RX buffer.
 */

#define BUFFER_RX_SLOT_DATA_OFFSET(rx_buf_idx, slot_idx) \
    BUFFER_RX_INDEX_OFFSET_BYTES(rx_buf_idx) + slot_idx * BUFFER_ELEM_MAX_SIZE_BYTES

#define BUFFER_RX_SLOT_HDR_DATA(rx_buf_idx, slot_idx, virt_dma_base) \
        ((u8*)virt_dma_base) + BUFFER_RX_SLOT_DATA_OFFSET(rx_buf_idx, slot_idx)

#define BUFFER_RX_SLOT_PAYLOAD_DATA(rx_buf_idx, slot_idx, virt_dma_base) \
    BUFFER_RX_SLOT_HDR_DATA(rx_buf_idx, slot_idx, virt_dma_base) + PACKET_HEADER_SIZE_BYTES

/* -------------------------------------------------------------------------- */

/**
 * The structure 'udp_core_raw_packet' represents the format of a UDP packet
 * as accepted by the FPGA core. It contains the bare minimum information needed
 * in order to copy the least amount of header information.
 * 
 * When a packet is received by the FPGA core, this driver reads populate a
 * lookalike structure and then do the minimum modifications to let upper kernel
 * layer accept it.
 */

struct udp_core_raw_packet 
{
    u64 payload_size_bytes;
    u64 source_ip;
    u64 source_port;
    u64 dest_ip;
    u64 dest_port;
    u64* payload;
};

#define PACKET_WORD_SIZE_BYTES              (8)
#define PACKET_HEADER_LENGTH                (5)
#define PACKET_HEADER_SIZE_BYTES            (40) // header len * word size bytes

#endif /* UDP_CORE_REGS_H */