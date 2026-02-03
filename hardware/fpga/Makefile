
# FPGA settings
FPGA_PART = XCK26-SFVC784-2LV-C
FPGA_TOP = udp_ip_core_wrapper
PROJECT = udp_ip_core
FPGA_ARCH = zynquplus

VARIANT ?= 10g

# Files for synthesis
SYN_FILES = rtl/utils.v
SYN_FILES += rtl/circular_buffer.v
SYN_FILES += rtl/axis_header_adder.v
SYN_FILES += rtl/axis_header_remover.v
SYN_FILES += rtl/user_rst_handler.v
SYN_FILES += rtl/config_regs_AXI_Manager.v
SYN_FILES += rtl/ctrl_axi_regs.v
SYN_FILES += rtl/pulse_on_posedge.v
SYN_FILES += rtl/axis_udp_port_filter.v
SYN_FILES += rtl/axi_dma_wr.v
SYN_FILES += rtl/axi_dma_rd.v
SYN_FILES += lib/eth/rtl/lfsr.v
SYN_FILES += lib/eth/rtl/eth_axis_rx.v
SYN_FILES += lib/eth/rtl/eth_axis_tx.v
SYN_FILES += lib/eth/rtl/ip_arb_mux.v
SYN_FILES += lib/eth/rtl/arp.v
SYN_FILES += lib/eth/rtl/arp_cache.v
SYN_FILES += lib/eth/rtl/arp_eth_rx.v
SYN_FILES += lib/eth/rtl/arp_eth_tx.v
SYN_FILES += lib/eth/rtl/eth_arb_mux.v
SYN_FILES += lib/eth/lib/axis/rtl/arbiter.v
SYN_FILES += lib/eth/lib/axis/rtl/priority_encoder.v
SYN_FILES += lib/eth/lib/axis/rtl/axis_fifo.v
SYN_FILES += lib/eth/lib/axis/rtl/axis_async_fifo.v
SYN_FILES += lib/eth/lib/axis/rtl/axis_async_fifo_adapter.v
SYN_FILES += lib/eth/lib/axis/rtl/sync_reset.v
SYN_FILES += lib/eth/lib/axis/rtl/axis_register.v

# 10G
SYN_FILES_10G = rtl/fpga_10g.v
SYN_FILES_10G += rtl/fpga_core_10g.v
SYN_FILES_10G += rtl/controller_64.v
SYN_FILES_10G += rtl/eth_xcvr_phy_wrapper.v
SYN_FILES_10G += lib/eth/rtl/eth_mac_10g_fifo.v
SYN_FILES_10G += lib/eth/rtl/eth_mac_10g.v
SYN_FILES_10G += lib/eth/rtl/axis_xgmii_rx_64.v
SYN_FILES_10G += lib/eth/rtl/axis_xgmii_tx_64.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_rx.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_rx_if.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_rx_frame_sync.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_rx_ber_mon.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_rx_watchdog.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_tx.v
SYN_FILES_10G += lib/eth/rtl/eth_phy_10g_tx_if.v
SYN_FILES_10G += lib/eth/rtl/xgmii_baser_dec_64.v
SYN_FILES_10G += lib/eth/rtl/xgmii_baser_enc_64.v
SYN_FILES_10G += lib/eth/rtl/udp_complete_64.v
SYN_FILES_10G += lib/eth/rtl/udp_checksum_gen_64.v
SYN_FILES_10G += lib/eth/rtl/udp_64.v
SYN_FILES_10G += lib/eth/rtl/udp_ip_rx_64.v
SYN_FILES_10G += lib/eth/rtl/udp_ip_tx_64.v
SYN_FILES_10G += lib/eth/rtl/ip_complete_64.v
SYN_FILES_10G += lib/eth/rtl/ip_64.v
SYN_FILES_10G += lib/eth/rtl/ip_eth_rx_64.v
SYN_FILES_10G += lib/eth/rtl/ip_eth_tx_64.v

# 1G
SYN_FILES_1G = rtl/fpga_1g.v
SYN_FILES_1G += rtl/fpga_core_1g.v
SYN_FILES_1G += rtl/controller.v
SYN_FILES_1G += rtl/mdio_master.v
SYN_FILES_1G += rtl/axis_adapter.v
SYN_FILES_1G += lib/eth/rtl/iddr.v
SYN_FILES_1G += lib/eth/rtl/oddr.v
SYN_FILES_1G += lib/eth/rtl/ssio_ddr_in.v
SYN_FILES_1G += lib/eth/rtl/ssio_ddr_out.v
SYN_FILES_1G += lib/eth/rtl/rgmii_phy_if.v
SYN_FILES_1G += lib/eth/rtl/eth_mac_1g_rgmii_fifo.v
SYN_FILES_1G += lib/eth/rtl/eth_mac_1g_rgmii.v
SYN_FILES_1G += lib/eth/rtl/eth_mac_1g.v
SYN_FILES_1G += lib/eth/rtl/axis_gmii_rx.v
SYN_FILES_1G += lib/eth/rtl/axis_gmii_tx.v
SYN_FILES_1G += lib/eth/rtl/udp_complete.v
SYN_FILES_1G += lib/eth/rtl/udp_checksum_gen.v
SYN_FILES_1G += lib/eth/rtl/udp.v
SYN_FILES_1G += lib/eth/rtl/udp_ip_rx.v
SYN_FILES_1G += lib/eth/rtl/udp_ip_tx.v
SYN_FILES_1G += lib/eth/rtl/ip_complete.v
SYN_FILES_1G += lib/eth/rtl/ip.v
SYN_FILES_1G += lib/eth/rtl/ip_eth_rx.v
SYN_FILES_1G += lib/eth/rtl/ip_eth_tx.v

# XDC files
XDC_FILES = dcs/main.xdc
XDC_FILES += lib/eth/syn/vivado/eth_mac_fifo.tcl
XDC_FILES += lib/eth/lib/axis/syn/vivado/axis_async_fifo.tcl
XDC_FILES += lib/eth/lib/axis/syn/vivado/sync_reset.tcl

# 10G
XDC_FILES_10G = dcs/10g.xdc

# 1G
XDC_FILES_1G = dcs/1g.xdc
XDC_FILES_1G += lib/eth/syn/vivado/rgmii_phy_if.tcl
XDC_FILES_1G += lib/eth/syn/vivado/eth_mac_1g_rgmii.tcl

# IP
IP_TCL_FILES_10G += ip/zynq_ps_10g.tcl
IP_TCL_FILES_10G += ip/eth_xcvr_gt.tcl

# IP
IP_TCL_FILES_1G += ip/zynq_ps_1g.tcl

# Variant selection logic
ifeq ($(VARIANT),1g)
SYN_FILES := $(SYN_FILES) $(SYN_FILES_1G)
XDC_FILES := $(XDC_FILES) $(XDC_FILES_1G)
IP_TCL_FILES := $(IP_TCL_FILES_1G)
else ifeq ($(VARIANT),10g)
SYN_FILES := $(SYN_FILES) $(SYN_FILES_10G)
XDC_FILES := $(XDC_FILES) $(XDC_FILES_10G)
IP_TCL_FILES := $(IP_TCL_FILES_10G)
else
$(error Unknown VARIANT=$(VARIANT). Use 1g or 10g)
endif

# TCL script for generating pfm
PFM_TCL += fpga/pfm.tcl

include ../common/vivado.mk

program: $(PROJECT).bit
	echo "open_hw" > program.tcl
	echo "connect_hw_server" >> program.tcl
	echo "open_hw_target" >> program.tcl
	echo "current_hw_device [lindex [get_hw_devices] 0]" >> program.tcl
	echo "refresh_hw_device -update_hw_probes false [current_hw_device]" >> program.tcl
	echo "set_property PROGRAM.FILE {$(PROJECT).bit} [current_hw_device]" >> program.tcl
	echo "program_hw_devices [current_hw_device]" >> program.tcl
	echo "exit" >> program.tcl
	vivado -nojournal -nolog -mode batch -source program.tcl

dtbo:
	dtc -I dts -O dtb -o output_overlay/$(PROJECT)/$(PROJECT).dtbo $(PROJECT).dtsi