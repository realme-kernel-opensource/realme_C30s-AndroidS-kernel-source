/*
 * SGM SGM4151x charger driver
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include <linux/power/sprd_battery_info.h>





#define __SGM41511_CHARGE_DEV__
//#define __SGM41512_CHARGE_DEV__


#define SGM4151x_REG_00				0x00
#define SGM4151x_REG_01				0x01
#define SGM4151x_REG_02				0x02
#define SGM4151x_REG_03				0x03
#define SGM4151x_REG_04				0x04
#define SGM4151x_REG_05				0x05
#define SGM4151x_REG_06				0x06
#define SGM4151x_REG_07				0x07
#define SGM4151x_REG_08				0x08
#define SGM4151x_REG_09				0x09
#define SGM4151x_REG_0A				0x0a
#define SGM4151x_REG_0B				0x0b
#define SGM4151x_REG_NUM			12

/* Register 0x00 */
#define REG00_ENHIZ_MASK			GENMASK(7, 7)
#define REG00_ENHIZ_SHIFT			7

#define REG00_IINLIM_MASK			GENMASK(4, 0)
#define REG00_IINLIM_SHIFT			0

/* Register 0x01*/
#define REG01_WDT_RESET_MASK			GENMASK(6, 6)
#define REG01_WDT_RESET_SHIFT			6
#define REG01_OTG_CONFIG_MASK			GENMASK(5, 5)
#define REG01_OTG_CONFIG_SHIFT			5
#define REG01_CHG_CONFIG_MASK			GENMASK(4, 4)
#define REG01_CHG_CONFIG_SHIFT			4
#define REG01_SYS_MINV_MASK			    GENMASK(3, 1)
#define REG01_SYS_MINV_SHIFT			1

/* Register 0x02*/
#define REG02_ICHG_MASK				GENMASK(5, 0)
#define REG02_ICHG_SHIFT			0

/* Register 0x03 */
#define REG03_IPRECHG_MASK			GENMASK(7, 4)
#define REG03_IPRECHG_SHIFT			4
#define REG03_ITERM_MASK			GENMASK(3, 0)
#define REG03_ITERM_SHIFT			0


/* Register 0x04*/
#define REG04_VREG_MASK				GENMASK(7, 3)
#define REG04_VREG_SHIFT			3
#define REG04_VRECHG_MASK			GENMASK(0, 0)
#define REG04_VRECHG_SHIFT			0

/* Register 0x05*/
#define REG05_EN_TERM_MASK			GENMASK(7, 7)
#define REG05_EN_TERM_SHIFT			7
#define REG05_WDT_MASK				GENMASK(5, 4)
#define REG05_WDT_SHIFT				4
#define REG05_EN_TIMER_MASK			GENMASK(3, 3)
#define REG05_EN_TIMER_SHIFT		3
#define REG05_CHG_TIMER_MASK		GENMASK(2, 2)
#define REG05_CHG_TIMER_SHIFT		2
#define REG05_TREG_MASK				GENMASK(1, 1)
#define REG05_TREG_SHIFT			1
#define REG05_JEITA_ISET_MASK		GENMASK(0, 0)
#define REG05_JEITA_ISET_SHIFT		0

/* Register 0x06*/
#define REG06_BOOSTV_MASK			GENMASK(5, 4)
#define REG06_BOOSTV_SHIFT			4
#define REG06_VINDPM_MASK		    GENMASK(3, 0)
#define REG06_VINDPM_SHIFT		    0

/* Register 0x07*/
#define REG07_TMR2X_EN_MASK			GENMASK(6, 6)
#define REG07_TMR2X_EN_SHIFT		6
#define REG07_BATFET_DIS_MASK		GENMASK(5, 5)
#define REG07_BATFET_DIS_SHIFT		5
#define REG07_JEITA_VSET_MASK		GENMASK(4, 4)
#define REG07_JEITA_VSET_SHIFT		4
#define REG07_BATFET_DLY_MASK		GENMASK(3, 3)
#define REG07_BATFET_DLY_SHIFT		3
#define REG07_BATFET_RST_EN_MASK	GENMASK(2, 2)
#define REG07_BATFET_RST_EN_SHIFT	2
/* Register 0x08*/
#define REG08_VBUS_STAT_MASK		GENMASK(7, 5)
#define REG08_VBUS_STAT_SHIFT		5
#define REG08_CHRG_STAT_MASK		GENMASK(4, 3)
#define REG08_CHRG_STAT_SHIFT		3
#define REG08_PG_STAT_MASK			GENMASK(2, 2)
#define REG08_PG_STAT_SHIFT			2
#define REG08_VSYS_STAT_MASK		GENMASK(0, 0)
#define REG08_VSYS_STAT_SHIFT		0

/* Register 0x09*/
#define REG09_FAULT_WDT_MASK			0x80
#define REG09_FAULT_WDT_SHIFT			7
#define REG09_FAULT_BOOST_MASK			0x40
#define REG09_FAULT_BOOST_SHIFT			6
#define REG09_FAULT_CHRG_MASK			0x30
#define REG09_FAULT_CHRG_SHIFT			4
#define REG09_FAULT_BAT_MASK			0x08
#define REG09_FAULT_BAT_SHIFT			3
#define REG09_FAULT_NTC_MASK			0x07
#define REG09_FAULT_NTC_SHIFT			0

/* Register 0x0A*/
#define REG0A_VBUS_GD_MASK			0x80
#define REG0A_VBUS_GD_SHIFT			7
#define REG0A_VDPM_STAT_MASK		GENMASK(6, 6)
#define REG0A_VDPM_STAT_SHIFT		6
#define REG0A_IDPM_STAT_MASK		GENMASK(5, 5)
#define REG0A_IDPM_STAT_SHIFT		5

/* Register 0x0B*/
#define REG0B_REG_RESET_MASK		0x80
#define REG0B_REG_RESET_SHIFT		7
#define REG0B_PN_MASK				GENMASK(6, 3)
#define REG0B_PN_SHIFT				3
#define REG0B_DEV_REV_MASK			GENMASK(1, 0)
#define REG0B_DEV_REV_SHIFT			0


#define REG00_HIZ_DISABLE			0
#define REG00_HIZ_ENABLE			1
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_STEP			100
#define REG00_IINLIM_MIN			100
#define REG00_IINLIM_MAX			3200

#define REG01_WDT_RESET				1
#define REG01_OTG_DISABLE			0
#define REG01_OTG_ENABLE			1
#define REG01_CHG_DISABLE			0
#define REG01_CHG_ENABLE			1
#define REG01_SYSV_OFFSET			2600
#define REG01_SYSV_MIN				2600
#define REG01_SYSV_MAX				3700
#define REG02_BOOSTV_LIM_500MA			0
#define REG02_BOOSTV_LIM_1200MA			1
#define REG02_ICHG_OFFSET			0
#define REG02_ICHG_STEP				60
#define REG02_ICHG_MIN				0
#define REG02_ICHG_MAX				3000

#define REG03_IPRECHG_OFFSET		60
#define REG03_IPRECHG_STEP			60
#define REG03_IPRECHG_MIN			60
#define REG03_IPRECHG_MAX			780
#define REG03_ITERM_OFFSET			60
#define REG03_ITERM_STEP			60
#define REG03_ITERM_MIN				60
#define REG03_ITERM_MAX				960


#define REG04_VREG_STEP				32

#ifdef __SGM41512_CHARGE_DEV__
#define REG04_VREG_MIN				3848
#define REG04_VREG_OFFSET			3848
#define REG04_VREG_MAX				4616
#elif defined(__SGM41511_CHARGE_DEV__)
#define REG04_VREG_MIN				3856
#define REG04_VREG_OFFSET			3856
#define REG04_VREG_MAX				4624
#endif

#define REG04_VRECHG_100MV			0
#define REG04_VRECHG_200MV			1

#define REG05_TERM_DISABLE			0
#define REG05_TERM_ENABLE			1
#define REG05_STAT_DIS_DISABLE			1
#define REG05_STAT_DIS_ENABLE			0
#define REG05_WDT_DISABLE			0
#define REG05_WDT_40S				1
#define REG05_WDT_80S				2
#define REG05_WDT_160S				3
#define REG05_CHG_TIMER_DISABLE			0
#define REG05_CHG_TIMER_ENABLE			1
#define REG05_CHG_TIMER_5HOURS			0
#define REG05_CHG_TIMER_10HOURS			1
#define REG05_TREG_80				0
#define REG05_TREG_120				1
#define REG05_JEITA_ISET_50PCT			0
#define REG05_JEITA_ISET_20PCT			1
#define REG06_BOOSTV_OFFSET			4850
#define REG06_BOOSTV_STEP			150
#define REG06_BOOSTV_MIN			4850
#define REG06_BOOSTV_MAX			5300
#define REG06_VINDPM_OFFSET			3900
#define REG06_VINDPM_STEP			100
#define REG06_VINDPM_MIN			3900
#define REG06_VINDPM_MAX			5400
#define REG07_TMR2X_EN_DISABLE			0
#define REG07_TMR2X_EN_ENABLE			1
#define REG07_BATFET_DIS_DISABLE		0
#define REG07_BATFET_DIS_ENABLE			1
#define REG07_JEITA_VSET_DISABLE		0
#define REG07_JEITA_VSET_ENABLE			1
#define REG07_BATFET_DLY_DISABLE		0
#define REG07_BATFET_DLY_ENABLE			1
#define REG07_BATFET_RST_EN_DISABLE		0
#define REG07_BATFET_RST_EN_ENABLE		1

#define REG08_VBUS_TYPE_NONE			0

#ifdef __SGM41512_CHARGE_DEV__
#define REG08_VBUS_TYPE_USB_SDP			1
#define REG08_VBUS_TYPE_USB_CDP			2
#define REG08_VBUS_TYPE_USB_DCP			3
#define REG08_VBUS_TYPE_DCP			4
#define REG08_VBUS_TYPE_UNKNOWN			5
#define REG08_VBUS_TYPE_ADAPTER			6
#endif

#define REG08_VBUS_TYPE_OTG			7


#define REG08_CHRG_STAT_IDLE			0
#define REG08_CHRG_STAT_PRECHG			1
#define REG08_CHRG_STAT_FASTCHG			2
#define REG08_CHRG_STAT_CHGDONE			3
#define REG08_POWER_NOT_GOOD			0
#define REG08_POWER_GOOD			1
#define REG08_NOT_IN_VSYS_STAT			0
#define REG08_IN_VSYS_STAT			1

#define REG09_FAULT_WDT				1
#define REG09_FAULT_BOOST			1
#define REG09_FAULT_CHRG_NORMAL			0
#define REG09_FAULT_CHRG_INPUT			1
#define REG09_FAULT_CHRG_THERMAL		2
#define REG09_FAULT_CHRG_TIMER			3
#define REG09_FAULT_BAT_OVP			1
#define REG09_FAULT_NTC_NORMAL			0
#define REG09_FAULT_NTC_WARM			2
#define REG09_FAULT_NTC_COOL			3
#define REG09_FAULT_NTC_COLD			5
#define REG09_FAULT_NTC_HOT			6

#define REG0A_VBUS_GD				1
#define REG0A_VDPM_STAT				1
#define REG0A_IDPM_STAT				1

#define REG0B_REG_RESET				1


/* Other Realted Definition*/
#define SGM4151x_BATTERY_NAME			"sc27xx-fgu"

#define BIT_DP_DM_BC_ENB			BIT(0)

#define SGM4151x_WDT_VALID_MS			50

#define SGM4151x_OTG_ALARM_TIMER_MS		15000
#define SGM4151x_OTG_VALID_MS			500
#define SGM4151x_OTG_RETRY_TIMES			10

#define SGM4151x_DISABLE_PIN_MASK		BIT(0)
#define SGM4151x_DISABLE_PIN_MASK_2721		BIT(15)

#define SGM4151x_FAST_CHG_VOL_MAX		10500000
#define SGM4151x_NORMAL_CHG_VOL_MAX		6500000

#define SGM4151x_WAKE_UP_MS			2000
#define SET_DELAY_CHECK_LIMIT_INPUT_CURRENT

extern int sc27xx_fgu_bat_id;
struct sgm4151x_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm4151x_dump_reg;
	struct device_attribute attr_sgm4151x_lookup_reg;
	struct device_attribute attr_sgm4151x_sel_reg_id;
	struct device_attribute attr_sgm4151x_reg_val;
	struct attribute *attrs[5];

	struct sgm4151x_charger_info *info;
};

struct power_supply_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
};
struct sgm4151x_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
	struct delayed_work check_limit_current_work;
	unsigned char check_limit_current_cnt;
	#endif
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 role;
	bool need_disable_Q1;
	int termination_cur;
	int vol_max_mv;
	u32 actual_limit_current;
	bool otg_enable;
	struct alarm otg_timer;
	struct sgm4151x_charger_sysfs *sysfs;
	int reg_id;
};

struct sgm4151x_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sgm4151x_charger_reg_tab reg_tab[SGM4151x_REG_NUM + 1] = {
	{0, SGM4151x_REG_00, "Setting Input Limit Current reg"},
	{1, SGM4151x_REG_01, "Related Function Enable reg"},
	{2, SGM4151x_REG_02, "Setting Charge Limit Current reg"},
	{3, SGM4151x_REG_03, "Setting Terminal Current reg"},
	{4, SGM4151x_REG_04, "Setting Charge Limit Voltage reg"},
	{5, SGM4151x_REG_05, "Related Function Config reg"},
	{6, SGM4151x_REG_06, "Setting input Limit Voltage reg"},
	{7, SGM4151x_REG_07, "Related Function Config reg"},
	{8, SGM4151x_REG_08, "Status reg"},
	{9, SGM4151x_REG_09, "Fault reg"},
	{10, SGM4151x_REG_0A, "Related Function Config reg"},
	{11, SGM4151x_REG_0B, "Setting rst reg"},	
	{12, 0, "null"},
};


extern int sprd_charger_parse_charger_id(void);
static int charger_id = 0xFF;
static bool ccali_mode = false;

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "androidboot.mode=cali") ||
	    strstr(cmd_line, "androidboot.mode=autotest"))
		ccali_mode = true;

	return 0;
}

static int sgm4151x_charger_set_limit_current(struct sgm4151x_charger_info *info,
					     u32 limit_cur);

static int sgm4151x_read(struct sgm4151x_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm4151x_write(struct sgm4151x_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm4151x_update_bits(struct sgm4151x_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm4151x_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm4151x_write(info, reg, v);
}

static void sgm4151x_charger_dump_register(struct sgm4151x_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SGM4151x_REG_NUM; i++) {
		ret = sgm4151x_read(info, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       reg_val);
			idx += len;
		}
	}

	dev_info(info->dev, "%s: %s", __func__, buf);
}

static bool sgm4151x_charger_is_bat_present(struct sgm4151x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM4151x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (!ret && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n", ret);

	return present;
}

static int sgm4151x_charger_is_fgu_present(struct sgm4151x_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM4151x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int sgm4151x_charger_set_vindpm(struct sgm4151x_charger_info *info, u32 vol)
{
	u8 reg_val;
	
dev_info(info->dev, "sgm4151x_charger_set_vindpm:%d\n", vol);
	if (vol < REG06_VINDPM_MIN)
		vol = REG06_VINDPM_MIN;
	else if (vol > REG06_VINDPM_MAX)
		vol = REG06_VINDPM_MAX;
	reg_val = (vol - REG06_VINDPM_OFFSET) / REG06_VINDPM_STEP;

	return sgm4151x_update_bits(info, SGM4151x_REG_06,
				   REG06_VINDPM_MASK, reg_val);
}

static int sgm4151x_charger_set_termina_vol(struct sgm4151x_charger_info *info, u32 vol)
{
	u8 reg_val;

    dev_info(info->dev, "sgm4151x_charger_set_termina_vol:%d\n", vol);
    //
	sgm4151x_update_bits(info, SGM4151x_REG_04, REG04_VRECHG_MASK,
				   REG04_VRECHG_200MV << REG04_VRECHG_SHIFT);
				   
	if (vol < REG04_VREG_MIN)
		vol = REG04_VREG_MIN;
	else if (vol >= REG04_VREG_MAX)
		vol = REG04_VREG_MAX;
	reg_val = (vol - REG04_VREG_OFFSET) / REG04_VREG_STEP;

	return sgm4151x_update_bits(info, SGM4151x_REG_04, REG04_VREG_MASK,
				   reg_val << REG04_VREG_SHIFT);
}

static int sgm4151x_charger_set_termina_cur(struct sgm4151x_charger_info *info, u32 cur)
{
	u8 reg_val;
	
    dev_info(info->dev, "sgm4151x_charger_set_termina_cur:%d\n", cur);
	if (cur <= REG03_ITERM_MIN)
		cur = REG03_ITERM_MIN;
	else if (cur >= REG03_ITERM_MAX)
		cur = REG03_ITERM_MAX;
	reg_val = (cur - REG03_ITERM_OFFSET) / REG03_ITERM_STEP;

	return sgm4151x_update_bits(info, SGM4151x_REG_03, REG03_ITERM_MASK, reg_val);
}

static int sgm4151x_enable_safetytimer(struct sgm4151x_charger_info *info,bool en)
{

	int ret = 0;
	if (en)
		ret = sgm4151x_update_bits(info, SGM4151x_REG_05,
		REG05_EN_TIMER_MASK, REG05_CHG_TIMER_ENABLE);
	else
		ret = sgm4151x_update_bits(info, SGM4151x_REG_05,
		REG05_EN_TIMER_MASK, REG05_CHG_TIMER_DISABLE);

	return ret;
}

int sgm4151x_set_prechrg_curr(struct sgm4151x_charger_info *info,int pre_current)
{
	int reg_val;
	int offset = REG03_IPRECHG_OFFSET;
	int ret = 0;

	if (pre_current < REG03_IPRECHG_MIN)
		pre_current = REG03_IPRECHG_MIN;
	else if (pre_current > REG03_IPRECHG_MAX)
		pre_current = REG03_IPRECHG_MAX;
	reg_val = (pre_current - offset) / REG03_IPRECHG_STEP;

	printk("chg_sgm4151x_set_prechrg_curr:%d\n",reg_val);

	ret = sgm4151x_update_bits(info, SGM4151x_REG_03, REG03_IPRECHG_MASK, reg_val << REG03_IPRECHG_SHIFT);
	if (ret)
		dev_err(info->dev, "set sgm4151x pre cur failed\n");

	return ret;
}
static int sgm4151x_charger_hw_init(struct sgm4151x_charger_info *info)
{
	struct sprd_battery_info bat_info;

	int ret;
	int num = 0;

	if (sc27xx_fgu_bat_id == 2)
		num = 1;
	
	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, num);
	if (ret) {
		dev_warn(info->dev, "sgm4151x_charger_hw_init:no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	} else {
	dev_warn(info->dev, "sgm4151x_charger_hw_init:have battery information is supplied\n");
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
		info->termination_cur = bat_info.charge_term_current_ua / 1000;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);

		ret = sgm4151x_update_bits(info, SGM4151x_REG_0B, REG0B_REG_RESET_MASK,
					  REG0B_REG_RESET << REG0B_REG_RESET_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset sgm4151x failed\n");
			return ret;
		}

		ret = sgm4151x_charger_set_vindpm(info, 4500);
		if (ret) {
			dev_err(info->dev, "set sgm4151x vindpm vol failed\n");
			return ret;
		}

		ret = sgm4151x_charger_set_termina_vol(info,
						      info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sgm4151x terminal vol failed\n");
			return ret;
		}

		ret = sgm4151x_charger_set_termina_cur(info, info->termination_cur);
		if (ret) {
			dev_err(info->dev, "set sgm4151x terminal cur failed\n");
			return ret;
		}

		ret = sgm4151x_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set sgm4151x limit current failed\n");
		ret = sgm4151x_enable_safetytimer(info,false);
		if (ret)
			dev_err(info->dev, "set sgm4151x disable safetytimer failed\n");

		ret = sgm4151x_set_prechrg_curr(info,300);
		if (ret)
			dev_err(info->dev, "set sgm4151x set prechrg failed\n");

	}

	return ret;
}

/*
static int sgm4151x_charger_get_charge_voltage(struct sgm4151x_charger_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM4151x_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM4151x_BATTERY_NAME\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}
*/

static int sgm4151x_charger_start_charge(struct sgm4151x_charger_info *info)
{
	int ret;

	dev_info(info->dev, "sgm4151x_charger_start_charge\n");
	ret = sgm4151x_update_bits(info, SGM4151x_REG_00,
				  REG00_ENHIZ_MASK, REG00_HIZ_DISABLE);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

//	ret = sgm4151x_update_bits(info, SGM4151x_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_40S << REG05_WDT_SHIFT);
    ret = sgm4151x_update_bits(info, SGM4151x_REG_05, REG05_WDT_MASK,
				  REG05_WDT_DISABLE);
				  
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm4151x watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable sgm4151x charge failed\n");
			return ret;
		}

	ret = sgm4151x_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sgm4151x_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sgm4151x terminal cur failed\n");

    dev_info(info->dev, "sgm4151x aquire charger wakelock\n");
    pm_stay_awake(info->dev);
	return ret;
}

static void sgm4151x_charger_stop_charge(struct sgm4151x_charger_info *info)
{
	int ret;
	
    dev_info(info->dev, "sgm4151x_charger_stop_charge sgm4151x release charger wakelock\n");
    pm_relax(info->dev);

	if (info->need_disable_Q1) {
		ret = sgm4151x_update_bits(info, SGM4151x_REG_00, REG00_ENHIZ_MASK,
					  REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		info->need_disable_Q1 = false;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable sgm4151x charge failed\n");

	ret = sgm4151x_update_bits(info, SGM4151x_REG_05, REG05_WDT_MASK,
				  REG05_WDT_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable sgm4151x watchdog\n");

}

static int sgm4151x_charger_set_current(struct sgm4151x_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	int ret;
	
dev_info(info->dev, "sgm4151x_charger_set_current :%d\n", cur);
	cur = cur / 1000;
	if (cur <= REG02_ICHG_MIN)
		cur = REG02_ICHG_MIN;
	else if (cur >= REG02_ICHG_MAX)
		cur = REG02_ICHG_MAX;
	reg_val = cur / REG02_ICHG_STEP;

	ret = sgm4151x_update_bits(info, SGM4151x_REG_02, REG02_ICHG_MASK, reg_val);

	return ret;
}

static int sgm4151x_charger_get_current(struct sgm4151x_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm4151x_read(info, SGM4151x_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	*cur = reg_val * REG02_ICHG_STEP * 1000;

dev_info(info->dev, "sgm4151x_charger_get_current :%d\n", *cur);
	return 0;
}

static int sgm4151x_charger_set_limit_current(struct sgm4151x_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;	
dev_info(info->dev, "sgm4151x_charger_set_limit_current :%d\n", limit_cur);

	limit_cur = limit_cur / 1000;
	if (limit_cur >= REG00_IINLIM_MAX)
		limit_cur = REG00_IINLIM_MAX;
	if (limit_cur <= REG00_IINLIM_MIN)
		limit_cur = REG00_IINLIM_MIN;

	info->last_limit_current = limit_cur * 1000;
	reg_val = (limit_cur - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;

	ret = sgm4151x_update_bits(info, SGM4151x_REG_00, REG00_IINLIM_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sgm4151x limit cur failed\n");

	info->actual_limit_current =
		(reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET) * 1000;

	return ret;
}

static u32 sgm4151x_charger_get_limit_current(struct sgm4151x_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sgm4151x_read(info, SGM4151x_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	*limit_cur = reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET;
	if (*limit_cur >= REG00_IINLIM_MAX)
		*limit_cur = REG00_IINLIM_MAX * 1000;
	else
		*limit_cur = *limit_cur * 1000;
    dev_info(info->dev, "sgm4151x_charger_get_limit_current[%d] info->actual_limit_current[%d]\n", *limit_cur, info->actual_limit_current);

	return 0;
}

static inline int sgm4151x_charger_get_health(struct sgm4151x_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int sgm4151x_charger_get_online(struct sgm4151x_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int sgm4151x_charger_feed_watchdog(struct sgm4151x_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;
dev_err(info->dev, "sgm4151x_charger_feed_watchdog In..\n");

	ret = sgm4151x_update_bits(info, SGM4151x_REG_01, REG01_WDT_RESET_MASK,
				  REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm4151x failed\n");
		return ret;
	}

	ret = sgm4151x_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = sgm4151x_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}

static inline int sgm4151x_charger_get_status(struct sgm4151x_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sgm4151x_charger_set_status(struct sgm4151x_charger_info *info,
				      int val)
{
	int ret = 0;
//	u32 input_vol;

dev_err(info->dev, "sgm4151x_charger_set_status  val=%d\n", val);

	if (!val && info->charging) {
		dev_info(info->dev,"sgm4151x_charger_set_status set status stop charging");
		sgm4151x_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		dev_info(info->dev,"sgm4151x_charger_set_status set status start charging");
		ret = sgm4151x_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void sgm4151x_charger_work(struct work_struct *data)
{
	struct sgm4151x_charger_info *info =
		container_of(data, struct sgm4151x_charger_info, work);
	bool present;

	present = sgm4151x_charger_is_bat_present(info);


	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->limit)
		schedule_delayed_work(&info->wdt_work, 0);
	else
		cancel_delayed_work_sync(&info->wdt_work);

#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
    if (info->limit) {
		schedule_delayed_work(&info->check_limit_current_work, HZ * 2);
	}
		
	else {
		cancel_delayed_work_sync(&info->check_limit_current_work);
	}	
	info->check_limit_current_cnt = 0;
#endif	

	dev_info(info->dev, "sgm4151x_charger_work: battery present = %d, charger type = %d info->limit = %d\n",
		 present, info->usb_phy->chg_type, info->limit);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static ssize_t sgm4151x_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs =
		container_of(attr, struct sgm4151x_charger_sysfs,
			     attr_sgm4151x_reg_val);
	struct sgm4151x_charger_info *info = sgm4151x_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return sprintf(buf, "%s sgm4151x_sysfs->info is null\n", __func__);

	ret = sgm4151x_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get sgm4151x_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get sgm4151x_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "sgm4151x_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm4151x_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs =
		container_of(attr, struct sgm4151x_charger_sysfs,
			     attr_sgm4151x_reg_val);
	struct sgm4151x_charger_info *info = sgm4151x_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sgm4151x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm4151x_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm4151x_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs =
		container_of(attr, struct sgm4151x_charger_sysfs,
			     attr_sgm4151x_sel_reg_id);
	struct sgm4151x_charger_info *info = sgm4151x_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sgm4151x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sgm4151x_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM4151x_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sgm4151x_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sgm4151x_sysfs->name, id);
	return count;
}

static ssize_t sgm4151x_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs =
		container_of(attr, struct sgm4151x_charger_sysfs,
			     attr_sgm4151x_sel_reg_id);
	struct sgm4151x_charger_info *info = sgm4151x_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm4151x_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sgm4151x_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs =
		container_of(attr, struct sgm4151x_charger_sysfs,
			     attr_sgm4151x_lookup_reg);
	struct sgm4151x_charger_info *info = sgm4151x_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return sprintf(buf, "%s sgm4151x_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SGM4151x_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sgm4151x_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs =
		container_of(attr, struct sgm4151x_charger_sysfs,
			     attr_sgm4151x_dump_reg);
	struct sgm4151x_charger_info *info = sgm4151x_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm4151x_sysfs->info is null\n", __func__);

	sgm4151x_charger_dump_register(info);

	return sprintf(buf, "%s\n", sgm4151x_sysfs->name);
}

static int sgm4151x_register_sysfs(struct sgm4151x_charger_info *info)
{
	struct sgm4151x_charger_sysfs *sgm4151x_sysfs;
	int ret;

	sgm4151x_sysfs = devm_kzalloc(info->dev, sizeof(*sgm4151x_sysfs), GFP_KERNEL);
	if (!sgm4151x_sysfs)
		return -ENOMEM;

	info->sysfs = sgm4151x_sysfs;
	sgm4151x_sysfs->name = "sgm4151x_sysfs";
	sgm4151x_sysfs->info = info;
	sgm4151x_sysfs->attrs[0] = &sgm4151x_sysfs->attr_sgm4151x_dump_reg.attr;
	sgm4151x_sysfs->attrs[1] = &sgm4151x_sysfs->attr_sgm4151x_lookup_reg.attr;
	sgm4151x_sysfs->attrs[2] = &sgm4151x_sysfs->attr_sgm4151x_sel_reg_id.attr;
	sgm4151x_sysfs->attrs[3] = &sgm4151x_sysfs->attr_sgm4151x_reg_val.attr;
	sgm4151x_sysfs->attrs[4] = NULL;
	sgm4151x_sysfs->attr_g.name = "debug";
	sgm4151x_sysfs->attr_g.attrs = sgm4151x_sysfs->attrs;

	sysfs_attr_init(&sgm4151x_sysfs->attr_sgm4151x_dump_reg.attr);
	sgm4151x_sysfs->attr_sgm4151x_dump_reg.attr.name = "sgm4151x_dump_reg";
	sgm4151x_sysfs->attr_sgm4151x_dump_reg.attr.mode = 0444;
	sgm4151x_sysfs->attr_sgm4151x_dump_reg.show = sgm4151x_dump_reg_show;

	sysfs_attr_init(&sgm4151x_sysfs->attr_sgm4151x_lookup_reg.attr);
	sgm4151x_sysfs->attr_sgm4151x_lookup_reg.attr.name = "sgm4151x_lookup_reg";
	sgm4151x_sysfs->attr_sgm4151x_lookup_reg.attr.mode = 0444;
	sgm4151x_sysfs->attr_sgm4151x_lookup_reg.show = sgm4151x_reg_table_show;

	sysfs_attr_init(&sgm4151x_sysfs->attr_sgm4151x_sel_reg_id.attr);
	sgm4151x_sysfs->attr_sgm4151x_sel_reg_id.attr.name = "sgm4151x_sel_reg_id";
	sgm4151x_sysfs->attr_sgm4151x_sel_reg_id.attr.mode = 0644;
	sgm4151x_sysfs->attr_sgm4151x_sel_reg_id.show = sgm4151x_reg_id_show;
	sgm4151x_sysfs->attr_sgm4151x_sel_reg_id.store = sgm4151x_reg_id_store;

	sysfs_attr_init(&sgm4151x_sysfs->attr_sgm4151x_reg_val.attr);
	sgm4151x_sysfs->attr_sgm4151x_reg_val.attr.name = "sgm4151x_reg_val";
	sgm4151x_sysfs->attr_sgm4151x_reg_val.attr.mode = 0644;
	sgm4151x_sysfs->attr_sgm4151x_reg_val.show = sgm4151x_reg_val_show;
	sgm4151x_sysfs->attr_sgm4151x_reg_val.store = sgm4151x_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sgm4151x_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int sgm4151x_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sgm4151x_charger_info *info =
		container_of(nb, struct sgm4151x_charger_info, usb_notify);

	info->limit = limit;
dev_info(info->dev, "sgm4151x_charger_usb_change , limit = %d  info->role= %d\n", limit, info->role);
    if (ccali_mode == true)return NOTIFY_OK;

	pm_wakeup_event(info->dev, SGM4151x_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int sgm4151x_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sgm4151x_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = sgm4151x_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4151x_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4151x_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = sgm4151x_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4151x_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev, "get sgm4151x charge status failed\n");
			goto out;
		}

	//	val->intval = !enabled;
		val->intval = !(enabled & info->charger_pd_mask);
		
//		pr_err("sgm4151x_charger_usb_get_property POWER_SUPPLY_PROP_CALIBRATE enabled[%x] val->intval = %d\n", enabled, val->intval);
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4151x_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sgm4151x_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm4151x_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm4151x_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm4151x_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == true) {
			ret = sgm4151x_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm4151x_charger_stop_charge(info);
		}
		dev_info(info->dev, "POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
			 val->intval ? "enable" : "disable");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_DUMP_CHARGER_REGISTER_CMD) {
			sgm4151x_charger_dump_register(info);			
			break;
		}
		ret = sgm4151x_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
#if 0
	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = sgm4151x_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;
#endif
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4151x_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_TYPE:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sgm4151x_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property sgm4151x_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
};

static const struct power_supply_desc sgm4151x_charger_desc = {
	.name			= "sgm4151x_charger",
//	.name			= "v5000_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sgm4151x_usb_props,
	.num_properties		= ARRAY_SIZE(sgm4151x_usb_props),
	.get_property		= sgm4151x_charger_usb_get_property,
	.set_property		= sgm4151x_charger_usb_set_property,
	.property_is_writeable	= sgm4151x_charger_property_is_writeable,
	.usb_types		= sgm4151x_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sgm4151x_charger_usb_types),
};

static void sgm4151x_charger_detect_status(struct sgm4151x_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;

	/*
	 * slave no need to start charge when vbus change.
	 * due to charging in shut down will check each psy
	 * whether online or not, so let info->limit = min.
	 */
	if (ccali_mode == true)return;
	
	schedule_work(&info->work);
}

static void sgm4151x_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4151x_charger_info *info = container_of(dwork,
							 struct sgm4151x_charger_info,
							 wdt_work);
	int ret;

	ret = sgm4151x_update_bits(info, SGM4151x_REG_01, REG01_WDT_RESET_MASK,
				  REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm4151x failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
static void sgm4151x_delay_check_limit_current(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4151x_charger_info *info = container_of(dwork,
							 struct sgm4151x_charger_info,
							 check_limit_current_work);
	int ret;
	u32 limit_cur = 0;
	dev_info(info->dev, "%s:line%d enter cnt[%d]\n", __func__, __LINE__, info->check_limit_current_cnt);

	ret = sgm4151x_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		goto out;
	}

	if (limit_cur != info->actual_limit_current) {
		sgm4151x_charger_set_limit_current(info, info->actual_limit_current);
		dev_err(info->dev, "sgm4151x_delay_check_limit_current:reset limit!!!\n");
	}

out:	
	if (++ info->check_limit_current_cnt < 9)schedule_delayed_work(&info->check_limit_current_work, HZ * 2);
}
#endif	



#ifdef CONFIG_REGULATOR
static bool sgm4151x_charger_check_otg_valid(struct sgm4151x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm4151x_read(info, SGM4151x_REG_01, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4151x charger otg valid status failed\n");
		return status;
	}

	if (value & REG01_OTG_CONFIG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_3 = 0x%x\n", value);

	return status;
}

static bool sgm4151x_charger_check_otg_fault(struct sgm4151x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm4151x_read(info, SGM4151x_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4151x charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG09_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_0C = 0x%x\n",
			value);

	return status;
}

static void sgm4151x_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4151x_charger_info *info = container_of(dwork,
			struct sgm4151x_charger_info, otg_work);
	bool otg_valid = sgm4151x_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm4151x_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm4151x_update_bits(info, SGM4151x_REG_01,
						  REG01_OTG_CONFIG_MASK,
						  REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart sgm4151x charger otg failed\n");
		}

		otg_valid = sgm4151x_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM4151x_OTG_RETRY_TIMES);

	if (retry >= SGM4151x_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm4151x_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm4151x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = sgm4151x_update_bits(info, SGM4151x_REG_01, REG01_OTG_CONFIG_MASK,
				  REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);

	if (ret) {
		dev_err(info->dev, "enable sgm4151x otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM4151x_WDT_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM4151x_OTG_VALID_MS));

	return 0;
}

static int sgm4151x_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm4151x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sgm4151x_update_bits(info, SGM4151x_REG_01,
				  REG01_OTG_CONFIG_MASK, REG01_OTG_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable sgm4151x otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int sgm4151x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm4151x_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	ret = sgm4151x_read(info, SGM4151x_REG_01, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sgm4151x otg status\n");
		return ret;
	}

	val &= REG01_OTG_CONFIG_MASK;

	return val;
}

static const struct regulator_ops sgm4151x_charger_vbus_ops = {
	.enable = sgm4151x_charger_enable_otg,
	.disable = sgm4151x_charger_disable_otg,
	.is_enabled = sgm4151x_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm4151x_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm4151x_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sgm4151x_charger_register_vbus_regulator(struct sgm4151x_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sgm4151x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int sgm4151x_charger_register_vbus_regulator(struct sgm4151x_charger_info *info)
{
	return 0;
}
#endif

static int sgm4151x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm4151x_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	
	charger_id = sprd_charger_parse_charger_id();
	get_boot_mode();
	pr_err("%s:line%d: Probe In ...charger_id[%d] ccali_mode[%d]\n", __func__, __LINE__, charger_id, ccali_mode);
	/*
    // 1:   sy6974b
    // 2:   sgm41511
    // 3:   sgm41513
	// 4:   sgm41513D
	// 5:   rt9741
	// 6:   rt9741d
    */
	if (charger_id == 1) {
	}
	else if (charger_id == 2) {
	}
	else return -ENODEV;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* HS03 for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 start */
	 client->addr = 0x6B;
	/* HS03 for SR-SL6215-01-178 Import multi-charger driver patch of SPCSS00872701 by gaochao at 20210720 end */
	info->client = client;
	info->dev = dev;

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);
	INIT_WORK(&info->work, sgm4151x_charger_work);

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = sgm4151x_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	ret = sgm4151x_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
			goto err_psy_usb;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SGM4151x_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = SGM4151x_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &sgm4151x_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}

	ret = sgm4151x_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sgm4151x_charger_hw_init\n");
		goto err_mutex_lock;
	}

	sgm4151x_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = sgm4151x_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = sgm4151x_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	

//	ret = sgm4151x_update_bits(info, SGM4151x_REG_05, REG05_WDT_MASK,
//				  REG05_WDT_40S << REG05_WDT_SHIFT);
	ret = sgm4151x_update_bits(info, SGM4151x_REG_05, REG05_WDT_MASK,
				  REG05_WDT_DISABLE);			  
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm4151x watchdog\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->otg_work, sgm4151x_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  sgm4151x_charger_feed_watchdog_work);
	#ifdef SET_DELAY_CHECK_LIMIT_INPUT_CURRENT
	INIT_DELAYED_WORK(&info->check_limit_current_work,
			  sgm4151x_delay_check_limit_current);
	#endif		  
	sgm4151x_charger_detect_status(info);
	pr_info("[%s]line=%d: probe success ccali_mode[%d]\n", __FUNCTION__, __LINE__, ccali_mode);

	return 0;

err_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
err_mutex_lock:
	mutex_destroy(&info->lock);

	return ret;
}

static int sgm4151x_charger_remove(struct i2c_client *client)
{
	struct sgm4151x_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sgm4151x_charger_suspend(struct device *dev)
{
	struct sgm4151x_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = SGM4151x_OTG_ALARM_TIMER_MS;
	int ret;


	if (info->otg_enable || info->limit)
		sgm4151x_charger_feed_watchdog(info, 0);
	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);

	/* feed watchdog first before suspend */
	ret = sgm4151x_update_bits(info, SGM4151x_REG_01, REG01_WDT_RESET_MASK,
				  REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sgm4151x failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int sgm4151x_charger_resume(struct device *dev)
{
	struct sgm4151x_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = sgm4151x_update_bits(info, SGM4151x_REG_01, REG01_WDT_RESET_MASK,
				  REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sgm4151x failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops sgm4151x_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm4151x_charger_suspend,
				sgm4151x_charger_resume)
};

static const struct i2c_device_id sgm4151x_i2c_id[] = {
	{"sgm41511_chg", 0},
	{"sgm41512_chg", 0},
	{"sgm4151x_chg", 0},
	{}
};

static const struct of_device_id sgm4151x_charger_of_match[] = {
	{ .compatible = "sgm,sgm41511_chg", },
	{ .compatible = "sgm,sgm41512_chg", },
	{ .compatible = "sgm,sgm4151x_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm4151x_charger_of_match);

static struct i2c_driver sgm4151x_charger_driver = {
	.driver = {
		.name = "sgm4151x-charger",
		.of_match_table = sgm4151x_charger_of_match,
		.pm = &sgm4151x_charger_pm_ops,
	},
	.probe = sgm4151x_charger_probe,
	.remove = sgm4151x_charger_remove,
	.id_table = sgm4151x_i2c_id,
};

module_i2c_driver(sgm4151x_charger_driver);

MODULE_AUTHOR("qhq <Allen.Qin@sg-micro.com>");
MODULE_DESCRIPTION("SGM4151x Charger Driver");
MODULE_LICENSE("GPL");

