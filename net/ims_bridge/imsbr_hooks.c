// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "sprd-imsbr: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/netfilter_ipv6.h>
#include <linux/skbuff.h>
#include <linux/icmpv6.h>
#include <linux/in.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/esp.h>
#include <net/udp.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>
#include "imsbr_core.h"
#include "imsbr_packet.h"
#include "imsbr_hooks.h"
#include "imsbr_netlink.h"

struct espheader esphs[MAX_ESPS];
static bool is_icmp_error(struct nf_conntrack_tuple *nft)
{
	u8 protonum = nft->dst.protonum;
	__u8 type = nft->dst.u.icmp.type;

	switch (protonum) {
	case IPPROTO_ICMP:
		switch (type) {
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAMETERPROB:
		case ICMP_REDIRECT:
			return true;
		}
		fallthrough;
	case IPPROTO_ICMPV6:
		if (type < 128)
			return true;
	}

	return false;
}

int imsbr_parse_nfttuple(struct net *net, struct sk_buff *skb,
			 struct nf_conntrack_tuple *nft)
{
	struct nf_conntrack_tuple innertuple, origtuple;
	struct iphdr *ip = ip_hdr(skb);
	unsigned int inner_nhoff;
	u16 l3num;

	if (ip->version == 4) {
		l3num = AF_INET;
		inner_nhoff = skb_network_offset(skb) + ip_hdrlen(skb) +
			      sizeof(struct icmphdr);
	} else if (ip->version == 6) {
		l3num = AF_INET6;
		inner_nhoff = skb_network_offset(skb) + sizeof(struct ipv6hdr) +
			      sizeof(struct icmp6hdr);
	} else {
		pr_err("parse tuple fail, ver=%d\n", ip->version);
		goto fail;
	}

	if (!nf_ct_get_tuplepr(skb, skb_network_offset(skb), l3num, net,
			       nft)) {
		pr_err("parse tuple fail, len=%d, nhoff=%d, l3num=%d\n",
		       skb->len, skb_network_offset(skb), l3num);
		goto fail;
	}

	if (is_icmp_error(nft)) {
		/* If parse the inner tuple fail, we will just use the outer
		 * tuple!
		 */
		if (!nf_ct_get_tuplepr(skb, inner_nhoff, l3num, net,
				       &origtuple))
			return 0;

		rcu_read_lock();
		/**
		 * Ordinarily, we'd expect the inverted tupleproto, but it's
		 * been preserved inside the ICMP.
		 */
		if (!nf_ct_invert_tuple(&innertuple, &origtuple)) {
			rcu_read_unlock();
			return 0;
		}

		rcu_read_unlock();
		*nft = innertuple;
	}

	return 0;

fail:
	IMSBR_STAT_INC(imsbr_stats->nfct_get_fail);
	return -EINVAL;
}

static int imsbr_get_tuple(struct net *net, struct sk_buff *skb,
			   struct nf_conntrack_tuple *nft)
{
	enum ip_conntrack_info ctinfo;
	const struct nf_conn *ct;
	int dir;

	ct = nf_ct_get(skb, &ctinfo);
	if (likely(ct)) {
		dir = CTINFO2DIR(ctinfo);
		*nft = ct->tuplehash[dir].tuple;
		return 0;
	}

	IMSBR_STAT_INC(imsbr_stats->nfct_slow_path);
	return imsbr_parse_nfttuple(net, skb, nft);
}

static void
imsbr_modify_esp_seq(unsigned int spi, unsigned int seq)
{
	int i;

	for (i = 0; i < MAX_ESPS; i++) {
		if (esphs[i].spi == spi && esphs[i].seq < seq) {
			esphs[i].seq = seq;
			break;
		}
	}
}

/*for cp vowifi */
#define IP_V_FLAG 0x40
#define IP_V6_FLAG 0x60
#define TYPE_UDP 0x11

static bool imsbr_packet_is_ike_auth(unsigned char *ptr, unsigned int len)
{
	unsigned int ub_begin, ub_end, i;
	unsigned char pkt_type;

	/*IKE authentication packeet has 00 00 00 00 next to UDP header*/
	if ((ptr[0] & IP_V_FLAG) == 0x40) {
		ub_begin = 28;
		ub_end = 32;
		pkt_type = ptr[9];
		if (ptr[20] == 0x01 && ptr[21] == 0xf4) {
			pr_info("this is ike packet DO SA INIT!");
			return true;
		}
	} else if ((ptr[0] & IP_V6_FLAG) == 0x60) {
		ub_begin = 48;
		ub_end = 52;
		pkt_type = ptr[6];
	} else {
		pr_info("this is not ike packet!");
		return false;
	}

	if (pkt_type == TYPE_UDP) {
		for (i = ub_begin; i < ub_end; i++) {
			if (ptr[i] != 0x00)
				return false;
		}
		pr_info("this is ike packet !!!\n");
		return true;
	}
	pr_info("This is not ike packet and return false\n");
	return false;
}

static unsigned int nf_imsbr_input(void *priv,
				   struct sk_buff *skb,
				   const struct nf_hook_state *state)
{
	struct nf_conntrack_tuple nft;
	struct imsbr_flow *flow;
	struct iphdr *iph;
	struct udphdr *uh;
	struct ip_esp_hdr *esph;

	if (!atomic_read(&imsbr_enabled))
		return NF_ACCEPT;

	if (imsbr_get_tuple(state->net, skb, &nft))
		return NF_ACCEPT;

	imsbr_nfct_debug("input", skb, &nft);

	iph = ip_hdr(skb);
	if (iph && (iph->version == 4 && iph->protocol == IPPROTO_UDP)) {
		uh = udp_hdr(skb);
		if (uh && ntohs(uh->source) == ESP_PORT) {
			esph = (struct ip_esp_hdr *)((char *)uh + 8);
			if (imsbr_spi_match(ntohl(esph->spi))) {
				pr_info("spi 0x%x matched, update seq to %d\n",
					ntohl(esph->spi), ntohl(esph->seq_no));
				imsbr_modify_esp_seq(ntohl(esph->spi),
						     ntohl(esph->seq_no));
			}
		}
	}

	/* rcu_read_lock hold by netfilter hook outside */
	flow = imsbr_flow_match(&nft);
	if (!flow)
		return NF_ACCEPT;

	/*for cp vowifi */
	if (flow->socket_type == IMSBR_SOCKET_CP &&
	    flow->media_type == IMSBR_MEDIA_IKE) {
		if (imsbr_packet_is_ike_auth(skb->data, skb->len)) {
			imsbr_packet_relay2cp(skb);
			pr_info("this is ike pkt, go to cp socket !");
			return NF_STOLEN;
		}
		return NF_ACCEPT;//now is esp packet
	}

	/* c2k: downlink ike pkt always go to ap socket */
	if (flow->media_type == IMSBR_MEDIA_IKE) {
		pr_info("input skb=%p is ike pkt, go to ap socket\n", skb);
		return NF_ACCEPT;
	}
	/* Considering handover, link_type maybe LINK_AP or LINK_CP, meanwhile
	 * socket_type maybe SOCKET_AP or SOCKET_CP.
	 *
	 * But in INPUT hook, we can ignore the link_type and do the decision
	 * based on socket_type.
	 */
	if (flow->socket_type == IMSBR_SOCKET_CP) {
		pr_debug("input skb=%p relay to cp\n", skb);
		imsbr_packet_relay2cp(skb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static unsigned int nf_imsbr_output(void *priv,
				    struct sk_buff *skb,
				    const struct nf_hook_state *state)
{
	struct nf_conntrack_tuple nft;
	struct imsbr_flow *flow;
	int sim, link_type;
	struct iphdr *iph;
	struct udphdr *uh;
	struct ip_esp_hdr *esph;

	if (!atomic_read(&imsbr_enabled))
		return NF_ACCEPT;

	if (imsbr_get_tuple(state->net, skb, &nft))
		return NF_ACCEPT;

	imsbr_nfct_debug("output", skb, &nft);

	iph = ip_hdr(skb);
	if (iph && (iph->version == 4 && iph->protocol == IPPROTO_UDP)) {
		uh = udp_hdr(skb);
		if (uh && ntohs(uh->dest) == ESP_PORT) {
			esph = (struct ip_esp_hdr *)((char *)uh +
				sizeof(struct udphdr));
			if (imsbr_spi_match(ntohl(esph->spi))) {
				pr_info("output spi %x matched, update seq to %d\n",
					ntohl(esph->spi), ntohl(esph->seq_no));
				imsbr_modify_esp_seq(ntohl(esph->spi),
						     ntohl(esph->seq_no));
			}
		}
	}

	/* rcu_read_lock hold by netfilter hook outside */
	flow = imsbr_flow_match(&nft);
	if (!flow)
		return NF_ACCEPT;

	/* c2k: downlink ike pkt always go to ap socket */
	if (flow->media_type == IMSBR_MEDIA_IKE) {
		pr_info("output skb=%p is ike pkt, go to wlan0\n", skb);
		return NF_ACCEPT;
	}

	/* This means the packet was relayed from CP, so there's no need to
	 * have a further check!
	 */
	if (flow->socket_type == IMSBR_SOCKET_CP)
		return NF_ACCEPT;

	link_type = flow->link_type;
	sim = flow->sim_card;

	/* We assume that WIFI is at AP and LTE is at CP, but this may be
	 * broken in the future...
	 */
	if ((link_type == IMSBR_LINK_CP && !imsbr_in_lte2wifi(sim)) ||
	    (link_type == IMSBR_LINK_AP && imsbr_in_wifi2lte(sim))) {
		pr_debug("output skb=%p relay to cp\n", skb);
		/* Complete checksum manually if hw checksum offload is on */
		if (skb->ip_summed == CHECKSUM_PARTIAL)
			skb_checksum_help(skb);
		imsbr_packet_relay2cp(skb);
		return NF_STOLEN;
	}

	if (cur_lp_state == IMSBR_LOWPOWER_START) {
		pr_info("lowpower output skb=%p relay to cp\n", skb);
		imsbr_packet_relay2cp(skb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static struct nf_hook_ops nf_imsbr_ops[] __read_mostly = {
	{
		.hook		= nf_imsbr_input,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER - 1,
	},
	{
		.hook		= nf_imsbr_output,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP_PRI_FILTER - 1,
	},
	{
		.hook		= nf_imsbr_input,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP6_PRI_FILTER - 1,
	},
	{
		.hook		= nf_imsbr_output,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP6_PRI_FILTER - 1,
	},
};

static int __net_init imsbr_nf_register(struct net *net)
{
	return nf_register_net_hooks(net, nf_imsbr_ops,
				    ARRAY_SIZE(nf_imsbr_ops));
}

static void __net_init imsbr_nf_unregister(struct net *net)
{
	nf_unregister_net_hooks(net, nf_imsbr_ops, ARRAY_SIZE(nf_imsbr_ops));
}

static struct pernet_operations imsbr_net_ops = {
	.init = imsbr_nf_register,
	.exit = imsbr_nf_unregister,
};

int __init imsbr_hooks_init(void)
{
	int err;

	pr_debug("Registering netfilter hooks\n");

	err = register_pernet_subsys(&imsbr_net_ops);
	if (err)
		pr_err("nf_register_hooks: err %d\n", err);

	return 0;
}

void imsbr_hooks_exit(void)
{
	pr_debug("Unregistering netfilter hooks\n");

	unregister_pernet_subsys(&imsbr_net_ops);
}
