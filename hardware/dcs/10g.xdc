# SFP+ Interface
set_property -dict {LOC T2 } [get_ports sfp0_rx_p] ;# GTH_DP2_C2M_P, som240_2_b1
set_property -dict {LOC T1 } [get_ports sfp0_rx_n] ;# GTH_DP2_C2M_N, som240_2_b2
set_property -dict {LOC R4 } [get_ports sfp0_tx_p] ;# GTH_DP2_M2C_P, som240_2_b5
set_property -dict {LOC R3 } [get_ports sfp0_tx_n] ;# GTH_DP2_M2C_N, som240_2_b6

set_property -dict {LOC Y6 } [get_ports sfp_mgt_refclk_0_p] ;# GTH_REFCLK0_C2M_P via U90, SOM240_2 C3
set_property -dict {LOC Y5 } [get_ports sfp_mgt_refclk_0_n] ;# GTH_REFCLK0_C2M_N via U90, SOM240_2 C4
set_property -dict {LOC Y10 IOSTANDARD LVCMOS18 SLEW SLOW DRIVE 8 } [get_ports sfp0_tx_disable_b]  ;# HDB19, SOM240_2_A47

# 156.25 MHz MGT reference clock
create_clock -period 6.400 -name sfp_mgt_refclk_0 [get_ports sfp_mgt_refclk_0_p]

set_false_path -to [get_ports {sfp0_tx_disable_b}]
set_output_delay 0 [get_ports {sfp0_tx_disable_b}]
