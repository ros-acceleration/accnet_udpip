// SPDX-License-Identifier: GPL-2.0+

/* udp-core-pkt.c
 *
 * Handling logic for UDP/raweth packets
 *
 * Copyright (C) Accelerat S.r.l.
 */

// #include <linux/netdevice.h>
// #include <linux/inetdevice.h>
// #include <linux/inet.h>
// #include <linux/ip.h>

#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/ip.h>
#include <linux/dma-mapping.h>
#include <linux/ethtool.h>
#include <linux/string.h>
#include <net/route.h>
#include <net/addrconf.h>
#include <linux/inet.h>

#include "udp_core.h"

#define DHCP_ADDR 0xFFFFFFFF
#define MDSN_ADDR 0xFB0000E0

#define UDPHDR_PAYLOAD_DATA(udph) ((u8*)udph + UDP_HLEN)

static int udp_core_pkt_compose_classic(
    struct sk_buff* skb,
    struct udp_core_raw_packet* udp_packet
)
{
    struct iphdr *ip_header;
    struct udphdr* udph;

    /**
     * NOTE: When using a classic socket, only UDP over IPv4 is currently 
     * supported by this driver. Refuse everything but UDP on IPv4.
     */

    if (skb->protocol != htons(ETH_P_IP)) 
    {
        return -1; // not IPv4 - refused.
    }

    ip_header = ip_hdr(skb);

    if (ip_header == NULL || ip_header->protocol != IPPROTO_UDP)
    {
        return -1; // not UDP - refused.
    }

    /**
     * NOTE: Even if UDP on IPv4, some special UDP packets are not supported
     * due to missing hardware support (i.e. DHCP, mDNS, ...). Refuse them.
     */
    if (ip_header->daddr == DHCP_ADDR || ip_header->daddr == MDSN_ADDR)
    {
        return -1;
    }

    udph = udp_hdr(skb);

    /**
     * NOTE: The current version of RTL is not supporting fragmentation
     * at HW level. Therefore, we need to drop this packet.
     */
    if (ntohs(udph->len) - UDP_HLEN > MAX_PAYLOAD_SIZE)
    {
	    pr_err("udp-core: packet too long! (src  = %u)\n", ntohs(udph->len));
        return -1; // packet too long
    }

    // extract the source and destination IP addresses
    udp_packet->dest_ip = ntohl(ip_header->daddr);
    udp_packet->source_ip = ntohl(ip_header->saddr);
    udp_packet->dest_port = ntohs(udph->dest);
    udp_packet->source_port = ntohs(udph->source);
    udp_packet->payload_size_bytes = ntohs(udph->len) - UDP_HLEN;
    udp_packet->payload = (u64*)UDPHDR_PAYLOAD_DATA(udph);

    return 0;
}

static int udp_core_pkt_compose_raw(
    struct sk_buff* skb, 
    struct udp_core_raw_packet* udp_packet)
{
    int retval;

    /**
     * NOTE: With raw sockets, only UDP over IPv4 is supported.
     */

    if (skb->protocol == ntohs(ETH_P_IP))
    {
        retval = udp_core_pkt_compose_classic(skb, udp_packet);
    }
    else
    {
        retval = -1;
    }

    return retval;
}

int udp_core_pkt_compose(
    struct sk_buff* skb,
    struct udp_core_raw_packet* udp_packet
)
{
    int retval;
    struct sock *sk;

    /**
     * NOTE: Independently from the kind of socket used (either raw or classic)
     * only UDP over IPv4 packets can be sent out by the device. However, a few
     * exceptions are admitted. Each exception is document alongside.
     * 
     * Both _raw and _classic shall return -1 when they cannot fullfil the
     * packet compose request (due to missing support) or return 0 otherwise.
     */

    sk = skb->sk;

    if (sk != NULL && sk->sk_family == PF_PACKET)
    {
        retval = udp_core_pkt_compose_raw(skb, udp_packet);
    }
    #ifdef NON_RAW_USAGE_ENABLED
    else
    {
        retval = udp_core_pkt_compose_classic(skb, udp_packet);
    }
    #else
    else
    {
        retval = -1;
    }
    #endif
    
    return retval;
}

struct udp_packet premade_udp_packet = 
{
    .dest_mac = IF_DEFAULT_MAC_ADDR,
    .src_mac = GW_MAC_OCTETS,
    .ether_type = htons(ETH_P_IP),
    .ihl = sizeof(struct iphdr) / 4,
    .version = IPVERSION,
    .tos = 0,
    .ident = 0x1,
    .flags = 0x0,
    .frag_offset = 0,
    .ttl = IPDEFTTL,
    .protocol = IPPROTO_UDP,
    .checksum = 0
};

static void udp_core_pkt_decompose_no_strip(
    struct sk_buff *skb,
    struct udp_core_raw_packet* raw_udp_packet,
    struct udp_packet* udp_packet
)
{
    u64 payload_size_bytes;
    struct iphdr *iph;

    // save payload size
    payload_size_bytes = raw_udp_packet->payload_size_bytes;

    // populate missing UDP fields
    udp_packet->total_len = htons(IPV4_HLEN + UDP_HLEN + payload_size_bytes);
    udp_packet->source_ip = htonl(raw_udp_packet->source_ip);
    udp_packet->dest_ip = htonl(raw_udp_packet->dest_ip);
    udp_packet->source_port = htons(raw_udp_packet->source_port);
    udp_packet->dest_port = htons(raw_udp_packet->dest_port);
    udp_packet->payload_len = htons(raw_udp_packet->payload_size_bytes + UDP_HLEN);

    /**
     * NOTE: When populating skb with a put data, all bytes are considered part
     * of data. Therefore part of header should be pull out and headers
     * pointers should be correctly set.
     */

    // copy into socket buffer header and (actual) payload received
    skb_put_data(skb, udp_packet, PKT_HLEN);
    skb_put_data(skb, raw_udp_packet->payload, payload_size_bytes);

    // reset socket buffer pointer and pull frame header from data
    skb_reset_mac_header(skb);
    skb_pull(skb, ETH_HLEN);

    // reset socket buffer network pointer 
    skb_reset_network_header(skb);

    // set transport header pointer to network + IPV4_HLEN
    skb_set_transport_header(skb, IPV4_HLEN);

    // set protocol to IPv4 and checksum as unnecessary
    skb->protocol = htons(ETH_P_IP);
    skb->ip_summed = CHECKSUM_UNNECESSARY;

    /**
     * TODO: This checksum should come from device, in the packet header!
     * 
     * With the current RTL implementation, IP checksum is removed when
     * unpacking UDP payload. However, in order to have a valid UDP/IP packet
     * for SKB, kernel stack requires it. So, let software compute it here
     * and remove it as soon as it is available from hardware with the other
     * header information.
     */
    
    iph = ip_hdr(skb);
    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

    return;
}

void udp_core_pkt_decompose(
    struct sk_buff* skb,
    struct udp_core_raw_packet* raw_udp_packet
)
{
    /**
     * Packet decomposition takes a payload (as received from FPGA device)
     * and build up a valid SKB to be sent to upper layer. Currently 
     * decomposition supports only UDP/IPv4 (without data strip).
     */
    udp_core_pkt_decompose_no_strip(skb, raw_udp_packet, &premade_udp_packet);
}
