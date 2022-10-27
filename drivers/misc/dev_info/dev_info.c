#include "../../../../fs/proc/internal.h"
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/device_info.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVINFO_NAME "devinfo"
/**for definfo log**/
#define log_fmt(fmt) "[line:%d][module:%s][%s] " fmt

#define DEVINFO_ERR(a, arg...)                                           \
	do {                                                                 \
		printk(KERN_NOTICE log_fmt(a), __LINE__, DEVINFO_NAME, __func__, \
			   ##arg);                                                   \
	} while (0)

#define DEVINFO_MSG(a, arg...)                                                 \
	do {                                                                       \
		printk(KERN_INFO log_fmt(a), __LINE__, DEVINFO_NAME, __func__, ##arg); \
	} while (0)

/**definfo log end**/
static char devinfo_prj_name[10] = "unknow";

static struct of_device_id devinfo_id[] = {
	{
		.compatible = "devices,dev_info",
	},
	{},
};

struct devinfo_data {
	struct platform_device *devinfo;
	struct pinctrl *pinctrl;
	int main_board_id;
	int sub_board_id;
	int sdcard_gpio;
	struct manufacture_info sub_mainboard_info;
};

static struct devinfo_data *dev_info;
static struct proc_dir_entry *parent = NULL;

static int op_prj_name_setup(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *temp_name;
	int rc = 0;

	cmdline_node = of_find_node_by_path("/chosen");
	rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (!rc) {
		temp_name = strstr(cmd_line, "prj_name=");
		if (temp_name) {
			sscanf(temp_name, "prj_name=%s", devinfo_prj_name);
			pr_err("%s: prj_name=%s", __func__, devinfo_prj_name);
		} else {
			pr_err("%s: prj_name read error", __func__);
		}
	}

	return rc;
}

static void *device_seq_start(struct seq_file *s, loff_t *pos)
{
	static unsigned long counter = 0;
	if (*pos == 0) {
		return &counter;
	} else {
		*pos = 0;
		return NULL;
	}
}

static void *device_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void device_seq_stop(struct seq_file *s, void *v) { return; }

static int device_seq_show(struct seq_file *s, void *v)
{
	struct proc_dir_entry *pde = s->private;
	struct manufacture_info *info = pde->data;
	if (info) {
		seq_printf(s, "Device version:\t\t%s\nDevice manufacture:\t\t%s\n",
				   info->version, info->manufacture);
		if (info->fw_path)
			seq_printf(s, "Device fw_path:\t\t%s\n", info->fw_path);
	}
	return 0;
}

static struct seq_operations device_seq_ops = {.start = device_seq_start,
											   .next = device_seq_next,
											   .stop = device_seq_stop,
											   .show = device_seq_show};

static int device_proc_open(struct inode *inode, struct file *file)
{
	int ret = seq_open(file, &device_seq_ops);
	pr_err("%s is called\n", __func__);

	if (!ret) {
		struct seq_file *sf = file->private_data;
		sf->private = PDE(inode);
	}

	return ret;
}

static const struct file_operations device_node_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.open = device_proc_open,
	.owner = THIS_MODULE,
};

int register_device_proc(char *name, char *version, char *manufacture)
{
	struct proc_dir_entry *d_entry;
	struct manufacture_info *info;

	if (!parent) {
		parent = proc_mkdir("devinfo", NULL);
		if (!parent) {
			pr_err("can't create devinfo proc\n");
			return -ENOENT;
		}
	}

	info = kzalloc(sizeof *info, GFP_KERNEL);
	info->version = version;
	info->manufacture = manufacture;
	d_entry = proc_create_data(name, S_IRUGO, parent, &device_node_fops, info);
	if (!d_entry) {
		DEVINFO_ERR("create %s proc failed.\n", name);
		kfree(info);
		return -ENOENT;
	}
	return 0;
}

int register_devinfo(char *name, struct manufacture_info *info)
{
	struct proc_dir_entry *d_entry;
	if (!parent) {
		parent = proc_mkdir("devinfo", NULL);
		if (!parent) {
			pr_err("can't create devinfo proc\n");
			return -ENOENT;
		}
	}

	d_entry = proc_create_data(name, S_IRUGO, parent, &device_node_fops, info);
	if (!d_entry) {
		pr_err("create %s proc failed.\n", name);
		return -ENOENT;
	}
	return 0;
}

static int mainboard_init(void)
{
	return register_device_proc("mainboard", "SPRD", devinfo_prj_name);
}

static int subboard_init(struct devinfo_data *const devinfo_data)
{
	int ret = 0;
	ret = register_device_proc("sub_mainboard", "SPRD", "sub-match");
	return ret;
}

static int devinfo_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct devinfo_data *const devinfo_data =
		devm_kzalloc(&pdev->dev, sizeof(struct devinfo_data), GFP_KERNEL);

	if (IS_ERR_OR_NULL(devinfo_data)) {
		printk("devinfo_data kzalloc failed\n");
		ret = -ENOMEM;
		return ret;
	}

	devinfo_data->devinfo = pdev;
	dev_info = devinfo_data;
	
	ret = op_prj_name_setup();
	if (ret < 0) {
		pr_err("%s: cmdline prj_name read error\n", __func__);
	}
	
	if (!parent) {
		parent = proc_mkdir("devinfo", NULL);
		if (!parent) {
			printk("can't create devinfo proc\n");
			ret = -ENOENT;
		}
	}
	
	ret = register_device_proc("audio_mainboard", "SPRD", "sub-match");
	if (ret < 0) {
		DEVINFO_ERR("register audio_mainboard failed\n");
	}
	
	ret = mainboard_init();
	if (ret < 0) {
		DEVINFO_ERR("register mainboard failed\n");
	}

	ret = subboard_init(devinfo_data);
	if (ret < 0) {
		DEVINFO_ERR("register subboard failed\n");
	}
		
	return ret;
}

static int devinfo_remove(struct platform_device *dev)
{
	remove_proc_entry(DEVINFO_NAME, NULL);
	return 0;
}

static struct platform_driver devinfo_platform_driver = {
	.probe = devinfo_probe,
	.remove = devinfo_remove,
	.driver =
		{
			.name = DEVINFO_NAME,
			.of_match_table = devinfo_id,
		},
};

module_platform_driver(devinfo_platform_driver);

MODULE_DESCRIPTION("devices dev info");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("lll<lll@vanyol.com>");
