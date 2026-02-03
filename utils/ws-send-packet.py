########################################
# Imports
########################################

from scapy.layers.l2 import Ether
from scapy.all import IP, UDP
from scapy.sendrecv import sendp

########################################
# Setup
########################################

INTERFACE = 'Ethernet 3'
INTERFACE_MAC = '98:b7:85:1f:4b:65'
INTERFACE_IP = '192.168.1.2'

KRIA_MAC = '02:00:00:00:00:00'
KRIA_IP = '192.168.1.128'

SRC_PORT = 7410
DST_PORT = 1234
PAYLOAD = 'H'
PAYLOAD_LEN = 530

########################################
# Send packet to IF
########################################

interface = INTERFACE
eth = Ether(src=INTERFACE_MAC, dst=KRIA_MAC)
ip = IP(src=INTERFACE_IP, dst=KRIA_IP)
udp = UDP(sport=SRC_PORT, dport=DST_PORT)
payload = PAYLOAD * PAYLOAD_LEN

# Combine layers to form the packet
pkt = eth / ip / udp / payload

print("Sending UDP packet...")
sendp(pkt, iface=interface)
