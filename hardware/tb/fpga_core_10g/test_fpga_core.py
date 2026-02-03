"""

Copyright (c) 2020 Alex Forencich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

"""

###################################################################################
# Imports
###################################################################################

import logging
import os

from scapy.layers.l2 import Ether, ARP
from scapy.layers.inet import IP, UDP

import cocotb_test.simulator

import cocotb
from cocotb.log import SimLog
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge

from cocotbext.eth import XgmiiFrame, XgmiiSource, XgmiiSink
from cocotbext.axi import AxiBus, AxiRam, AxiLiteMaster, AxiLiteBus

import struct

###################################################################################
# TB class (common for all tests)
###################################################################################

class TB:

    axil_ctrl_addresses_dic = {
        "ADDR_AP_CTRL_0_N_P"        : 0x00000000,
        "ADDR_RES_0_Y_O"            : 0x00000008,
        "ADDR_MAC_0_N_O"            : 0x00000010,
        "ADDR_MAC_1_N_O"            : 0x00000018,
        "ADDR_GW_0_N_O"             : 0x00000020,
        "ADDR_SNM_0_N_O"            : 0x00000028,
        "ADDR_IP_LOC_0_N_O"         : 0x00000030,
        "ADDR_UDP_RANGE_L_0_N_O"    : 0x00000038,
        "ADDR_UDP_RANGE_H_0_N_O"    : 0x00000040,
        "ADDR_SHMEM_0_N_O"          : 0x00000048,
        "ADDR_ISR0"                 : 0x00000050,
        "ADDR_IER0"                 : 0x00000058,
        "ADDR_GIE"                  : 0x00000060,
        "ADDR_BUFTX_OFFSET_0_N_I"   : 0x00000068,
        "ADDR_BUFTX_HEAD_0_N_I"     : 0x00000068,
        "ADDR_BUFTX_TAIL_0_N_I"     : 0x00000070,
        "ADDR_BUFTX_EMPTY_0_N_I"    : 0x00000078,
        "ADDR_BUFTX_FULL_0_N_I"     : 0x00000080,
        "ADDR_BUFTX_PUSHED_0_Y_O"   : 0x00000088,
        "ADDR_BUFTX_POPPED_0_N_I"   : 0x00000090,
        "ADDR_BUFRX_PUSH_IRQ_0_IRQ" : 0x00000098,
        "ADDR_BUFRX_OFFSET_0_N_I"   : 0x000000a0,
    }

    C_BUFFRX_INDEX_WIDTH   = 5
    C_S_AXI_DATA_WIDTH     = 32
    BUFFER_POPPED_OFFSET   = 0
    BUFFER_PUSHED_OFFSET   = 1
    BUFFER_FULL_OFFSET     = 2
    BUFFER_EMPTY_OFFSET    = 3
    BUFFER_TAIL_OFFSET     = 4
    BUFFER_TAIL_UPPER      = BUFFER_TAIL_OFFSET + C_BUFFRX_INDEX_WIDTH - 1
    BUFFER_HEAD_OFFSET     = BUFFER_TAIL_UPPER + 1
    BUFFER_HEAD_UPPER      = BUFFER_HEAD_OFFSET + C_BUFFRX_INDEX_WIDTH - 1
    BUFFER_OPENSOCK_OFFSET = BUFFER_HEAD_UPPER + 1
    BUFFER_DUMMY_OFFSET    = BUFFER_OPENSOCK_OFFSET + 1
    BUFFER_DUMMY_UPPER     = C_S_AXI_DATA_WIDTH - 1

    def __init__(self, dut):
        self.dut = dut

        self.log = SimLog("cocotb.tb")
        self.log.setLevel(logging.DEBUG)

        cocotb.start_soon(Clock(dut.clk, 6.4, units="ns").start())

        # Ethernet
        cocotb.start_soon(Clock(dut.sfp0_rx_clk, 6.4, units="ns").start())
        self.sfp0_source = XgmiiSource(dut.sfp0_rxd, dut.sfp0_rxc, dut.sfp0_rx_clk, dut.sfp0_rx_rst)
        cocotb.start_soon(Clock(dut.sfp0_tx_clk, 6.4, units="ns").start())
        self.sfp0_sink = XgmiiSink(dut.sfp0_txd, dut.sfp0_txc, dut.sfp0_tx_clk, dut.sfp0_tx_rst)

        # AXI master interface
        self.NUM_BUFFERS_RX = self.dut.controller_inst.MAX_UDP_PORTS.value
        self.BUFFER_RX_LENGTH = self.dut.controller_inst.BUFFER_RX_LENGTH.value
        self.BUFFER_ELEM_MAX_SIZE = self.dut.controller_inst.BUFFER_ELEM_MAX_SIZE.value
        self.BUFFER_SIZE = self.BUFFER_RX_LENGTH*self.BUFFER_ELEM_MAX_SIZE
        shmem_size = (self.NUM_BUFFERS_RX + 1) * self.BUFFER_SIZE
        self.axi_ram = AxiRam(AxiBus.from_prefix(dut, "m_axi"), dut.clk, dut.rst, size=shmem_size)

        # AXI slave interface (control)
        self.s_axil_ctrl  = AxiLiteMaster(AxiLiteBus.from_prefix(dut, "s_axil"), dut.clk, dut.rst)

    async def init(self):

        self.dut.rst.setimmediatevalue(0)
        self.dut.sfp0_rx_rst.setimmediatevalue(0)
        self.dut.sfp0_tx_rst.setimmediatevalue(0)

        for k in range(10):
            await RisingEdge(self.dut.clk)

        self.dut.rst.value = 1
        self.dut.sfp0_rx_rst.value = 1
        self.dut.sfp0_tx_rst.value = 1

        for k in range(10):
            await RisingEdge(self.dut.clk)

        self.dut.rst.value = 0
        self.dut.sfp0_rx_rst.value = 0
        self.dut.sfp0_tx_rst.value = 0

    async def config(self, dut_eth, dut_ip):
        
        # Configure udpcomplete parameters
        mac_bytes = mac_str_to_bytes(dut_eth)
        mac_bytes_L = bytes([mac_bytes[5], mac_bytes[4], mac_bytes[3], mac_bytes[2]])
        mac_bytes_H = bytes([mac_bytes[1], mac_bytes[0], 0x00, 0x00])
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_MAC_0_N_O"], mac_bytes_L)
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_MAC_1_N_O"], mac_bytes_H)
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_GW_0_N_O"], ip_str_to_ip_bytes("192.168.2.1"))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_SNM_0_N_O"], ip_str_to_ip_bytes("255.255.255.0"))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_IP_LOC_0_N_O"], ip_str_to_ip_bytes(dut_ip))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_SHMEM_0_N_O"], (0x00000000).to_bytes(1, 'big'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_UDP_RANGE_L_0_N_O"], (5677).to_bytes(2, 'little'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_UDP_RANGE_H_0_N_O"], (5679).to_bytes(2, 'little'))
        for buffer_idx in range(self.NUM_BUFFERS_RX): await self.set_buffer_rx_popped(buffer_idx, 0)
        for buffer_idx in range(self.NUM_BUFFERS_RX): await self.set_buffer_rx_opensocket(buffer_idx, 1)
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_PUSHED_0_Y_O"], (0).to_bytes(1, 'big'))
        
        # Enable interrupts
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_IER0"], (1).to_bytes(1, 'big'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_GIE"], (1).to_bytes(1, 'big'))
        # Assert reset so that DUT udpates parameters with the previous values
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (0).to_bytes(1, 'big'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (1).to_bytes(1, 'big'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (0).to_bytes(1, 'big'))
        await RisingEdge(self.dut.clk)

    # Buffers (DDR)
    def get_buffer_rx_addr_ddr(self, buffer_id):
        return self.dut.controller_inst.shared_mem_base_address.value + buffer_id * self.BUFFER_SIZE
    def get_buffer_tx_addr_ddr(self):
        return self.dut.controller_inst.shared_mem_base_address.value + self.NUM_BUFFERS_RX * self.BUFFER_SIZE
    
    # Buffers (control axil)

    def get_buffer_rx_addr_control(self, buffer_id):
        return TB.axil_ctrl_addresses_dic["ADDR_BUFRX_OFFSET_0_N_I"] + 8 * buffer_id
    
    def get_buffer_tx_addr_control(self):
        return TB.axil_ctrl_addresses_dic["ADDR_BUFTX_OFFSET_0_N_I"] + 8 * self.NUM_BUFFERS_RX
    
    async def get_buffer_rx_param(self, buffer_id, param_offset):
        buff_addr = self.get_buffer_rx_addr_control(buffer_id)
        buff_ctrl_data = int.from_bytes(await self.s_axil_ctrl.read(buff_addr, 4), 'little')
        # Unmask value
        start_position = param_offset
        if (param_offset == TB.BUFFER_TAIL_OFFSET or param_offset == TB.BUFFER_HEAD_OFFSET):
            num_bits = 5
        else:
            num_bits = 1
        mask = (1 << num_bits) - 1
        data_extracted = (buff_ctrl_data >> start_position ) & mask

        # Return value
        return data_extracted
    
    async def deassert_interrupt(self):
        # Write 0 onto ISR
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_ISR0"], (0).to_bytes(1, 'big'))
    
    async def set_buffer_rx_popped(self, buffer_id, value):
        # Read current content
        buff_addr = self.get_buffer_rx_addr_control(buffer_id)
        original_value = int.from_bytes(await self.s_axil_ctrl.read(buff_addr, 4), 'little')
        # Replace our value in original content
        new_value = TB.replace_bits(original_value, value, TB.BUFFER_POPPED_OFFSET, 1)
        await self.s_axil_ctrl.write(buff_addr, struct.pack('<I', new_value)) # Little endian

    async def set_buffer_rx_opensocket(self, buffer_id, value):
        # Read current content
        buff_addr = self.get_buffer_rx_addr_control(buffer_id)
        original_value = int.from_bytes(await self.s_axil_ctrl.read(buff_addr, 4), 'little')
        # Replace our value in original content
        new_value = TB.replace_bits(original_value, value, TB.BUFFER_OPENSOCK_OFFSET, 1)
        await self.s_axil_ctrl.write(buff_addr, struct.pack('<I', new_value)) # Little endian

    def replace_bits(number, my_value, pos, n):
        mask = ~(2**n - 1 << pos) # Create a mask to clear the bits at the specified position
        cleared_number = number & mask # Clear the bits at the specified position
        shifted_value = (my_value & (2**n - 1)) << pos # Shift and mask the new value to match the bit position
        result = cleared_number | shifted_value # Combine the cleared number and the new value
        return result

    async def recv_arp_from_dut(self, ext_eth, ext_ip, dut_eth, dut_ip):
        # Wait for ARP reply packet at sfp tx
        self.log.info("receive ARP request from DUT")
        rx_frame = await self.sfp0_sink.recv()
        rx_pkt = Ether(bytes(rx_frame.get_payload()))

        self.log.info("RX packet: %s", repr(rx_pkt))
        self.log.info(rx_pkt.payload)

        assert rx_pkt.dst == ext_eth
        assert rx_pkt.src == dut_eth
        assert rx_pkt[ARP].hwtype == 1
        assert rx_pkt[ARP].ptype == 0x0800
        assert rx_pkt[ARP].hwlen == 6
        assert rx_pkt[ARP].plen == 4
        assert rx_pkt[ARP].op == 2
        assert rx_pkt[ARP].hwsrc == dut_eth
        assert rx_pkt[ARP].psrc == dut_ip
        assert rx_pkt[ARP].hwdst == ext_eth
        assert rx_pkt[ARP].pdst == ext_ip

    async def send_arp_to_dut(self, ext_eth, ext_ip, dut_ip):
        hwbroadcast = 'ff:ff:ff:ff:ff:ff'
        hwtarget = '00:00:00:00:00:00'

        # Send ARP request to the DUT
        arp = ARP(
            hwtype=1,
            ptype=0x0800,
            hwlen=6,
            plen=4, 
            op=1,
            hwsrc=ext_eth, 
            psrc=ext_ip,
            hwdst=hwtarget, 
            pdst=dut_ip)
        eth = Ether(src=ext_eth, dst=hwbroadcast)
        pkt = eth / arp
        frame = XgmiiFrame.from_payload(pkt.build())
        await self.sfp0_source.send(frame)

    async def send_packet_to_dut(self, packet_cfg):

        self.log.info("Generating UDP packet...")
        eth = Ether(src=packet_cfg.src_eth, dst=packet_cfg.dst_eth)
        ip = IP(src=packet_cfg.src_ip, dst=packet_cfg.dst_ip)
        udp = UDP(sport=packet_cfg.src_udp, dport=packet_cfg.dst_udp)
        test_pkt = eth / ip / udp / packet_cfg.payload
        test_frame = XgmiiFrame.from_payload(test_pkt.build())
        # Send packet to DUT
        await self.sfp0_source.send(test_frame)

    async def check_buffer_rx_slot(self, packet_cfg, buffer_rx_id, buffer_slot):

        # Read buffer
        packet_addr = self.get_buffer_rx_addr_ddr(0) + buffer_rx_id*self.BUFFER_SIZE + buffer_slot*self.BUFFER_ELEM_MAX_SIZE
        read_bytes = self.axi_ram.read(packet_addr, packet_cfg.payload_size+5*8) # The header takes 5 8-byte words
        # self.log.info("Dumping axi ram content...\n" + self.axi_ram.hexdump_str(packet_addr, MAX_PACKET_SIZE*BUFFER_RX_LENGTH, prefix="RAM"))

        # Assert content (if the payload is longer than the limit, we expect the payload to be different to the data read from memory)
        expected_value = packet_cfg.payload_size.to_bytes(8, byteorder='little') + ip_str_to_ip_bytes(packet_cfg.src_ip) + packet_cfg.src_udp.to_bytes(8, byteorder='little') + ip_str_to_ip_bytes(packet_cfg.dst_ip) + packet_cfg.dst_udp.to_bytes(8, byteorder='little') + packet_cfg.payload
        if packet_cfg.payload_size+5*8 < self.BUFFER_ELEM_MAX_SIZE:
            assert(read_bytes == expected_value)
        else:
            assert(read_bytes != expected_value)

    async def check_int_status(self, expected_value):
        # Read interrupt line and compare to expected_value
        assert (self.dut.buffer_rx_pushed_interr_o == expected_value)

    async def check_buffer_rx(self, packet_cfg, buffer_rx_id, check_int_status=True):

        circbuff_rx_empty  = await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_EMPTY_OFFSET)
        while circbuff_rx_empty:
            circbuff_rx_empty  = await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_EMPTY_OFFSET)

        if check_int_status:
            await self.check_int_status(1)

        circbuff_rx_tail_index  = await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_TAIL_OFFSET)

        await self.print_buffer_rx_status(buffer_rx_id)
        await self.check_buffer_rx_slot(packet_cfg, buffer_rx_id, circbuff_rx_tail_index)
        await self.deassert_interrupt()
        await self.set_buffer_rx_popped(buffer_rx_id, 0)
        await self.set_buffer_rx_popped(buffer_rx_id, 1)
        await self.set_buffer_rx_popped(buffer_rx_id, 0)
        await self.print_buffer_rx_status(buffer_rx_id)

    async def print_buffer_rx_status(self, buffer_rx_id):
        circbuff_rx_head_index = str(await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_HEAD_OFFSET))
        circbuff_rx_tail_index = str(await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_TAIL_OFFSET))
        circbuff_rx_full       = str(await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_FULL_OFFSET))
        circbuff_rx_empty      = str(await self.get_buffer_rx_param(buffer_rx_id, TB.BUFFER_EMPTY_OFFSET))
        self.log.info("Buffer rx status: head=" + circbuff_rx_head_index + ", tail=" + circbuff_rx_tail_index + ", full=" + circbuff_rx_full + ", empty=" + circbuff_rx_empty)

    async def place_packet_at_mem(self, packet_cfg):

        # Build packet to be placed at DUT memory (DDR)        
        ddr_packet = packet_cfg.payload_size.to_bytes(8, byteorder='little')
        ddr_packet += ip_str_to_ip_bytes(packet_cfg.src_ip)
        ddr_packet += packet_cfg.src_udp.to_bytes(8, byteorder='little') 
        ddr_packet += ip_str_to_ip_bytes(packet_cfg.dst_ip)
        ddr_packet += packet_cfg.dst_udp.to_bytes(8, byteorder='little')
        ddr_packet += packet_cfg.payload

        # Gather tx buffer info to know where to put the packet
        circbuff_tx_head_index = int.from_bytes(await self.s_axil_ctrl.read(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_HEAD_0_N_I"], 4), 'little' )
        circbuff_tx_tail_index = int.from_bytes(await self.s_axil_ctrl.read(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_TAIL_0_N_I"], 4), 'little' )
        circbuff_tx_full       = int.from_bytes(await self.s_axil_ctrl.read(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_FULL_0_N_I"], 4), 'little' )
        circbuff_tx_empty      = int.from_bytes(await self.s_axil_ctrl.read(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_EMPTY_0_N_I"], 4), 'little')
        circbuff_tx_base_addr  = int(self.dut.controller_inst.circbuff_tx_base_addr  )
        MAX_PACKET_SIZE        = int(self.dut.controller_inst.BUFFER_ELEM_MAX_SIZE)

        circbuff_tx_next_pack_addr = circbuff_tx_base_addr + circbuff_tx_head_index*MAX_PACKET_SIZE
        print(circbuff_tx_head_index)
        print(circbuff_tx_full)
        if (not circbuff_tx_full):
            self.axi_ram.write(circbuff_tx_next_pack_addr, ddr_packet)
            self.log.info(self.axi_ram.hexdump_str(circbuff_tx_next_pack_addr, 256, prefix="RAM"))
            # Notify tx circular buffer about a new data pushed
            await self.notify_pl_buffer_tx_push()

    async def notify_pl_buffer_tx_push(self):
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_PUSHED_0_Y_O"], (0).to_bytes(1, 'big'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_PUSHED_0_Y_O"], (1).to_bytes(1, 'big'))
        await self.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_BUFTX_PUSHED_0_Y_O"], (0).to_bytes(1, 'big'))
    
    async def check_tx_packet_at_sfp(self, packet_cfg):

        # Wait for packet at sfp tx. If it requires ARP reply, reply
        rx_frame = await self.sfp0_sink.recv()
        rx_pkt = Ether(bytes(rx_frame.get_payload()))
        if rx_pkt.dst == 'ff:ff:ff:ff:ff:ff':
            await self.reply_arp(packet_cfg, rx_frame)
            # Wait for actual payload
            rx_frame = await self.sfp0_sink.recv()
            rx_pkt = Ether(bytes(rx_frame.get_payload()))

        # Monitor sfp tx until detecting traffic (UDP from the DUT)
        self.log.info("receive UDP packet from DUT")
        self.log.info("RX packet: %s", repr(rx_pkt))

        # Assert packet content
        assert rx_pkt.dst == packet_cfg.dst_eth
        assert rx_pkt.src == packet_cfg.src_eth
        assert rx_pkt[IP].dst == packet_cfg.dst_ip
        assert rx_pkt[IP].src == packet_cfg.src_ip
        assert rx_pkt[UDP].dport == packet_cfg.dst_udp
        assert rx_pkt[UDP].sport == packet_cfg.src_udp

    async def reply_arp(self, packet_cfg, rx_frame):

        # Monitor sfp tx until detecting traffic (ARP request from the DUT)
        self.log.info("receive ARP request from DUT")
        rx_pkt = Ether(bytes(rx_frame.get_payload()))
        self.log.info("RX packet: %s", repr(rx_pkt))
        self.log.info(rx_pkt.payload)

        assert rx_pkt.dst == 'ff:ff:ff:ff:ff:ff'
        assert rx_pkt.src == packet_cfg.src_eth
        assert rx_pkt[ARP].hwtype == 1
        assert rx_pkt[ARP].ptype == 0x0800
        assert rx_pkt[ARP].hwlen == 6
        assert rx_pkt[ARP].plen == 4
        assert rx_pkt[ARP].op == 1
        assert rx_pkt[ARP].hwsrc == packet_cfg.src_eth
        assert rx_pkt[ARP].psrc == packet_cfg.src_ip
        assert rx_pkt[ARP].hwdst == '00:00:00:00:00:00'
        assert rx_pkt[ARP].pdst == packet_cfg.dst_ip

        # ARP response to the DUT
        self.log.info("send ARP response to DUT")
        arp = ARP(hwtype=1, ptype=0x0800, hwlen=6, plen=4, op=2,
            hwsrc=packet_cfg.dst_eth, psrc=packet_cfg.dst_ip,
            hwdst=packet_cfg.src_eth, pdst=packet_cfg.src_ip)
        eth = Ether(src=packet_cfg.src_eth, dst=packet_cfg.dst_eth)
        resp_pkt = eth / arp
        resp_frame = XgmiiFrame.from_payload(resp_pkt.build())
        await self.sfp0_source.send(resp_frame)

def ip_str_to_ip_bytes(ip_str="0.0.0.0"):
    ip_parts = ip_str.split(".")
    ip_bytes_arr = bytearray()
    for part in ip_parts:
        byte = int(part).to_bytes(1, byteorder='little')
        ip_bytes_arr.extend(byte)

    ip_bytes_8B = bytes(ip_bytes_arr[::-1].ljust(8, b'\x00'))
    return ip_bytes_8B

def mac_str_to_bytes(mac_str="ff:ff:ff:ff:ff:ff"):
    components = mac_str.split(':')
    int_values = [int(component, 16) for component in components]
    bytes_variable = bytes(int_values)
    return bytes_variable

class Packet_cfg:
    def __init__(self, payload_size, src_eth, src_ip, src_udp, dst_eth, dst_ip, dst_udp):
        self.payload_size = payload_size
        self.payload = bytes([x % 256 for x in range(payload_size)])
        self.src_eth = src_eth
        self.src_ip = src_ip
        self.src_udp = src_udp
        self.dst_eth = dst_eth
        self.dst_ip = dst_ip
        self.dst_udp = dst_udp

###################################################################################
# Test: sfprx_to_shmem 
# Stimulus: SFP packet generated and sent to DUT SFP RX port
# Expected: packet payload available at DUT m_axi 
###################################################################################

@cocotb.test()
async def run_test_arp(dut):

    # Initialize TB
    tb = TB(dut)
    await tb.init()

    # General test parameters
    dut_eth = '02:00:00:00:00:00'
    dut_ip = '192.168.2.128'
    dut_udp = 1234
    ext_eth = '5a:51:52:53:54:55'
    ext_ip = '192.168.2.100'
    ext_udp = 1234
    await tb.config(dut_eth, dut_ip)

    # Send ARP
    await tb.send_arp_to_dut(ext_eth, ext_ip, dut_ip)
    await tb.recv_arp_from_dut(ext_eth, ext_ip, dut_eth, dut_ip)

    # Send simple UDP
    payload_size = 10
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)

    # Wait a few nsec
    for _ in range(5): await RisingEdge(dut.clk)

    # Send ARP
    await tb.send_arp_to_dut(ext_eth, ext_ip, dut_ip)
    await tb.recv_arp_from_dut(ext_eth, ext_ip, dut_eth, dut_ip)

    # Send simple UDP
    payload_size = 10
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    #await tb.check_buffer_rx(packet_cfg, 1)

    # Wait a few nsec
    for _ in range(5): await RisingEdge(dut.clk)

    # Send ARP
    await tb.send_arp_to_dut(ext_eth, ext_ip, dut_ip)
    await tb.recv_arp_from_dut(ext_eth, ext_ip, dut_eth, dut_ip)

    # Send simple UDP
    payload_size = 10
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    #await tb.check_buffer_rx(packet_cfg, 1)

    # Wait a few nsec
    for _ in range(5): await RisingEdge(dut.clk)

    # Send ARP
    await tb.send_arp_to_dut(ext_eth, ext_ip, dut_ip)
    await tb.recv_arp_from_dut(ext_eth, ext_ip, dut_eth, dut_ip)

    # Send simple UDP
    payload_size = 10
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, 5678)
    await tb.send_packet_to_dut(packet_cfg)
    await tb.check_buffer_rx(packet_cfg, 1)

    # Wait a few nsec
    for _ in range(5): await RisingEdge(dut.clk)

@cocotb.test()
async def run_test_udp_rx(dut):

    # Initialize TB
    tb = TB(dut)
    await tb.init()

    # General test parameters
    dut_eth = '02:00:00:00:00:00'
    dut_ip = '192.168.2.128'
    dut_udp = 5678
    ext_eth = '5a:51:52:53:54:55'
    ext_ip = '192.168.2.100'
    ext_udp = 1234
    await tb.config(dut_eth, dut_ip)

    # Send 1 10B packet 
    payload_size = 10
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    await tb.check_buffer_rx(packet_cfg, 1)

    # Send 1 256B packet 
    payload_size = 256
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    await tb.check_buffer_rx(packet_cfg, 1)

    # Send 1 1024B packet 
    payload_size = 1024
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    await tb.check_buffer_rx(packet_cfg, 1)

    # Send 1 2048B packet 
    payload_size = 2048
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    await tb.check_buffer_rx(packet_cfg, 1)

    # Send 32 256B packets
    payload_size = 256
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    for _ in range(32):
        await tb.send_packet_to_dut(packet_cfg)
    for _ in range(32):
        await tb.check_buffer_rx(packet_cfg, 1, False)    

    # Leave some extra time to make visual simulation look better
    for _ in range(100): await RisingEdge(dut.clk)

###################################################################################
# Test: shmem_to_sfprx
# Stimulus: UDP packet payload placed at shared memory 
# Expected: packet payload available at DUT sfp tx 
###################################################################################

@cocotb.test()
async def run_test_udp_tx(dut):

    # Initialize TB
    tb = TB(dut)
    await tb.init()

    # General test parameters
    dut_eth = '02:00:00:00:00:00'
    dut_ip = '192.168.2.128'
    dut_udp = 5678
    ext_eth = '5a:51:52:53:54:55'
    ext_ip = '192.168.2.100'
    ext_udp = 1234
    await tb.config(dut_eth, dut_ip)

    # Send 1 10B packet 
    payload_size = 10
    packet_cfg = Packet_cfg(payload_size, dut_eth, dut_ip, dut_udp, ext_eth, ext_ip, ext_udp)
    await tb.place_packet_at_mem(packet_cfg)
    await tb.check_tx_packet_at_sfp(packet_cfg)

    # Send 1 256B packet 
    payload_size = 256
    packet_cfg = Packet_cfg(payload_size, dut_eth, dut_ip, dut_udp, ext_eth, ext_ip, ext_udp)
    await tb.place_packet_at_mem(packet_cfg)
    await tb.check_tx_packet_at_sfp(packet_cfg)

    # Send 1 1024B packet 
    payload_size = 1024
    packet_cfg = Packet_cfg(payload_size, dut_eth, dut_ip, dut_udp, ext_eth, ext_ip, ext_udp)
    await tb.place_packet_at_mem(packet_cfg)
    await tb.check_tx_packet_at_sfp(packet_cfg)

    # Send 32 256B packets
    payload_size = 256
    packet_cfg = Packet_cfg(payload_size, dut_eth, dut_ip, dut_udp, ext_eth, ext_ip, ext_udp)
    for _ in range(32):
        await tb.place_packet_at_mem(packet_cfg)
    for _ in range(32):
        await tb.check_tx_packet_at_sfp(packet_cfg)

    # Leave some extra time to make visual simulation look better
    for _ in range(100): await RisingEdge(dut.clk)

###################################################################################
# Test: run_test_user_reset
# Stimulus: UDP packet payload placed at shared memory 
# Expected: packet payload available at DUT sfp tx 
###################################################################################

@cocotb.test()
async def run_test_user_reset_rx(dut):

    # Initialize TB
    tb = TB(dut)
    await tb.init()

    # General test parameters
    dut_eth = '02:00:00:00:00:00'
    dut_ip = '192.168.2.128'
    dut_udp = 5678
    ext_eth = '5a:51:52:53:54:55'
    ext_ip = '192.168.2.100'
    ext_udp = 1234
    await tb.config(dut_eth, dut_ip)

    # Send 1 100B packet (rx)

    payload_size = 100
    packet_cfg = Packet_cfg(payload_size, ext_eth, ext_ip, ext_udp, dut_eth, dut_ip, dut_udp)
    await tb.send_packet_to_dut(packet_cfg)
    # Change udpcomplete local config and apply reset while the udp ip is working (the reset shouldn't have effect until finishing the operation)
    while (not tb.dut.controller_inst.rx_payload_axis_tvalid.value): await RisingEdge(tb.dut.clk)
    new_submask = "255.255.0.0"
    await tb.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_SNM_0_N_O"], ip_str_to_ip_bytes(new_submask))
    # subnet_mask should not be updated with new value until transaction has finished
    await tb.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (1).to_bytes(1, 'big'))
    await tb.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (0).to_bytes(1, 'big'))

    assert tb.dut.controller_inst.subnet_mask.value != tb.dut.controller_inst.subnet_mask_from_ps.value
    # Wait for transaction to finish
    await tb.check_buffer_rx(packet_cfg, 1)
    # Now subnet_mask should have been updated
    assert tb.dut.controller_inst.subnet_mask.value == tb.dut.controller_inst.subnet_mask_from_ps.value
    # Leave some extra time to make visual simulation look better
    for _ in range(100): await RisingEdge(dut.clk)

@cocotb.test()
async def run_test_user_reset_tx(dut):

    # Initialize TB
    tb = TB(dut)
    await tb.init()

    # General test parameters
    dut_eth = '02:00:00:00:00:00'
    dut_ip = '192.168.2.128'
    dut_udp = 5678
    ext_eth = '5a:51:52:53:54:55'
    ext_ip = '192.168.2.100'
    ext_udp = 1234
    await tb.config(dut_eth, dut_ip)

    # Send 1 100B packet (tx)

    payload_size = 100
    packet_cfg = Packet_cfg(payload_size, dut_eth, dut_ip, dut_udp, ext_eth, ext_ip, ext_udp)
    await tb.place_packet_at_mem(packet_cfg)
    # Change udpcomplete local config and apply reset while the udp ip is working (the reset shouldn't have effect until finishing the operation)
    while (tb.dut.controller_inst.tx_payload_axis_tvalid.value == 0): await RisingEdge(tb.dut.clk)
    new_submask = "255.255.0.0"
    await tb.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_SNM_0_N_O"], ip_str_to_ip_bytes(new_submask))
    # subnet_mask should not be updated with new value until transaction has finished
    await tb.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (1).to_bytes(1, 'big'))
    await RisingEdge(tb.dut.clk)
    await tb.s_axil_ctrl.write(TB.axil_ctrl_addresses_dic["ADDR_RES_0_Y_O"], (0).to_bytes(1, 'big'))
    await RisingEdge(tb.dut.clk)
    # Wait for transaction to finish
    await tb.check_tx_packet_at_sfp(packet_cfg)
    # Now subnet_mask should have been updated
    await RisingEdge(tb.dut.clk)
    # Now subnet_mask should have been updated
    assert tb.dut.controller_inst.subnet_mask.value == tb.dut.controller_inst.subnet_mask_from_ps.value
    # Leave some extra time to make visual simulation look better
    for _ in range(100): await RisingEdge(dut.clk)

###################################################################################
# paths, cocotb and simulator definitions
###################################################################################

tests_dir = os.path.abspath(os.path.dirname(__file__))
rtl_dir = os.path.abspath(os.path.join(tests_dir, '..', '..', 'rtl'))
lib_dir = os.path.abspath(os.path.join(rtl_dir, '..', 'lib'))
axis_rtl_dir = os.path.abspath(os.path.join(lib_dir, 'eth', 'lib', 'axis', 'rtl'))
eth_rtl_dir = os.path.abspath(os.path.join(lib_dir, 'eth', 'rtl'))


def test_fpga_core(request):
    dut = "fpga_core"
    module = os.path.splitext(os.path.basename(__file__))[0]
    toplevel = dut

    verilog_sources = [
        os.path.join(rtl_dir, f"{dut}_10g.v"),
        os.path.join(rtl_dir, "controller_64.v"),
        os.path.join(rtl_dir, "circular_buffer.v"),
        os.path.join(rtl_dir, "utils.v"),
        os.path.join(rtl_dir, "axis_header_adder.v"),
        os.path.join(rtl_dir, "axis_header_remover.v"),
        os.path.join(rtl_dir, "config_regs_AXI_Manager.v"),
        os.path.join(rtl_dir, "ctrl_axi_regs.v"),
        os.path.join(rtl_dir, "pulse_on_edge.v"),
        os.path.join(rtl_dir, "axis_udp_port_filter.v"),
        os.path.join(eth_rtl_dir, "eth_mac_10g_fifo.v"),
        os.path.join(eth_rtl_dir, "eth_mac_10g.v"),
        os.path.join(eth_rtl_dir, "axis_xgmii_rx_64.v"),
        os.path.join(eth_rtl_dir, "axis_xgmii_tx_64.v"),
        os.path.join(eth_rtl_dir, "lfsr.v"),
        os.path.join(eth_rtl_dir, "eth_axis_rx.v"),
        os.path.join(eth_rtl_dir, "eth_axis_tx.v"),
        os.path.join(eth_rtl_dir, "udp_complete_64.v"),
        os.path.join(eth_rtl_dir, "udp_checksum_gen_64.v"),
        os.path.join(eth_rtl_dir, "udp_64.v"),
        os.path.join(eth_rtl_dir, "udp_ip_rx_64.v"),
        os.path.join(eth_rtl_dir, "udp_ip_tx_64.v"),
        os.path.join(eth_rtl_dir, "ip_complete_64.v"),
        os.path.join(eth_rtl_dir, "ip_64.v"),
        os.path.join(eth_rtl_dir, "ip_eth_rx_64.v"),
        os.path.join(eth_rtl_dir, "ip_eth_tx_64.v"),
        os.path.join(eth_rtl_dir, "ip_arb_mux.v"),
        os.path.join(eth_rtl_dir, "arp.v"),
        os.path.join(eth_rtl_dir, "arp_cache.v"),
        os.path.join(eth_rtl_dir, "arp_eth_rx.v"),
        os.path.join(eth_rtl_dir, "arp_eth_tx.v"),
        os.path.join(eth_rtl_dir, "eth_arb_mux.v"),
        os.path.join(axis_rtl_dir, "arbiter.v"),
        os.path.join(axis_rtl_dir, "priority_encoder.v"),
        os.path.join(axis_rtl_dir, "axis_fifo.v"),
        os.path.join(axis_rtl_dir, "axis_async_fifo.v"),
        os.path.join(axis_rtl_dir, "axis_async_fifo_adapter.v"),
        os.path.join(axis_rtl_dir, "axis_register.v"),
        os.path.join(axis_rtl_dir, "axi_dma_wr.v"),
        os.path.join(axis_rtl_dir, "axi_dma_rd.v"),
    ]

    parameters = {}

    # parameters['A'] = val

    extra_env = {f'PARAM_{k}': str(v) for k, v in parameters.items()}

    sim_build = os.path.join(tests_dir, "sim_build",
        request.node.name.replace('[', '-').replace(']', ''))

    cocotb_test.simulator.run(
        python_search=[tests_dir],
        verilog_sources=verilog_sources,
        toplevel=toplevel,
        module=module,
        parameters=parameters,
        sim_build=sim_build,
        extra_env=extra_env,
    )
