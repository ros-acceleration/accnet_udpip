#!/bin/bash
# ------------------------------------------------------------------------------
# Insert the kernel module and load bitstream onto FPGA
# ------------------------------------------------------------------------------
sudo insmod udp_core.ko
sudo xmutil unloadapp
sudo xmutil loadapp udp_ip_core
