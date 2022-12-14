/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * PDX-License-Identifier: GPL-2.0+
 *
 * signed-off-by: Junjie.Wang <junjie.wang@spreadtrum.com>
 */

#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include <net/xfrm.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

extern struct list_head ptype_all __read_mostly;

static int xfrm6_dec_tcpdump_opt = 1;

static bool get_vowifi_dec_status(void)
{
	return xfrm6_dec_tcpdump_opt ? true : false;
}

static inline int deliver_skb(struct sk_buff *skb,
			      struct packet_type *pt_prev,
			      struct net_device *orig_dev)
{
	if (unlikely(skb_orphan_frags(skb, GFP_ATOMIC)))
		return -ENOMEM;
	refcount_inc(&skb->users);
	return pt_prev->func(skb, skb->dev, pt_prev, orig_dev);
}

static void nf_xfrm6_output_decode_cap_log(struct sk_buff *skb,
					   struct net_device *out_dev)
{
	struct net_device *orig_dev;
	struct net_device *pseudo_dev = NULL;
	struct sk_buff *copy_skb;
	struct net *net;

	copy_skb = pskb_copy(skb, GFP_ATOMIC);
	if (!copy_skb) {
		pr_err("%s: pskb copy failed,return!\n", __func__);
		return;
	}

	/* Skb->mac_headr can not be zero,
	 * otherwise it will crash in fun eth_header_parse.
	 */
	skb_set_mac_header(copy_skb, 0);
	skb_reset_mac_len(copy_skb);
	orig_dev = out_dev;
	skb_reset_network_header(copy_skb);

	if (!orig_dev && skb->sk) {
		pr_err("no netdevice found in skb and check lo device.\n");
		net = sock_net(skb->sk);
		if (net) {
			pseudo_dev  = dev_get_by_name(net, "dummy0");

			if (!pseudo_dev) {
				pr_err("no dummy0 netdevice found.\n");
				goto free_clone;
			}
			copy_skb->dev = pseudo_dev;
		} else {
			pr_err("no net found.\n");
			goto free_clone;
		}
	} else {
		pr_err("skb has no sk,so cannot dump\n");
		goto free_clone;
	}

	copy_skb->protocol = htons(ETH_P_IPV6);
	copy_skb->transport_header = copy_skb->network_header;
	copy_skb->pkt_type = PACKET_OUTGOING;
	dev_queue_xmit(copy_skb);

	return;
free_clone:
	/* Free clone skb. */
	kfree_skb(copy_skb);
}

static void *skb_ext_get_ptr2(struct skb_ext *ext, enum skb_ext_id id)
{
	return (void *)ext + (ext->offset[id] * 8);
}

static void nf_xfrm6_input_decode_cap_log(struct sk_buff *skb)
{
	struct net_device *pseudo_dev;
	struct sk_buff *copy_skb;
	struct xfrm_state *x;
	int cr_xfrm_depth;
	struct sec_path *sp;
	struct skb_ext *ext = skb->extensions;

	sp = skb_ext_get_ptr2(ext, SKB_EXT_SEC_PATH);
	if (!sp)
		return;

	/* If current is tunnel mode, no need to dump again. */
	cr_xfrm_depth = sp->len - 1;
	if (unlikely(cr_xfrm_depth < 0))
		return;

	x = sp->xvec[cr_xfrm_depth];
	if (x && x->props.mode == XFRM_MODE_TUNNEL)
		return;

	copy_skb = skb_clone(skb, GFP_ATOMIC);

	if (!copy_skb) {
		pr_err("clone failed,return!\n");
		return;
	}

	pseudo_dev  = dev_get_by_name(&init_net, "dummy0");
	if (!pseudo_dev) {
		pr_err("no dummy0 netdevice found.\n");
		goto free_clone1;
	}
	copy_skb->dev = pseudo_dev;
	dev_queue_xmit(copy_skb);
	return;

free_clone1:
	/* Free clone skb. */
	kfree_skb(copy_skb);
}

static unsigned int nf_ipv6_ipsec_dec_dump_in(void *priv,
					      struct sk_buff *skb,
					      const struct nf_hook_state *state)
{
	struct skb_ext *ext = skb->extensions;

	/* Check whether cap flag is zero. */
	if (!get_vowifi_dec_status())
		return NF_ACCEPT;

	if ((skb->active_extensions & (1 << SKB_EXT_SEC_PATH)) && ext)
		nf_xfrm6_input_decode_cap_log(skb);

	return NF_ACCEPT;
}

static unsigned int
nf_ipv6_ipsec_dec_dump_out(void *priv,
			   struct sk_buff *skb,
			   const struct nf_hook_state *state)
{
	/* Check whether cap flag is zero. */
	if (!get_vowifi_dec_status())
		return NF_ACCEPT;

	if (skb_dst(skb) && skb_dst(skb)->xfrm && state->out)
		nf_xfrm6_output_decode_cap_log(skb, state->out);

	return NF_ACCEPT;
}

static struct nf_hook_ops nf_ipv6_ipsec_dec_dump_ops[] __read_mostly = {
	/* Before do encryption, save the pkt. */
	{
	    .hook	= nf_ipv6_ipsec_dec_dump_out,
	    .pf	        = NFPROTO_IPV6,
	    .hooknum	= NF_INET_LOCAL_OUT,
	    .priority	= NF_IP6_PRI_LAST,
	},
	/* Check whether pkt is the decrypted one,and save it. */
	{
	    .hook	= nf_ipv6_ipsec_dec_dump_in,
	    .pf	        = NFPROTO_IPV6,
	    .hooknum    = NF_INET_LOCAL_IN,
	    .priority   = NF_IP6_PRI_LAST,
	},
};

static ssize_t tcpdump_opt_proc_write(struct file *file,
				      const char __user *buffer,
				      size_t count,
				      loff_t *pos)
{
	char mode;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;

		xfrm6_dec_tcpdump_opt = (mode != '0');
	}

	return count;
}

static int tcpdump_ct_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, xfrm6_dec_tcpdump_opt ? "1\n" : "0\n");
	return 0;
}

static int tcpdump_opt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tcpdump_ct_proc_show, NULL);
}

#ifdef CONFIG_PROC_FS
static const struct file_operations xfrm6_dec_tcpdump_fops = {
	.open  = tcpdump_opt_proc_open,
	.read  = seq_read,
	.write  = tcpdump_opt_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};
#endif

static int __net_init nf6_vowifi_dec_init(struct net *net)
{
	int err = 0;
	umode_t mode = 0664;

	err = nf_register_net_hooks(net, nf_ipv6_ipsec_dec_dump_ops,
				    ARRAY_SIZE(nf_ipv6_ipsec_dec_dump_ops));
	if (err < 0) {
		pr_err("nf_register_hooks 6_ipsec failed (%d).\n", err);
		return err;
	}
#ifdef CONFIG_PROC_FS
	if (!proc_create("nf_xfrm_dec_tcpdump6", mode,
			 net->nf.proc_netfilter,
			 &xfrm6_dec_tcpdump_fops)) {
		nf_unregister_net_hooks(net, nf_ipv6_ipsec_dec_dump_ops,
					ARRAY_SIZE(nf_ipv6_ipsec_dec_dump_ops));
		return -ENOMEM;
	}
#endif
	return err;
}

static void nf6_vowifi_dec_exit(struct net *net)
{
#ifdef CONFIG_PROC_FS
	remove_proc_entry("nf_xfrm_dec_tcpdump6", net->nf.proc_netfilter);
#endif
	nf_unregister_net_hooks(net, nf_ipv6_ipsec_dec_dump_ops,
				ARRAY_SIZE(nf_ipv6_ipsec_dec_dump_ops));
}

static struct pernet_operations nf6_net_xfrm_dec_ops = {
	.init = nf6_vowifi_dec_init,
	.exit = nf6_vowifi_dec_exit,
};

static int __init ip6table_vowifi_ipsec_dec_dump_init(void)
{
	return register_pernet_subsys(&nf6_net_xfrm_dec_ops);
}

static void __exit ip6table_vowifi_ipsec_dec_dump_exit(void)
{
	unregister_pernet_subsys(&nf6_net_xfrm_dec_ops);
}

module_init(ip6table_vowifi_ipsec_dec_dump_init);
module_exit(ip6table_vowifi_ipsec_dec_dump_exit);
MODULE_LICENSE("GPL v2");
