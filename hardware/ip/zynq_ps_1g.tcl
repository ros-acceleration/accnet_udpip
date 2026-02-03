# Copyright 2022, The Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    1. Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#
#    2. Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS OF THE UNIVERSITY OF CALIFORNIA ''AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE REGENTS OF THE UNIVERSITY OF CALIFORNIA OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation are those
# of the authors and should not be interpreted as representing official policies,
# either expressed or implied, of The Regents of the University of California.

# create block design
create_bd_design "udp_ip_core"

##############################
# Instantiate blocks
##############################

# Zynq PS
set zynq_ultra_ps [ create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ultra_ps ]
set_property -dict [list \
    CONFIG.PSU_BANK_0_IO_STANDARD {LVCMOS18} \
    CONFIG.PSU_BANK_1_IO_STANDARD {LVCMOS18} \
    CONFIG.PSU_BANK_2_IO_STANDARD {LVCMOS18} \
    CONFIG.PSU_BANK_3_IO_STANDARD {LVCMOS33} \
    CONFIG.PSU_DYNAMIC_DDR_CONFIG_EN 1 \
    CONFIG.PSU__DDRC__COMPONENTS {UDIMM} \
    CONFIG.PSU__DDRC__DEVICE_CAPACITY {4096 MBits} \
    CONFIG.PSU__DDRC__SPEED_BIN {DDR4_2133P} \
    CONFIG.PSU__DDRC__ROW_ADDR_COUNT {15} \
    CONFIG.PSU__DDRC__T_RC {46.5} \
    CONFIG.PSU__DDRC__T_FAW {21.0} \
    CONFIG.PSU__DDRC__DDR4_ADDR_MAPPING {0} \
    CONFIG.PSU__DDRC__FREQ_MHZ {1067} \
    CONFIG.PSU__PMU__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__PMU__GPI0__ENABLE {1} \
    CONFIG.PSU__PMU__GPI1__ENABLE {0} \
    CONFIG.PSU__PMU__GPI2__ENABLE {0} \
    CONFIG.PSU__PMU__GPI3__ENABLE {0} \
    CONFIG.PSU__PMU__GPI4__ENABLE {0} \
    CONFIG.PSU__PMU__GPI5__ENABLE {0} \
    CONFIG.PSU__QSPI__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__QSPI__PERIPHERAL__MODE {Dual Parallel} \
    CONFIG.PSU__QSPI__GRP_FBCLK__ENABLE {1} \
    CONFIG.PSU__CAN1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__CAN1__PERIPHERAL__IO {MIO 24 .. 25} \
    CONFIG.PSU__GPIO0_MIO__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__GPIO1_MIO__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__I2C0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__I2C0__PERIPHERAL__IO {MIO 14 .. 15} \
    CONFIG.PSU__I2C1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__I2C1__PERIPHERAL__IO {MIO 16 .. 17} \
    CONFIG.PSU__UART0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__UART0__PERIPHERAL__IO {MIO 18 .. 19} \
    CONFIG.PSU__UART1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__UART1__PERIPHERAL__IO {MIO 20 .. 21} \
    CONFIG.PSU__SD1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__SD1__GRP_CD__ENABLE {1} \
    CONFIG.PSU__SD1__GRP_WP__ENABLE {1} \
    CONFIG.PSU__ENET3__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__ENET3__GRP_MDIO__ENABLE {1} \
    CONFIG.PSU__USB0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__USB0__REF_CLK_SEL {Ref Clk2} \
    CONFIG.PSU__USB3_0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__USB3_0__PERIPHERAL__IO {GT Lane2} \
    CONFIG.PSU__DISPLAYPORT__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__DPAUX__PERIPHERAL__IO {MIO 27 .. 30} \
    CONFIG.PSU__DP__REF_CLK_SEL {Ref Clk3} \
    CONFIG.PSU__DP__LANE_SEL {Single Lower} \
    CONFIG.PSU__SATA__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__SATA__LANE1__IO {GT Lane3} \
    CONFIG.PSU__PCIE__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__PCIE__PERIPHERAL__ROOTPORT_IO {MIO 31} \
    CONFIG.PSU__PCIE__DEVICE_PORT_TYPE {Root Port} \
    CONFIG.PSU__PCIE__BAR0_ENABLE {0} \
    CONFIG.PSU__PCIE__CLASS_CODE_BASE {0x06} \
    CONFIG.PSU__PCIE__CLASS_CODE_SUB {0x04} \
    CONFIG.PSU__SWDT0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__SWDT1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__TTC0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__TTC1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__TTC2__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__TTC3__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__USE__M_AXI_GP0 {1} \
    CONFIG.PSU__MAXIGP0__DATA_WIDTH {32} \
    CONFIG.PSU__USE__M_AXI_GP1 {0} \
    CONFIG.PSU__USE__M_AXI_GP2 {0} \
    CONFIG.PSU__USE__S_AXI_GP0 {0} \
    CONFIG.PSU__USE__S_AXI_GP0 {1} \
    CONFIG.PSU__USE__S_AXI_GP2 {0} \
    CONFIG.PSU__USE__IRQ0 {1} \
    CONFIG.PSU__CRF_APB__ACPU_CTRL__SRCSEL {APLL} \
    CONFIG.PSU__CRF_APB__DDR_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRF_APB__DP_VIDEO_REF_CTRL__SRCSEL {VPLL} \
    CONFIG.PSU__CRF_APB__DP_AUDIO_REF_CTRL__SRCSEL {RPLL} \
    CONFIG.PSU__CRF_APB__DP_STC_REF_CTRL__SRCSEL {RPLL} \
    CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__FREQMHZ {667} \
    CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__SRCSEL {APLL} \
    CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__FREQMHZ {667} \
    CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__SRCSEL {APLL} \
    CONFIG.PSU__CRF_APB__GPU_REF_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRF_APB__TOPSW_MAIN_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRF_APB__TOPSW_LSBUS_CTRL__SRCSEL {IOPLL} \
    CONFIG.PSU__CRL_APB__ADMA_REF_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRL_APB__CPU_R5_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRL_APB__IOU_SWITCH_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRL_APB__LPD_LSBUS_CTRL__SRCSEL {IOPLL} \
    CONFIG.PSU__CRL_APB__LPD_SWITCH_CTRL__SRCSEL {DPLL} \
    CONFIG.PSU__CRL_APB__PCAP_CTRL__SRCSEL {IOPLL} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {300} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__SRCSEL {IOPLL} \
    CONFIG.PSU__CRL_APB__SDIO1_REF_CTRL__SRCSEL {IOPLL} \
] $zynq_ultra_ps

# reset
set proc_sys_reset [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset ]

# fpga
create_bd_cell -type module -reference fpga fpga_inst

# smartconnects + interrupts
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property -dict [list CONFIG.NUM_SI {1}] [get_bd_cells smartconnect_0]
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_1
set_property -dict [list CONFIG.NUM_MI {2} CONFIG.NUM_SI {1}] [get_bd_cells smartconnect_1]

update_compile_order -fileset sources_1

##############################
# reset block
##############################

# Clock
set pl_clk0 [get_bd_pins $zynq_ultra_ps/pl_clk0]
connect_bd_net $pl_clk0 [get_bd_pins $proc_sys_reset/slowest_sync_clk]
set pl_clk0_busif [list]

# Reset
set pl_resetn0 [get_bd_pins $zynq_ultra_ps/pl_resetn0]
connect_bd_net $pl_resetn0 [get_bd_pins $proc_sys_reset/ext_reset_in]

##############################
# AXI HP0 - smartconnect 0 - fpga_core_axi
##############################

connect_bd_net [get_bd_pins zynq_ultra_ps/saxihpc0_fpd_aclk] [get_bd_pins fpga_inst/clk_125mhz_o]
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M00_AXI] [get_bd_intf_pins zynq_ultra_ps/S_AXI_HPC0_FPD]

connect_bd_net [get_bd_pins fpga_inst/fpga_core_axi_aclk] [get_bd_pins fpga_inst/clk_125mhz_o]
connect_bd_net [get_bd_pins fpga_inst/fpga_core_axi_aresetn] [get_bd_pins fpga_inst/rstn_125mhz_o]
# set_property -dict [list CONFIG.CLK_DOMAIN {clk_125mhz} CONFIG.FREQ_HZ {125000000}] [get_bd_intf_pins fpga_inst/fpga_core_axi]
# set_property CONFIG.ASSOCIATED_BUSIF {get_bd_intf_ports fpga_inst/fpga_core_axi} [get_pins fpga_inst/clk_125mhz_o]
# set_property -dict [list CONFIG.CLK_DOMAIN {clk_125mhz_o} CONFIG.FREQ_HZ {125000000}] [get_bd_pins fpga_inst/fpga_core_axi_aclk]

connect_bd_intf_net [get_bd_intf_pins smartconnect_0/S00_AXI] [get_bd_intf_pins fpga_inst/fpga_core_axi]
connect_bd_net [get_bd_pins smartconnect_0/aclk] [get_bd_pins fpga_inst/clk_125mhz_o]
connect_bd_net [get_bd_pins smartconnect_0/aresetn] [get_bd_pins fpga_inst/rstn_125mhz_o]
assign_bd_address -target_address_space /fpga_inst/fpga_core_axi [get_bd_addr_segs zynq_ultra_ps/SAXIGP0/HPC0_DDR_LOW] -force
exclude_bd_addr_seg [get_bd_addr_segs zynq_ultra_ps/SAXIGP0/HPC0_QSPI] -target_address_space [get_bd_addr_spaces fpga_inst/fpga_core_axi]
exclude_bd_addr_seg [get_bd_addr_segs zynq_ultra_ps/SAXIGP0/HPC0_PCIE_LOW] -target_address_space [get_bd_addr_spaces fpga_inst/fpga_core_axi]
exclude_bd_addr_seg [get_bd_addr_segs zynq_ultra_ps/SAXIGP0/HPC0_LPS_OCM] -target_address_space [get_bd_addr_spaces fpga_inst/fpga_core_axi]
exclude_bd_addr_seg [get_bd_addr_segs zynq_ultra_ps/SAXIGP0/HPC0_DDR_HIGH] -target_address_space [get_bd_addr_spaces fpga_inst/fpga_core_axi]

##############################
# AXI HPM0 - smartconnect 1 - interr controller & fpga_core_axil
##############################

connect_bd_net [get_bd_pins zynq_ultra_ps/maxihpm0_fpd_aclk] [get_bd_pins fpga_inst/clk_125mhz_o]

# zynq ps HPM0 <-> smartconnect 1
connect_bd_intf_net [get_bd_intf_pins zynq_ultra_ps/M_AXI_HPM0_FPD] [get_bd_intf_pins smartconnect_1/S00_AXI]
connect_bd_net [get_bd_pins smartconnect_1/aclk] [get_bd_pins fpga_inst/clk_125mhz_o]
connect_bd_net [get_bd_pins smartconnect_1/aresetn] [get_bd_pins fpga_inst/rstn_125mhz_o]

# smartconnect 1 M01 <-> fpga_inst fpga_core_axil
connect_bd_net [get_bd_pins fpga_inst/fpga_core_axil_aclk] [get_bd_pins fpga_inst/clk_125mhz_o]
connect_bd_net [get_bd_pins fpga_inst/fpga_core_axil_aresetn] [get_bd_pins fpga_inst/rstn_125mhz_o]
# set_property -dict [list CONFIG.CLK_DOMAIN {clk_125mhz} CONFIG.FREQ_HZ {125000000}] [get_bd_intf_pins fpga_inst/fpga_core_axil]
# set_property -dict [list CONFIG.CLK_DOMAIN {clk_125mhz_o} CONFIG.FREQ_HZ {125000000}] [get_bd_pins fpga_inst/fpga_core_axil_aclk]

connect_bd_intf_net [get_bd_intf_pins smartconnect_1/M00_AXI] [get_bd_intf_pins fpga_inst/fpga_core_axil]
assign_bd_address -target_address_space /zynq_ultra_ps/Data [get_bd_addr_segs fpga_inst/fpga_core_axil/reg0] -force
set_property range 64K [get_bd_addr_segs {zynq_ultra_ps/Data/SEG_fpga_inst_reg0}]
set_property offset 0x00A0010000 [get_bd_addr_segs {zynq_ultra_ps/Data/SEG_fpga_inst_reg0}]

##############################
# interrupts
##############################

set_property CONFIG.NUM_PORTS {1} [get_bd_cells xlconcat_0]
connect_bd_net [get_bd_pins xlconcat_0/In0] [get_bd_pins fpga_inst/buffer_rx_pushed_interr]
connect_bd_net [get_bd_pins xlconcat_0/dout] [get_bd_pins zynq_ultra_ps/pl_ps_irq0]

##############################
# fpga core
##############################

# Make some pins externals and rename (to remove _0 at the end)
startgroup
make_bd_pins_external [get_bd_pins fpga_inst/clk_25mhz_ref] 
make_bd_pins_external [get_bd_pins fpga_inst/led] 
make_bd_pins_external [get_bd_pins fpga_inst/phy_rx_clk]
make_bd_pins_external [get_bd_pins fpga_inst/phy_rxd]
make_bd_pins_external [get_bd_pins fpga_inst/phy_rx_ctl]
make_bd_pins_external [get_bd_pins fpga_inst/phy_tx_clk]
make_bd_pins_external [get_bd_pins fpga_inst/phy_txd]
make_bd_pins_external [get_bd_pins fpga_inst/phy_tx_ctl]
make_bd_pins_external [get_bd_pins fpga_inst/phy_reset_n]
make_bd_pins_external [get_bd_pins fpga_inst/phy_mdio]
make_bd_pins_external [get_bd_pins fpga_inst/phy_mdc]
endgroup
set_property name clk_25mhz_ref [get_bd_ports clk_25mhz_ref_0]
set_property name led [get_bd_ports led_0]
set_property name phy_rx_clk [get_bd_ports phy_rx_clk_0]
set_property name phy_rxd [get_bd_ports phy_rxd_0]
set_property name phy_rx_ctl [get_bd_ports phy_rx_ctl_0]
set_property name phy_tx_clk [get_bd_ports phy_tx_clk_0]
set_property name phy_txd [get_bd_ports phy_txd_0]
set_property name phy_tx_ctl [get_bd_ports phy_tx_ctl_0]
set_property name phy_reset_n [get_bd_ports phy_reset_n_0]
set_property name phy_mdio [get_bd_ports phy_mdio_0]
set_property name phy_mdc [get_bd_ports phy_mdc_0]

# set_property -dict [list CONFIG.FREQ_HZ 125000000] [get_bd_ports phy_tx_clk]
# set_property -dict [list CONFIG.FREQ_HZ 125000000] [get_bd_ports phy_rx_clk]
# set_property -dict [list CONFIG.PHASE 90.0] [get_bd_ports phy_rx_clk]

##############################
# extensible platform
##############################

set_property platform.extensible true [current_project]
set_property PFM.AXI_PORT {M_AXI_HPM1_FPD {memport "M_AXI_GP" sptag "" memory "" is_range "false"} M_AXI_HPM0_LPD { memport "M_AXI_GP" sptag "" memory "" is_range "false" } S_AXI_HP1_FPD { memport "S_AXI_HP" sptag "HP1" memory "" is_range "false" } S_AXI_HP2_FPD { memport "S_AXI_HP" sptag "HP2" memory "" is_range "false" } S_AXI_HP3_FPD { memport "S_AXI_HP" sptag "HP3" memory "" is_range "false" } S_AXI_HPC1_FPD { memport "S_AXI_HP" sptag "HPC1" memory "" is_range "false" }} [get_bd_cells /zynq_ultra_ps]
set_property PFM.AXI_PORT {M02_AXI {memport "M_AXI_GP" sptag "" memory "" is_range "true"} M03_AXI {memport "M_AXI_GP" sptag "" memory "" is_range "true"} M04_AXI {memport "M_AXI_GP" sptag "" memory "" is_range "true"} M05_AXI {memport "M_AXI_GP" sptag "" memory "" is_range "true"} M06_AXI {memport "M_AXI_GP" sptag "" memory "" is_range "true"} M07_AXI {memport "M_AXI_GP" sptag "" memory "" is_range "true"}} [get_bd_cells /smartconnect_1]
set_property PFM.CLOCK {pl_clk0 {id "0" is_default "true" proc_sys_reset "/proc_sys_reset" status "fixed" freq_hz "299997009"}} [get_bd_cells /zynq_ultra_ps]
set_property PFM.IRQ {In1 {is_range "true"}} [get_bd_cells /xlconcat_0]

##############################
# misc
##############################

save_bd_design [current_bd_design]
validate_bd_design

save_bd_design [current_bd_design]
close_bd_design [current_bd_design]
