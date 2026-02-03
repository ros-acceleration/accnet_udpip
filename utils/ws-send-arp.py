########################################
# Imports
########################################

from scapy.layers.l2 import Ether, ARP
from scapy.sendrecv import sendp

########################################
# Setup
########################################

INTERFACE = 'Ethernet 3'
INTERFACE_MAC = '98:b7:85:1f:4b:65'
INTERFACE_IP = '192.168.1.2'
REQUESTED_IP = '192.168.1.128' # IP to be solved via ARP request

########################################
# Send ARP request and wait resp back
########################################

interface = INTERFACE
eth = Ether(src=INTERFACE_MAC, dst='ff:ff:ff:ff:ff:ff')
arp = ARP(hwtype=1, ptype=0x0800, hwlen=6, plen=4, op=1,
    hwsrc=INTERFACE_MAC, psrc=INTERFACE_IP,
    hwdst='00:00:00:00:00:00', pdst=REQUESTED_IP)
pkt = eth / arp

print("Send ARP request...")
sendp(pkt, iface=interface)
