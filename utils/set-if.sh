#!/bin/bash
# This script allows to set a static IP for the specified interface using 'nmcli'
# Use 'nmcli connection show' to find the interface you want to configure and check the configuration

IF="Wired connection 3"
IP=192.168.1.128/24
GW=192.168.1.2

sudo nmcli connection modify "$IF" ipv4.method manual ipv4.addresses $IP ipv4.gateway $GW
sudo nmcli connection down "$IF"
sudo nmcli connection up "$IF"