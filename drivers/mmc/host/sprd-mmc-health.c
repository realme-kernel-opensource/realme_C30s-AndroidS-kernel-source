/*
 * create in 2021/1/7.
 * create emmc node in  /proc/bootdevice
 */

#include <linux/mmc/sprd-mmc-health.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <trace/hooks/mmchealth.h>
#include <linux/workqueue.h>
#include "../core/core.h"
#include "../core/card.h"
#include "../core/host.h"
#include "../core/mmc_ops.h"
static int emmc_flag;
/*FORESEE qunlian*/
static unsigned int emmc_manfid[] = {0xd6, 0x32, 0x6b, 0x32, 0x9b, 0x9b};
static char *emmc_prod_name[] = {"A3A551", "MMC32G", "MMC32G", "MMC64G", "Y2P032", "Y2P064"};
struct __mmchealthdata mmchealthdata;
struct work_struct mmc_helalt_work;
struct mmc_card *card_data = NULL;
static bool is_create = false;

#define PROC_MODE 0444
#define FORESEE_32G_eMMC1 1 	//FORESEE 32G
#define Phison_32G_eMMC1 2 		//qunlian 32G
#define Phison_32G_eMMC2 3 		//qunlian 32G
#define Phison_64G_eMMC1 4 		//qunlian 64G
#define YMTC_EC110_32G_eMMC 5	//changjiangcunchu 32G
#define YMTC_EC110_64G_eMMC 6	//changjiangcunchu 64G

int set_emmc_mode(struct mmc_card *card)
{
  int i;
  unsigned int current_cid_manfid = card->cid.manfid;
  char *current_prod_name = &card->cid.prod_name[0];

  for (i = 0 ; i < sizeof(emmc_manfid)/sizeof(unsigned int); i++)
  {
      if ((current_cid_manfid == emmc_manfid[i]) && (!strncmp(current_prod_name, emmc_prod_name[i], 6)))
       return emmc_flag = i+1;  //emmc num
  }
  return 0;
}
static int mmc_send_health_data(struct mmc_card *card, struct mmc_host *host,
		u32 opcode, void *buf, unsigned len, u32 arg)
{
	struct mmc_request mrq = {};
	struct mmc_command cmd = {};
	struct mmc_data data = {};
	struct scatterlist sg;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = arg;

	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, buf, len);

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(host, &mrq);

	if (cmd.error) {
		pr_err("cmd%d, cmd error: %d\n", opcode, cmd.error);
		return cmd.error;
	}

	if (data.error) {
		pr_err("cmd%d, data error: %d\n", opcode, data.error);
		return data.error;
	}

	return 0;
}

/* FORESEE 32G eMMC */
static int mmc_get_health_data1(struct mmc_card *card)
{
	int err;
	u8 *health_data;

	if (!card)
		return -EINVAL;

	health_data = kzalloc(512, GFP_KERNEL);
	if (!health_data)
		return -ENOMEM;
    /*CMD 56*/
	err = mmc_send_health_data(card, card->host, MMC_GEN_CMD,
				health_data, 512, 0x200005F1);
	if (err)
		goto out;
    /*CMD 13*/
	err = mmc_send_status(card, NULL);
	if (err)
		goto out;

	set_mmchealth_data(health_data);
out:
	kfree(health_data);
	return err;
}

/* Phison(qunlian) 32G eMMC */
static int mmc_get_health_data2(struct mmc_card *card)
{
	int err;
	u8 *health_data;

	if (!card)
		return -EINVAL;

	health_data = kzalloc(512, GFP_KERNEL);
	if (!health_data)
		return -ENOMEM;
    /*CMD 56*/
	err = mmc_send_health_data(card, card->host, MMC_GEN_CMD,
				health_data, 512, 0x4B534BFB);
	if (err)
		goto out;
    /*CMD 13*/
	err = mmc_send_status(card, NULL);
	if (err)
		goto out;
	/*CMD 56*/
	err = mmc_send_health_data(card, card->host, MMC_GEN_CMD,
				health_data, 512, 0x0d);
	if (err)
		goto out;
    /*CMD 13*/
	err = mmc_send_status(card, NULL);
	if (err)
		goto out;

	set_mmchealth_data(health_data);
out:
	kfree(health_data);
	return err;
}

/* YMTC_EC110_eMMC */
static int mmc_get_health_data3(struct mmc_card *card)
{
	int err;
	u8 *health_data;

	if (!card)
		return -EINVAL;

	health_data = kzalloc(512, GFP_KERNEL);
	if (!health_data)
		return -ENOMEM;
    /*CMD 56*/
	err = mmc_send_health_data(card, card->host, MMC_GEN_CMD,
				health_data, 512, 0x594D54FB);
	if (err)
		goto out;
    /*CMD 13*/
	err = mmc_send_status(card, NULL);
	if (err)
		goto out;
    /*CMD 56*/
	err = mmc_send_health_data(card, card->host, MMC_GEN_CMD,
				health_data, 512, 0x29);
	if (err)
		goto out;
    /*CMD 13*/
	err = mmc_send_status(card, NULL);
	if (err)
		goto out;

	set_mmchealth_data(health_data);
out:
	kfree(health_data);
	return err;
}

/* api */
void sprd_mmc_health_flow(void *data, struct mmc_card *card)
{
	int flag = 0;

	card_data = card;
	if (!is_create) {
		/*获取cid,csd数据信息来判断颗粒*/
  		flag = set_emmc_mode(card);
  		sprd_create_mmc_health_init(flag);
		is_create = true;
	}

  	return ;
}

void set_mmchealth_data(u8 *data)
{
	memcpy(&mmchealthdata.buf[0], data, 512);
}

static int sprd_get_health_data_show(struct seq_file *m, void *v)
{
	printk("%s %d get mmc health data \n", __func__, __LINE__);
	mmc_claim_host(card_data->host);
	if (emmc_flag == FORESEE_32G_eMMC1)
		mmc_get_health_data1(card_data);
	else if (emmc_flag == Phison_32G_eMMC1 || emmc_flag == Phison_32G_eMMC2 || emmc_flag == Phison_64G_eMMC1)
		mmc_get_health_data2(card_data);
	else if (emmc_flag == YMTC_EC110_32G_eMMC || emmc_flag == YMTC_EC110_64G_eMMC)
		mmc_get_health_data3(card_data);
	mmc_release_host(card_data->host);

	return 0;
}

static int sprd_get_health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_get_health_data_show, inode->i_private);
}

static const struct file_operations get_health_data_fops = {
	.open = sprd_get_health_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*********************  FORESEE_32G_eMMC  *****************************************/
//Read reclaim tlc
static int sprd_Read_reclaim_tlc_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[51]  << 8) & 0xff00) |
				(mmchealthdata.buf[50] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Read_reclaim_tlc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Read_reclaim_tlc_show, inode->i_private);
}

static const struct file_operations Read_reclaim_tlc_fops = {
	.open = sprd_Read_reclaim_tlc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Factory bad block count
static int sprd_factory_bad_block_count_show(struct seq_file *m, void *v)
{
	u16 temp = mmchealthdata.buf[32] & 0xff;

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_factory_bad_block_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_factory_bad_block_count_show, inode->i_private);
}

static const struct file_operations factory_bad_block_count_fops = {
	.open = sprd_factory_bad_block_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Vcc drop count
static int sprd_vcc_drop_count_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[53]  << 8) & 0xff00) |
				(mmchealthdata.buf[52] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_vcc_drop_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_vcc_drop_count_show, inode->i_private);
}

static const struct file_operations vcc_drop_count_fops = {
	.open = sprd_vcc_drop_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Num_TLC
static int sprd_Min_EC_Num_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[15] << 24) & 0xff000000) |
			((mmchealthdata.buf[14] << 16) & 0xff0000) |
			((mmchealthdata.buf[13] << 8) & 0xff00) |
			(mmchealthdata.buf[12] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Num_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Num_TLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Num_TLC_fops = {
	.open = sprd_Min_EC_Num_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Num_TLC
static int sprd_Max_EC_Num_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[19] << 24) & 0xff000000) |
			((mmchealthdata.buf[18] << 16) & 0xff0000) |
			((mmchealthdata.buf[17] << 8) & 0xff00) |
			(mmchealthdata.buf[16] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Num_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Num_TLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Num_TLC_fops = {
	.open = sprd_Max_EC_Num_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Num_TLC
static int sprd_Ave_EC_Num_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[23] << 24) & 0xff000000) |
			((mmchealthdata.buf[22] << 16) & 0xff0000) |
			((mmchealthdata.buf[21] << 8) & 0xff00) |
			(mmchealthdata.buf[20] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Num_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Num_TLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Num_TLC_fops = {
	.open = sprd_Ave_EC_Num_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Num_SLC
static int sprd_Min_EC_Num_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[3] << 24) & 0xff000000) |
			((mmchealthdata.buf[2] << 16) & 0xff0000) |
			((mmchealthdata.buf[1] << 8) & 0xff00) |
			(mmchealthdata.buf[0] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Num_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Num_SLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Num_SLC_fops = {
	.open = sprd_Min_EC_Num_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Num_SLC
static int sprd_Max_EC_Num_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[7] << 24) & 0xff000000) |
			((mmchealthdata.buf[6] << 16) & 0xff0000) |
			((mmchealthdata.buf[5] << 8) & 0xff00) |
			(mmchealthdata.buf[4] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Num_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Num_SLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Num_SLC_fops = {
	.open = sprd_Max_EC_Num_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Num_SLC
static int sprd_Ave_EC_Num_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[11] << 24) & 0xff000000) |
			((mmchealthdata.buf[10] << 16) & 0xff0000) |
			((mmchealthdata.buf[9] << 8) & 0xff00) |
			(mmchealthdata.buf[8] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Num_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Num_SLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Num_SLC_fops = {
	.open = sprd_Ave_EC_Num_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total_write_size(MB)/100
static int sprd_Total_write_size_show(struct seq_file *m, void *v)
{
	u32 temp_L = 0;
	u64 temp_M = 0;
	u64 temp_T = 0;

	temp_M = ((mmchealthdata.buf[103] << 24) & 0xff000000) |
					((mmchealthdata.buf[102] << 16) & 0xff0000) |
					((mmchealthdata.buf[101] << 8) & 0xff00) |
					(mmchealthdata.buf[100] & 0xff);

	temp_L = ((mmchealthdata.buf[99] << 24) & 0xff000000) |
			((mmchealthdata.buf[98] << 16) & 0xff0000) |
			((mmchealthdata.buf[97] << 8) & 0xff00) |
			(mmchealthdata.buf[96] & 0xff);
			
	temp_T = ((temp_M << 32) | temp_L) * 512 / (1024 * 1024 * 100);

	seq_printf(m, "%d\n", temp_T);

	return 0;
}

static int sprd_Total_write_size_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Total_write_size_show, inode->i_private);
}

static const struct file_operations Total_write_size_fops = {
	.open = sprd_Total_write_size_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//FORESEE eMMC health data
static int sprd_health_data_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < 32; i++)
		seq_printf(m, "%02x", mmchealthdata.buf[i]);

	return 0;
}

static int sprd_health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_health_data_show, inode->i_private);
}

static const struct file_operations health_data_fops = {
	.open = sprd_health_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*****************************************************************/
static const struct file_operations *proc_fops_list1[] = {
	&Read_reclaim_tlc_fops,	
    	&factory_bad_block_count_fops,
	&vcc_drop_count_fops,
    	&Min_EC_Num_TLC_fops,
    	&Max_EC_Num_TLC_fops,
    	&Ave_EC_Num_TLC_fops,
    	&Min_EC_Num_SLC_fops,
    	&Max_EC_Num_SLC_fops,
    	&Ave_EC_Num_SLC_fops,
    	&Total_write_size_fops,
	&health_data_fops,
	&get_health_data_fops,
};

static char * const sprd_emmc_node_info1[] = {
	"Read_reclaim_tlc",
	"factory_bad_block_count",
	"vcc_drop_count",
    	"Min_EC_Num_TLC",
    	"Max_EC_Num_TLC",
    	"Ave_EC_Num_TLC",
    	"Min_EC_Num_SLC",
    	"Max_EC_Num_SLC",
    	"Ave_EC_Num_SLC",
    	"Total_write_size",
	"health_data",
	"get_health_data",
};

int sprd_create_mmc_health_init1(void)
{
	struct proc_dir_entry *mmchealthdir;
	struct proc_dir_entry *prEntry;
	int i, node;

	mmchealthdir = proc_mkdir("mmchealth", NULL);
	if (!mmchealthdir) {
		pr_err("%s: failed to create /proc/mmchealth\n",
			__func__);
		return -1;
	}

	node = ARRAY_SIZE(sprd_emmc_node_info1);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(sprd_emmc_node_info1[i], PROC_MODE,
				      mmchealthdir, proc_fops_list1[i]);
		if (!prEntry) {
			pr_err("%s,failed to create node: /proc/mmchealth/%s\n",
				__func__, sprd_emmc_node_info1[i]);
			return -1;
		}
	}

	return 0;
}


/*********************  Phison_32_64G_eMMC  *****************************************/
//Factory bad block count
static int sprd_bad_block_count_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[0] << 8 ) & 0xff00) |
				(mmchealthdata.buf[1] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_bad_block_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_bad_block_count_show, inode->i_private);
}

static const struct file_operations bad_block_count_fops = {
	.open = sprd_bad_block_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Runtime Bad Block
static int sprd_runtime_bad_block_count_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[2] << 8 ) & 0xff00) |
				(mmchealthdata.buf[3] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_runtime_bad_block_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_runtime_bad_block_count_show, inode->i_private);
}

static const struct file_operations runtime_bad_block_count_fops = {
	.open = sprd_runtime_bad_block_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//reserved block num
static int sprd_reserved_block_num_show(struct seq_file *m, void *v)
{
	u16 temp = ((mmchealthdata.buf[4] << 8 ) & 0xff00) |
				(mmchealthdata.buf[5] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_reserved_block_num_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_reserved_block_num_show, inode->i_private);
}

static const struct file_operations reserved_block_num_fops = {
	.open = sprd_reserved_block_num_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//read_reclaim_count
static int sprd_read_reclaim_count_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[87] << 24) & 0xff000000) |
			((mmchealthdata.buf[86] << 16) & 0xff0000) |
			((mmchealthdata.buf[85] << 8) & 0xff00) |
			(mmchealthdata.buf[84] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_read_reclaim_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_read_reclaim_count_show, inode->i_private);
}

static const struct file_operations read_reclaim_count_fops = {
	.open = sprd_read_reclaim_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Cnt_TLC
static int sprd_Min_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[16] << 24) & 0xff000000) |
			((mmchealthdata.buf[17] << 16) & 0xff0000) |
			((mmchealthdata.buf[18] << 8) & 0xff00) |
			(mmchealthdata.buf[19] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Cnt_TLC_fops = {
	.open = sprd_Min_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Cnt_TLC
static int sprd_Max_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[20] << 24) & 0xff000000) |
			((mmchealthdata.buf[21] << 16) & 0xff0000) |
			((mmchealthdata.buf[22] << 8) & 0xff00) |
			(mmchealthdata.buf[23] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Cnt_TLC_fops = {
	.open = sprd_Max_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Cnt_TLC
static int sprd_Ave_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[24] << 24) & 0xff000000) |
			((mmchealthdata.buf[25] << 16) & 0xff0000) |
			((mmchealthdata.buf[26] << 8) & 0xff00) |
			(mmchealthdata.buf[27] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Cnt_TLC_fops = {
	.open = sprd_Ave_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Tal_EC_Cnt_TLC
static int sprd_Tal_EC_Cnt_TLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[28] << 24) & 0xff000000) |
			((mmchealthdata.buf[29] << 16) & 0xff0000) |
			((mmchealthdata.buf[30] << 8) & 0xff00) |
			(mmchealthdata.buf[31] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Tal_EC_Cnt_TLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Tal_EC_Cnt_TLC_show, inode->i_private);
}

static const struct file_operations Tal_EC_Cnt_TLC_fops = {
	.open = sprd_Tal_EC_Cnt_TLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Cnt_SLC
static int sprd_Min_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[32] << 24) & 0xff000000) |
			((mmchealthdata.buf[33] << 16) & 0xff0000) |
			((mmchealthdata.buf[34] << 8) & 0xff00) |
			(mmchealthdata.buf[35] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Min_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Min_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Min_EC_Cnt_SLC_fops = {
	.open = sprd_Min_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Cnt_SLC
static int sprd_Max_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[36] << 24) & 0xff000000) |
			((mmchealthdata.buf[37] << 16) & 0xff0000) |
			((mmchealthdata.buf[38] << 8) & 0xff00) |
			(mmchealthdata.buf[39] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Max_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Max_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Max_EC_Cnt_SLC_fops = {
	.open = sprd_Max_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Cnt_SLC
static int sprd_Ave_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[40] << 24) & 0xff000000) |
			((mmchealthdata.buf[41] << 16) & 0xff0000) |
			((mmchealthdata.buf[42] << 8) & 0xff00) |
			(mmchealthdata.buf[43] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Ave_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Ave_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Ave_EC_Cnt_SLC_fops = {
	.open = sprd_Ave_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Tal_EC_Cnt_SLC
static int sprd_Tal_EC_Cnt_SLC_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[44] << 24) & 0xff000000) |
			((mmchealthdata.buf[45] << 16) & 0xff0000) |
			((mmchealthdata.buf[46] << 8) & 0xff00) |
			(mmchealthdata.buf[47] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Tal_EC_Cnt_SLC_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Tal_EC_Cnt_SLC_show, inode->i_private);
}

static const struct file_operations Tal_EC_Cnt_SLC_fops = {
	.open = sprd_Tal_EC_Cnt_SLC_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total write cnt
static int sprd_Tal_write_count_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[99] << 24) & 0xff000000) |
			((mmchealthdata.buf[98] << 16) & 0xff0000) |
			((mmchealthdata.buf[97] << 8) & 0xff00) |
			(mmchealthdata.buf[96] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_Tal_write_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_Tal_write_count_show, inode->i_private);
}

static const struct file_operations Tal_write_count_fops = {
	.open = sprd_Tal_write_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//power loss count
static int sprd_power_loss_count_show(struct seq_file *m, void *v)
{
	u32 temp = ((mmchealthdata.buf[195] << 24) & 0xff000000) |
			((mmchealthdata.buf[194] << 16) & 0xff0000) |
			((mmchealthdata.buf[193] << 8) & 0xff00) |
			(mmchealthdata.buf[192] & 0xff);

	seq_printf(m, "%d\n", temp);

	return 0;
}

static int sprd_power_loss_count_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_power_loss_count_show, inode->i_private);
}

static const struct file_operations power_loss_count_fops = {
	.open = sprd_power_loss_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Phison eMMC health data
static int sprd_Health_data_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < 32; i++)
		seq_printf(m, "%02x", mmchealthdata.buf[i]);

	return 0;
}

static int sprd_Health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_Health_data_show, inode->i_private);
}

static const struct file_operations Health_data_fops = {
	.open = sprd_Health_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations *proc_fops_list2[] = {
	&bad_block_count_fops,
	&runtime_bad_block_count_fops,
	&reserved_block_num_fops,
	&read_reclaim_count_fops,
	&Min_EC_Cnt_TLC_fops,
	&Max_EC_Cnt_TLC_fops,
    	&Ave_EC_Cnt_TLC_fops,
	&Tal_EC_Cnt_TLC_fops,
	&Min_EC_Cnt_SLC_fops,
    	&Max_EC_Cnt_SLC_fops,
    	&Ave_EC_Cnt_SLC_fops,
	&Tal_EC_Cnt_SLC_fops,
	&Tal_write_count_fops,
	&power_loss_count_fops,
	&Health_data_fops,
	&get_health_data_fops,
};

static char * const sprd_emmc_node_info2[] = {
	"factory_bad_block_count",
	"runtime_bad_block_cout",
	"reserved_blcok_num",
	"read_reclaim_count",
	"Min_EC_Num_TLC",
    	"Max_EC_Num_TLC",
    	"Ave_EC_Num_TLC",
	"Total_EC_Num_TLC",
	"Min_EC_Num_SLC",
    	"Max_EC_Num_SLC",
    	"Ave_EC_Num_SLC",
	"Total_EC_Num_SLC",
	"Total_write_size",
	"power_loss_count", 
	"health_data",
	"get_health_data",
};

int sprd_create_mmc_health_init2(void)
{
	struct proc_dir_entry *mmchealthdir;
	struct proc_dir_entry *prEntry;
	int i, node;

	mmchealthdir = proc_mkdir("mmchealth", NULL);
	if (!mmchealthdir) {
		pr_err("%s: failed to create /proc/mmchealth\n",
			__func__);
		return -1;
	}

	node = ARRAY_SIZE(sprd_emmc_node_info2);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(sprd_emmc_node_info2[i], PROC_MODE,
				      mmchealthdir, proc_fops_list2[i]);
		if (!prEntry) {
			pr_err("%s,failed to create node: /proc/mmchealth/%s\n",
				__func__, sprd_emmc_node_info2[i]);
			return -1;
		}
	}

	return 0;
}

/*********************  YMTC_EC110_eMMC  *****************************************/
//Factory bad block count
static int sprd_fbbc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[0]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int fbbc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_fbbc_show, inode->i_private);
}

static const struct file_operations fbbc_fops = {
	.open = fbbc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//TLC Reserved Block Num
static int sprd_trbn_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[4]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int trbn_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_trbn_show, inode->i_private);
}

static const struct file_operations trbn_fops = {
	.open = trbn_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//SLC Reserved Block Num
static int sprd_srbn_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[8]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int srbn_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_srbn_show, inode->i_private);
}

static const struct file_operations srbn_fops = {
	.open = srbn_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Run time bad block count (erase fail)
static int sprd_rtbbcef_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[20]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int rtbbcef_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_rtbbcef_show, inode->i_private);
}

static const struct file_operations rtbbcef_fops = {
	.open = rtbbcef_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Run time bad block count (program fail)
static int sprd_rtbbcpf_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[24]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int rtbbcpf_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_rtbbcpf_show, inode->i_private);
}

static const struct file_operations rtbbcpf_fops = {
	.open = rtbbcpf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Run time bad block count (read uecc)
static int sprd_rtbbcru_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[28]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int rtbbcru_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_rtbbcru_show, inode->i_private);
}

static const struct file_operations rtbbcru_fops = {
	.open = rtbbcru_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//UECC Count
static int sprd_uc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[32]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int uc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_uc_show, inode->i_private);
}

static const struct file_operations uc_fops = {
	.open = uc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Read reclaim SLC block count
static int sprd_rrsbc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[44]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int rrsbc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_rrsbc_show, inode->i_private);
}

static const struct file_operations rrsbc_fops = {
	.open = rrsbc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Read reclaim TLC block count
static int sprd_rrtbc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[48]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int rrtbc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_rrtbc_show, inode->i_private);
}

static const struct file_operations rrtbc_fops = {
	.open = rrtbc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//VDT drop count
static int sprd_vdc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[52]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int vdc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_vdc_show, inode->i_private);
}

static const struct file_operations vdc_fops = {
	.open = vdc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Sudden power off recovery success count
static int sprd_sporsc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[64]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int sporsc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_sporsc_show, inode->i_private);
}

static const struct file_operations sporsc_fops = {
	.open = sporsc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Sudden power off recovery fail count
static int sprd_sporfc_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[68]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int sporfc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_sporfc_show, inode->i_private);
}

static const struct file_operations sporfc_fops = {
	.open = sporfc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Num_SLC (EC: erase count)
static int sprd_minens_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[92]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int minens_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_minens_show, inode->i_private);
}

static const struct file_operations minens_fops = {
	.open = minens_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Num_SLC
static int sprd_maxens_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[96]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int maxens_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_maxens_show, inode->i_private);
}

static const struct file_operations maxens_fops = {
	.open = maxens_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Num_SLC
static int sprd_aens_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[100]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int aens_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_aens_show, inode->i_private);
}

static const struct file_operations aens_fops = {
	.open = aens_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Min_EC_Num_TLC
static int sprd_minent_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[104]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int minent_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_minent_show, inode->i_private);
}

static const struct file_operations minent_fops = {
	.open = minent_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Max_EC_Num_TLC
static int sprd_maxent_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[108]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int maxent_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_maxent_show, inode->i_private);
}

static const struct file_operations maxent_fops = {
	.open = maxent_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Ave_EC_Num_TLC
static int sprd_aent_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[112]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int aent_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_aent_show, inode->i_private);
}

static const struct file_operations aent_fops = {
	.open = aent_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total byte read(MB)
static int sprd_tbr_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[116]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int tbr_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_tbr_show, inode->i_private);
}

static const struct file_operations tbr_fops = {
	.open = tbr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total byte write(MB)
static int sprd_tbw_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[120]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int tbw_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_tbw_show, inode->i_private);
}

static const struct file_operations tbw_fops = {
	.open = tbw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//Total initialization count
static int sprd_tic_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[124]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int tic_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_tic_show, inode->i_private);
}

static const struct file_operations tic_fops = {
	.open = tic_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//SLC used life
static int sprd_sul_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[176]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int sul_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_sul_show, inode->i_private);
}

static const struct file_operations sul_fops = {
	.open = sul_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//TLC used life
static int sprd_tul_show(struct seq_file *m, void *v)
{
	u32 temp = *((u32 *)(&mmchealthdata.buf[180]));

	temp = be32_to_cpu(temp);

	seq_printf(m, "0x%x\n", temp);

	return 0;
}

static int tul_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, sprd_tul_show, inode->i_private);
}

static const struct file_operations tul_fops = {
	.open = tul_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**********************************************/
static const struct file_operations *proc_fops_list3[] = {
	&fbbc_fops,
	&trbn_fops,
	&srbn_fops,
	&rtbbcef_fops,
	&rtbbcpf_fops,
	&rtbbcru_fops,
	&uc_fops,
	&rrsbc_fops,
	&rrtbc_fops,
	&vdc_fops,
	&sporsc_fops,
	&sporfc_fops,
	&minens_fops,
	&maxens_fops,
	&aens_fops,
	&minent_fops,
	&maxent_fops,
	&aent_fops,
	&tbr_fops,
	&tbw_fops,
	&tic_fops,
	&sul_fops,
	&tul_fops,
	&health_data_fops,
	&get_health_data_fops,
};

static char * const sprd_emmc_node_info3[] = {
	"Factory_bad_block_count",
	"TLC_Reserved_Block_Num",
	"SLC_Reserved_Block_Num",
	"Run_time_bad_block_count(erase_fail)",
	"Run_time_bad_block_count(program_fail)",
	"Run_time_bad_block_count(read_UECC)",
	"UECC_Count",
	"Read_reclaim_SLC_block_count",
	"Read_reclaim_TLC_block_count",
	"VDT_drop_count",
	"Sudden_power_off_recovery_success_count",
	"Sudden_power_off_recovery_fail_count",
	"Min_EC_Num_SLC",
	"Max_EC_Num_SLC",
	"Ave_EC_Num_SLC",
	"Min_EC_Num_TLC",
	"Max_EC_Num_TLC",
	"Ave_EC_Num_TLC",
	"Total_byte_read(MB)",
	"Total_byte_write(MB)",
	"Total_initialization_count",
	"SLC_used_life",
	"TLC_used_life",
	"health_data",
	"get_health_data",
};

int sprd_create_mmc_health_init3(void)
{
	struct proc_dir_entry *mmchealthdir;
	struct proc_dir_entry *prEntry;
	int i, node;

	mmchealthdir = proc_mkdir("mmchealth", NULL);
	if (!mmchealthdir) {
		pr_err("%s: failed to create /proc/mmchealth\n",
			__func__);
		return -1;
	}

	node = ARRAY_SIZE(sprd_emmc_node_info3);
	for (i = 0; i < node; i++) {
		prEntry = proc_create(sprd_emmc_node_info3[i], PROC_MODE,
				      mmchealthdir, proc_fops_list3[i]);
		if (!prEntry) {
			pr_err("%s,failed to create node: /proc/mmchealth/%s\n",
				__func__, sprd_emmc_node_info3[i]);
			return -1;
		}
	}

	return 0;
}


/*  最终提供的接口  */
int sprd_create_mmc_health_init(int flag)
{
	int res = 0;

  	/* FORESEE 32G eMMc */
  	if (flag == FORESEE_32G_eMMC1)
		res = sprd_create_mmc_health_init1();
	/* Phison qunlian 32G eMMC */
	else if (flag == Phison_32G_eMMC1 || flag == Phison_32G_eMMC2 || flag == Phison_64G_eMMC1)
		res = sprd_create_mmc_health_init2();
	/* YMTC_EC110 32G eMMC */
	else if (flag == YMTC_EC110_32G_eMMC || flag == YMTC_EC110_64G_eMMC)
		res = sprd_create_mmc_health_init3();
	
	return res;
}

int storag_emmc_health_init(void)
{
	int ret;

	//INIT_WORK(&mmc_helalt_work, sprd_mmc_helalth_work);
 	//ret = register_trace_android_rvh_mmc_health(sprd_mmc_health_flow, NULL);
	ret = register_trace_android_rvh_mmc_health(sprd_mmc_health_flow, NULL);
  	if (ret)
  		pr_err("register_trace_android_vh_regmap_update fail, ret[%d]\n", ret);
  
  	return ret;
}

module_init(storag_emmc_health_init);
MODULE_LICENSE("GPL v2");
