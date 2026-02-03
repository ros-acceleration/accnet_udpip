########################################
# Imports
########################################

import socket
from scapy.layers.l2 import Ether, ARP
from scapy.sendrecv import sendp, sniff

########################################
# Setup
########################################

INTERFACE = 'Ethernet 3'
INTERFACE_MAC = '98:b7:85:1f:4b:65'
INTERFACE_IP = '192.168.1.2'
LOCAL_PORT = 1234

KRIA_IP = '192.168.1.128' # IP to be solved via ARP request
KRIA_PORT = 7400
KRIA_MAC = '02:00:00:00:00:00'

########################################
# Opens UDP socket
########################################

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((INTERFACE_IP, LOCAL_PORT))
print(f"Opened UDP Socket at port {LOCAL_PORT}...")

########################################
# Waits for arp request and send arp response back
########################################

interface = INTERFACE
eth = Ether(src=INTERFACE_MAC, dst=KRIA_MAC)
arp = ARP(hwtype=1, ptype=0x0800, hwlen=6, plen=4, op=2,
    hwsrc=INTERFACE_MAC, psrc=INTERFACE_IP,
    hwdst=KRIA_MAC, pdst=KRIA_IP)
resp_pkt = eth / arp
filter_expr = 'arp and src host ' + KRIA_IP

print("Waiting for ARP request...")
while True:
    arp_request = sniff(count=1, iface=interface, filter=filter_expr)
    if arp_request[0].op == 1:
        print("Sending ARP response...")
        sendp(resp_pkt, iface=interface)
        break

while True:
    ########################################
    # Listens UDP
    ########################################

    print("Waiting for receiving UDP packet...")
    data, addr = sock.recvfrom(1024)  # receive up to 1024 bytes of data
    decoded_data = data.decode('utf-8')  # decode the data to UTF-8 string
    print(decoded_data)

    ########################################
    # Replies UDP
    ########################################

    print("Sending ack back...")
    ack_message = "ack: " + decoded_data
    ack_message_encoded = ack_message.encode('utf-8')
    sock.sendto(ack_message_encoded, addr)
    print(ack_message)
