/*
 * Userspace driver for UDP Ethernet Stack in FPGA
 *
 * Copyright (C) Acceleration Robotics S.L.U
 * Copyright (C) Accelerat S.r.l.
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>

#include "udriver.h"

/****************************************************************************
* Private struct: definitions & declarations
****************************************************************************/

struct RBTC_CTRL_BUFRX
{
    uint64_t popped        : 1;  // Bit 0
    uint64_t pushed        : 1;  // Bit 1
    uint64_t full          : 1;  // Bit 2
    uint64_t empty         : 1;  // Bit 3
    uint64_t tail          : 5;  // Bits 4-8 (5 bits)
    uint64_t head          : 5;  // Bits 9-13 (5 bits)
    uint64_t socket_state  : 1;  // Bit 14
    uint64_t dummy         : 1;  // Bit 15
    uint64_t reserved      : 49; // Bits 16-64 (reserved/unused)
};

struct udp_ip_device
{
    int32_t         irq_fd;
    int32_t         mem_fd;
    xrtDeviceHandle handle;
    xrtBufferHandle shmem_buff;
    size_t          shmem_size;
    uint64_t        shmem_phys_addr;
    void*           mapped_dev;
    uint64_t        page_offset;
    uint16_t        port_min;
    uint16_t        port_max;
};

static struct udp_ip_device dev;

/****************************************************************************
* Private functions: declarations
****************************************************************************/

static void read_reg(
    struct udp_ip_device* dev, 
    uint32_t register_offset, 
    uint32_t* value
);

static void write_reg(
    struct udp_ip_device* dev, 
    uint32_t register_offset, 
    uint32_t value
);

static void notify_pop_to_rx_buffer(
    struct udp_ip_device* dev, 
    uint32_t buffer_id
);

static void get_buffer_rx_param(
    struct udp_ip_device* dev, 
    uint32_t buffer_id, 
    struct RBTC_CTRL_BUFRX* reg
);

static void uint32_to_byte_arr(
    const uint32_t uint32_in, 
    uint8_t out_bytes[INET_ALEN]
);

static uint32_t byte_arr_to_uint32(const uint8_t array[INET_ALEN]);

static void eth_mac_to_eth_mac32(
    const uint8_t mac[ETH_ALEN], 
    uint32_t* mac32_h,
    uint32_t* mac32_l
);

/****************************************************************************
* Public functions: definitions
****************************************************************************/

int udriver_initialize(
    const uint8_t local_mac[ETH_ALEN],
    const uint8_t local_ip[INET_ALEN],
    const uint8_t subnet_mask[INET_ALEN],
    const uint8_t gw_ip[INET_ALEN], 
    uint16_t port_min, 
    uint16_t port_max
) 
{
    uint32_t mac32_l;
    uint32_t mac32_h;
    uint32_t buffer_rx_index;
    xrtBufferFlags flags;

    // ---------------------------------------------------------
    // Input data consistency check
    // ---------------------------------------------------------
    if (port_max - port_min >= MAX_UDP_PORTS)
    {
        printf("Port range is too wide - The max range is %d \n", MAX_UDP_PORTS);
        return -1;
    }

    dev.port_min = port_min;
    dev.port_max = port_max;

    // ---------------------------------------------------------
    // Open FPGA device
    // ---------------------------------------------------------
    dev.handle = xrtDeviceOpen(0);

    // ---------------------------------------------------------
    // Allocate memory (shared memory in DDR for ring buffers)
    // ---------------------------------------------------------

    #if CACHEABLE_MEM == 1
    flags = XRT_BO_FLAGS_CACHEABLE;
    #else
    flags = XRT_BO_FLAGS_NONE;
    #endif
    
    // Allocate shared memory buffer - in case of exception program fails
    dev.shmem_buff = xrtBOAlloc(dev.handle, BUF_TOTAL_SIZE, flags, 0); 
    dev.shmem_phys_addr = xrtBOAddress(dev.shmem_buff);
    dev.shmem_size = BUF_TOTAL_SIZE;

    // Note. 64 bit addressable memory is not supported by the IP
    if (dev.shmem_phys_addr > 0xFFFFFFFF)
    {
        printf("XRT allocated 64 bit addressable memory. Abort. \n");
        return -1;
    }

    // ---------------------------------------------------------
    // Mapping memory for udpip core configuration registers
    // ---------------------------------------------------------

    // Open /dev/mem to access physical memory
    dev.mem_fd = open(DEVMEM, O_RDWR | O_SYNC);
    
    if (dev.mem_fd == -1) 
    {
        printf("Cannot open /dev/mem. \n");
        return -1;
    }

    // Map the physical address of the pl control registers into the virtual address space
    dev.mapped_dev = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev.mem_fd, DEVICE_PAGE_BASE);
    
    if (dev.mapped_dev == MAP_FAILED) 
    {
        printf("Cannot map memory. \n");
        close(dev.mem_fd);
        return -1;
    }

    // ---------------------------------------------------------
    // Configure device registers
    // ---------------------------------------------------------
    
    // Assert reset
    write_reg(&dev, RBTC_CTRL_ADDR_RES_0_Y_O, 1); 
        
    // Set local MAC
    eth_mac_to_eth_mac32(local_mac, &mac32_h, &mac32_l);
    
    write_reg(&dev, RBTC_CTRL_ADDR_MAC_0_N_O, mac32_l);
    write_reg(&dev, RBTC_CTRL_ADDR_MAC_1_N_O, mac32_h);
    
    // Local gateway
    // NOTE: Must correspond to the other device's ID in a direct connection so that ARP is resolved
    write_reg(&dev, RBTC_CTRL_ADDR_GW_0_N_O, byte_arr_to_uint32(gw_ip));
    
    // Local subnet mask
    write_reg(&dev, RBTC_CTRL_ADDR_SNM_0_N_O, byte_arr_to_uint32(subnet_mask));
        
    // Local ip
    write_reg(&dev, RBTC_CTRL_ADDR_IP_LOC_0_N_O, byte_arr_to_uint32(local_ip));
    
    // Shared memory address
    write_reg(&dev, RBTC_CTRL_ADDR_SHMEM_0_N_O, (uint32_t)dev.shmem_phys_addr);
    
    // Listened ports range
    write_reg(&dev, RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O, port_min);
    write_reg(&dev, RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O, port_max);
    
    // Reset buffers - not pushed / not popped
    for (buffer_rx_index = 0; buffer_rx_index < MAX_UDP_PORTS; buffer_rx_index++)
        notify_pop_to_rx_buffer(&dev, buffer_rx_index);
    
    write_reg(&dev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);
    
    // Reset sockets - they are initially closed
    for (buffer_rx_index = 0; buffer_rx_index < MAX_UDP_PORTS; buffer_rx_index++)
        udriver_set_socket_status(buffer_rx_index, UDRIVER_SOCKET_CLOSED);

    // ---------------------------------------------------------
    // Open the kernel support for interrupt
    // ---------------------------------------------------------

    #if IRQ_SUPPORT == 1
    // Open /dev/mem to access physical memory
    dev.irq_fd = open(DEVIRQ, O_RDONLY);

    if (dev.irq_fd == -1) 
    {
        printf("Cannot open /dev/udp-core-irq. \n");
        return -1;
    }
    #else
    // Disable interrupts
    write_reg(&dev, RBTC_CTRL_ADDR_IER0, 0);
    write_reg(&dev, RBTC_CTRL_ADDR_GIE, 0);
    #endif

    // Deassert reset
    write_reg(&dev, RBTC_CTRL_ADDR_RES_0_Y_O, 0);

    return 0;
}

void udriver_destroy(void) 
{

    munmap(dev.mapped_dev, PAGE_SIZE);
    close(dev.mem_fd);
}

int udriver_set_socket_status(uint32_t port, uint32_t status) 
{
    uint32_t buffer_id;
    uint32_t value;

    if (port > dev.port_max || port < dev.port_min)
    {
        return -1;
    }

    buffer_id = port - dev.port_min;
    read_reg(&dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), &value);
    
    if (status == UDRIVER_SOCKET_OPEN) 
        value |= (1 << BUFFER_OPENSOCK_OFFSET);
    else
        value &= (~(1 << BUFFER_OPENSOCK_OFFSET));

    write_reg(&dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), value);

    return 0;
}

int udriver_send(struct udp_packet* udp_packet) 
{
    uint32_t tx_slot_full;
    uint32_t buftx_offset;
    uint32_t total_size;

    read_reg(&dev, RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I, &tx_slot_full);

    if (tx_slot_full)
        return -1;

    read_reg(&dev, RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I, &buftx_offset);
    buftx_offset = BUF_TX_OFFSET_BYTES + (buftx_offset * BUF_ELEM_MAX_SIZE_BYTES);

    // place packet in shared memory buffer
    total_size = PACKET_HDR_SIZE_BYTES + udp_packet->payload_size_bytes;
    xrtBOWrite(dev.shmem_buff, udp_packet, PACKET_HDR_SIZE_BYTES, buftx_offset);
    xrtBOWrite(dev.shmem_buff, udp_packet->payload, udp_packet->payload_size_bytes, buftx_offset+PACKET_HDR_SIZE_BYTES); 
    #if CACHEABLE_MEM == 1
    xrtBOSync (dev.shmem_buff, XCL_BO_SYNC_BO_FROM_DEVICE, total_size, buftx_offset);
    #endif

    // push to buffer tx
    write_reg(&dev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);
    write_reg(&dev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 1);
    write_reg(&dev, RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O, 0);

    return udp_packet->payload_size_bytes;
}

int udriver_recv(struct udp_packet* udp_packet, uint32_t port) 
{
    uint32_t buffer_id;
    uint32_t buf_base_addr;
    struct RBTC_CTRL_BUFRX reg;

    #if IRQ_SUPPORT == 1
    char irq_timestamp[MAX_TIMESTAMP_SIZE];
    int irq_arrived;

    // blocking read - wait until IRQ arrives
    irq_arrived = read(dev.irq_fd, irq_timestamp, MAX_TIMESTAMP_SIZE);

    if (irq_arrived <= 0) 
    {
        printf("Unable to wait for IRQ. Abort");
        return -1;
    }
    #endif

    buffer_id = port - dev.port_min;

    get_buffer_rx_param(&dev, buffer_id, &reg); 

    if (reg.empty)
        return 0;
    
    buf_base_addr = BUF_RX_IDX_OFFSET_BYTES(buffer_id) + reg.tail * BUF_ELEM_MAX_SIZE_BYTES;
    xrtBORead(dev.shmem_buff, udp_packet, PACKET_HDR_SIZE_BYTES, buf_base_addr);
    xrtBORead(dev.shmem_buff, udp_packet->payload, udp_packet->payload_size_bytes, buf_base_addr+PACKET_HDR_SIZE_BYTES);
    
    notify_pop_to_rx_buffer(&dev, buffer_id);

    return udp_packet->payload_size_bytes;
}

int udriver_probe_port(uint32_t port) 
{
    uint32_t buffer_id;
    struct RBTC_CTRL_BUFRX reg;

    buffer_id = port - dev.port_min;

    get_buffer_rx_param(&dev, buffer_id, &reg); 

    if (reg.empty)
        return 0;

    return 1;
}

void udriver_print_regs(uint32_t port)
{
    uint32_t buffer_id;
    uint32_t reg_i;
    uint32_t registers[RBTC_CTRL_REG_NUM];

    struct RBTC_CTRL_BUFRX reg;

    buffer_id = port - dev.port_min;

    for (reg_i = 0; reg_i < RBTC_CTRL_REG_NUM; reg_i++)
        read_reg(&dev, reg_i * RBTC_CTRL_REG_STRIDE, &registers[reg_i]);

    get_buffer_rx_param(&dev, buffer_id, &reg);

    printf("RBTC_CTRL_ADDR_AP_CTRL_0_N_P                : 0x%x\n", registers[0]);
    printf("RBTC_CTRL_ADDR_RES_0_Y_O                    : 0x%x\n", registers[1]);
    printf("RBTC_CTRL_ADDR_MAC_0_N_O                    : 0x%x\n", registers[2]);
    printf("RBTC_CTRL_ADDR_MAC_1_N_O                    : 0x%x\n", registers[3]);
    printf("RBTC_CTRL_ADDR_GW_0_N_O                     : 0x%x\n", registers[4]);
    printf("RBTC_CTRL_ADDR_SNM_0_N_O                    : 0x%x\n", registers[5]);
    printf("RBTC_CTRL_ADDR_IP_LOC_0_N_O                 : 0x%x\n", registers[6]);
    printf("RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O            : 0x%x\n", registers[7]);
    printf("RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O            : 0x%x\n", registers[8]);
    printf("RBTC_CTRL_ADDR_SHMEM_0_N_O                  : 0x%x\n", registers[9]);
    printf("RBTC_CTRL_ADDR_ISR0                         : 0x%x\n", registers[10]);
    printf("RBTC_CTRL_ADDR_IER0                         : 0x%x\n", registers[11]);
    printf("RBTC_CTRL_ADDR_GIE                          : 0x%x\n", registers[12]);
    printf("RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I             : 0x%x\n", registers[13]);
    printf("RBTC_CTRL_ADDR_BUFTX_TAIL_0_N_I             : 0x%x\n", registers[14]);
    printf("RBTC_CTRL_ADDR_BUFTX_EMPTY_0_N_I            : 0x%x\n", registers[15]);
    printf("RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I             : 0x%x\n", registers[16]);
    printf("RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O           : 0x%x\n", registers[17]);
    printf("RBTC_CTRL_ADDR_BUFTX_POPPED_0_N_I           : 0x%x\n", registers[18]);
    printf("RBTC_CTRL_ADDR_BUFRX_PUSH_IRQ_0_IRQ         : 0x%x\n", registers[19]);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_POPPED_OFFSET     : %u\n", reg.popped);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_PUSHED_OFFSET     : %u\n", reg.pushed);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_FULL_OFFSET       : %u\n", reg.full);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_EMPTY_OFFSET      : %u\n", reg.empty);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_TAIL_OFFSET       : %u\n", reg.tail);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_HEAD_OFFSET       : %u\n", reg.head);
    printf("RBTC_CTRL_BUFRX0 - BUFFER_OPENSOCK_OFFSET   : %u\n", reg.socket_state);

    printf("\n");
}

void udriver_print_packet(struct udp_packet* packet) 
{
    uint8_t src_ip[INET_ALEN];
    uint8_t dest_ip[INET_ALEN];

    uint32_to_byte_arr(packet->source_ip, src_ip);
    uint32_to_byte_arr(packet->dest_ip, dest_ip);

    printf("Header / payload size:    %ld\n", packet->payload_size_bytes);
    printf("Header / source ip:       %hhu.%hhu.%hhu.%hhu\n", src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
    printf("Header / source port:     %ld\n", packet->source_port);
    printf("Header / dest ip:         %hhu.%hhu.%hhu.%hhu\n", dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3]);
    printf("Header / dest port:       %ld\n", packet->dest_port);
    printf("Header / payload:         %.*s\n\n", (int32_t)packet->payload_size_bytes, (char*)packet->payload);
}

uint32_t udriver_get_local_ip()
{
    uint32_t local_ip;
    read_reg(&dev, RBTC_CTRL_ADDR_IP_LOC_0_N_O, &local_ip);
    return local_ip;
}

uint16_t udriver_get_port_range_low()
{
    uint32_t port_low;
    read_reg(&dev, RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O, &port_low);
    return (uint16_t)port_low;
}

uint16_t udriver_get_port_range_high()
{
    uint32_t port_high;
    read_reg(&dev, RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O, &port_high);
    return (uint16_t)port_high;
}

/****************************************************************************
* Private functions: definitions
****************************************************************************/

#define REG_GET_OFFSET(dev, offset) \
    (uint32_t*)((uint8_t*)(dev->mapped_dev) + dev->page_offset + offset)

static void read_reg(
    struct udp_ip_device* dev, 
    uint32_t register_offset, 
    uint32_t* value
) 
{
    volatile uint32_t* reg_addr = 
        REG_GET_OFFSET(dev, register_offset);
    
    *value = *reg_addr;
}

static void write_reg(
    struct udp_ip_device* dev, 
    uint32_t register_offset, 
    uint32_t value
) 
{
    volatile uint32_t* reg_addr = 
        REG_GET_OFFSET(dev, register_offset);

    *reg_addr = value;
}

static void notify_pop_to_rx_buffer(
    struct udp_ip_device* dev, 
    uint32_t buffer_id
) 
{
    uint32_t value;
    uint32_t mask_clear;
    uint32_t mask_set;

    mask_clear = ~(1 << BUFFER_POPPED_OFFSET);
    mask_set = 1 << BUFFER_POPPED_OFFSET;

    // get current value
    read_reg(dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), &value);
    
    // clear pop
    write_reg(dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), value & mask_clear);
    
    // set pop
    write_reg(dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), value | mask_set);
    
    // clear pop
    write_reg(dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), value & mask_clear);
}

static void get_buffer_rx_param(
    struct udp_ip_device* dev, 
    uint32_t buffer_id, 
    struct RBTC_CTRL_BUFRX* reg
) 
{
    read_reg(dev, BUFFER_RX_CTRL_BASE_OFFSET(buffer_id), (uint32_t*) reg);
}

/**
 * Combines the byte of a network ordered uint32 into a byte array.
 * Used to represent network ordered IP addresses into 4 byte array.
 * 
 * Example:
 *  -> in : uint32      = 0xC0A80102
 *  -> out: byte array  = [0xC0, 0xA8, 0x01, 0x64]
 *  
 */
static void uint32_to_byte_arr(const uint32_t uint32_in, uint8_t out_bytes[INET_ALEN]) 
{
    out_bytes[0] = (uint32_in >> 8 * 3) & 0xFF;
    out_bytes[1] = (uint32_in >> 8 * 2) & 0xFF;
    out_bytes[2] = (uint32_in >> 8 * 1) & 0xFF;
    out_bytes[3] = (uint32_in >> 8 * 0) & 0xFF;
}

/**
 * Combines the bytes of a byte array into a network order uint32.
 * Used to represent IP or MAC addresses in 4 byte network order.
 * 
 * Example:
 *  Configuration of IP address into the 32-bit device register. Suppose to
 *  configure 192.168.1.100 into the register.
 *  -> in : byte array  = [0xC0, 0xA8, 0x01, 0x64]
 *  -> out: uint32      = 0xC0A80102
 * 
 */
static uint32_t byte_arr_to_uint32(const uint8_t array[INET_ALEN]) 
{
    uint32_t result = 0;

    result |= (uint32_t)((uint8_t)(array[0])) << 8 * 3;
    result |= (uint32_t)((uint8_t)(array[1])) << 8 * 2;
    result |= (uint32_t)((uint8_t)(array[2])) << 8 * 1;
    result |= (uint32_t)((uint8_t)(array[3])) << 8 * 0;

    return result;
}

/**
 * Converts a Ethernet MAC address (represented as array of bytes) to make it
 * understandable by the device.
 * 
 * The device has 32-bits registers. Therefore the 6-octet Ethernet MAC address
 * must be represented in two 32-bits integers. The higher part is padded with 
 * zeros.
 * 
 * Leaves the results in mac32_h, mac32_l (network order).
 */
static void eth_mac_to_eth_mac32(
    const uint8_t mac[ETH_ALEN], 
    uint32_t* mac32_h,
    uint32_t* mac32_l
)
{
    uint8_t mac_high[4] = {0, 0, mac[0], mac[1]};
    uint8_t mac_low[4] = {mac[2], mac[3], mac[4], mac[5]};

    *mac32_h = byte_arr_to_uint32(mac_high);
    *mac32_l = byte_arr_to_uint32(mac_low);
}