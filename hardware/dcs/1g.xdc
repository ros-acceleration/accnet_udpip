# Gigabit Ethernet RGMII PHY
set_property -dict {LOC D4 IOSTANDARD LVCMOS18} [get_ports phy_rx_clk]
set_property -dict {LOC A1 IOSTANDARD LVCMOS18} [get_ports {phy_rxd[0]}]
set_property -dict {LOC B3 IOSTANDARD LVCMOS18} [get_ports {phy_rxd[1]}]
set_property -dict {LOC A3 IOSTANDARD LVCMOS18} [get_ports {phy_rxd[2]}]
set_property -dict {LOC B4 IOSTANDARD LVCMOS18} [get_ports {phy_rxd[3]}]
set_property -dict {LOC A4 IOSTANDARD LVCMOS18} [get_ports phy_rx_ctl]
set_property -dict {LOC A2 IOSTANDARD LVCMOS18} [get_ports phy_tx_clk]
set_property -dict {LOC E1 IOSTANDARD LVCMOS18} [get_ports {phy_txd[0]}]
set_property -dict {LOC D1 IOSTANDARD LVCMOS18} [get_ports {phy_txd[1]}]
set_property -dict {LOC F2 IOSTANDARD LVCMOS18} [get_ports {phy_txd[2]}]
set_property -dict {LOC E2 IOSTANDARD LVCMOS18} [get_ports {phy_txd[3]}]
set_property -dict {LOC F1 IOSTANDARD LVCMOS18} [get_ports phy_tx_ctl]
set_property -dict {LOC B1 IOSTANDARD LVCMOS18} [get_ports phy_reset_n]
set_property -dict {LOC F3 IOSTANDARD LVCMOS18} [get_ports phy_mdio]
set_property -dict {LOC G3 IOSTANDARD LVCMOS18} [get_ports phy_mdc]

create_clock -period 8.000 -name phy_rx_clk [get_ports phy_rx_clk]

set_false_path -to [get_ports phy_reset_n]
set_output_delay 0.000 [get_ports phy_reset_n]
set_false_path -to [get_ports {phy_mdio phy_mdc}]
set_output_delay 0.000 [get_ports {phy_mdio phy_mdc}]
set_false_path -from [get_ports phy_mdio]
set_input_delay 0.000 [get_ports phy_mdio]

# IDELAY on RGMII from PHY chip
set_property IDELAY_VALUE 0 [get_cells {phy_rx_ctl_idelay phy_rxd_idelay_*}]

