/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "sprd_pdbg.h"
#include "sprd_pdbg_sc2730.h"

#define AP_INTC_PMIC_INDEX 173

struct power_debug_roc1 {
	struct power_debug *pdbg;
	struct platform_device *pdev;
	struct power_debug_cfg *pcfg;
};

static struct power_debug_roc1 pdbg_roc1;

static struct pdm_info roc1_pdm_info[] = {
	{
		.addr_offset = 0x0580,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_AI_SYS", "PD_AI_CAMBRICON",
			"PD_AI_POWERVR", "PD_IPA_SYS"}
	},
	{
		.addr_offset = 0x0584,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_AP_SYS", "PD_AP_VDSP",
			"PD_AP_VSP", "PD_ESE_SYS"}
	},
	{
		.addr_offset = 0x0588,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_GPU_TOP", "PD_GPU_RGX_DUST",
			"PD_MM_TOP"}
	},
	{
		.addr_offset = 0x058C,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_AUDCP_SYS", "PD_AUDCP_AUDDSP",
			"PD_PUBCP_SYS", "PD_PUB_SYS"}
	},
	{
		.addr_offset = 0x0590,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_WTLCP_SYS", "PD_WTLCP_LTE_PROC",
			"PD_WTLCP_LTE_CE", "PD_WTLCP_LTE_DPFEC"}
	},
	{
		.addr_offset = 0x0594,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, 16, 24, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_WTLCP_HU3GE_A", "PD_WTLCP_HU3GE_B",
			"PD_WTLCP_LDSP", "PD_WTLCP_TGDSP"}
	},
	{
		.addr_offset = 0x059C,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_APCPU_C0", "PD_APCPU_C1",
			"PD_APCPU_C2", "PD_APCPU_C3"}
	},
	{
		.addr_offset = 0x05A0,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_APCPU_C4", "PD_APCPU_C5",
			"PD_APCPU_C6", "PD_APCPU_C7"}
	},
	{
		.addr_offset = 0x05A4,
		.pwd_bit_width = 8,
		.bit_index = {0, 8, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"PD_APCPU_TOP", "PD_WTLCP_TD_PROC"}
	},
	{
		.addr_offset = 0x05B0,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"APCPU_CORE0_PWR_MODE", "APCPU_CORE1_PWR_MODE",
			"APCPU_CORE2_PWR_MODE", "APCPU_CORE3_PWR_MODE"}
	},
	{
		.addr_offset = 0x05B4,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"APCPU_CORE4_PWR_MODE", "APCPU_CORE5_PWR_MODE",
			"APCPU_CORE6_PWR_MODE", "APCPU_CORE7_PWR_MODE"}
	},
	{
		.addr_offset = 0x05B8,
		.pwd_bit_width = 8,
		.bit_index = {0, MAX_STATES_NUM_PER_REG},
		.pdm_name = {"APCPU_CORINTH_PWR_MODE"}
	}
};

static struct reg_check roc1_ap_ahb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xB7,
		.expect_value = 0x80,
		.preg_name = "REG_AP_AHB_AHB_EB"
	},
	{
		.addr_offset = 0x000C,
		.value_mask = 0x0A,
		.expect_value = 0x0A,
		.preg_name = "REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG"
	},
	{
		.addr_offset = 0x0060,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M0_LPC"
	},
	{
		.addr_offset = 0x0064,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M1_LPC"
	},
	{
		.addr_offset = 0x0068,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M2_LPC"
	},
	{
		.addr_offset = 0x006C,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M3_LPC"
	},
	{
		.addr_offset = 0x0070,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M4_LPC"
	},
	{
		.addr_offset = 0x0074,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M5_LPC"
	},
	{
		.addr_offset = 0x0078,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_M6_LPC"
	},
	{
		.addr_offset = 0x008C,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S0_LPC"
	},
	{
		.addr_offset = 0x0090,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S1_LPC"
	},
	{
		.addr_offset = 0x0094,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S2_LPC"
	},
	{
		.addr_offset = 0x0098,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S3_LPC"
	},
	{
		.addr_offset = 0x009C,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S4_LPC"
	},
	{
		.addr_offset = 0x00A0,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S5_LPC"
	},
	{
		.addr_offset = 0x00A4,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_S6_LPC"
	},
	{
		.addr_offset = 0x0124,
		.value_mask = 0x10000,
		.expect_value = 0x10000,
		.preg_name = "REG_AP_AHB_MERGE_M1_LPC"
	},
	{
		.addr_offset = 0x012C,
		.value_mask = 0x1,
		.expect_value = 0x1,
		.preg_name = "REG_AP_AHB_MERGE_S0_LPC"
	},
	{
		.addr_offset = 0x0050,
		.value_mask = 0x1,
		.expect_value = 0x1,
		.preg_name = "REG_AP_AHB_DISP_ASYNC_BRG"
	},
	{
		.addr_offset = 0x005C,
		.value_mask = 0x1,
		.expect_value = 0x1,
		.preg_name = "REG_AP_AHB_AP_ASYNC_BRG"
	}
};

static struct reg_check roc1_ap_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xC3C1FFEF,
		.expect_value = 0x00,
		.preg_name = "REG_AP_APB_APB_EB"
	},
	{
		.addr_offset = 0x000C,
		.value_mask = 0x01FF,
		.expect_value = 0x00,
		.preg_name = "REG_AP_APB_EB_SEC"
	},
	{
		.addr_offset = 0x0010,
		.value_mask = 0x000F,
		.expect_value = 0x00,
		.preg_name = "REG_AP_APB_APB_EB1"
	}
};

static struct reg_check roc1_pmu_apb_reg[] = {
	{
		.addr_offset = 0x00C8,
		.value_mask = 0xFFFFFFFF,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE"
	},
	{
		.addr_offset = 0x00CC,
		.value_mask = 0x7FFFFFFF,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_SLEEP_CTRL"
	},
	{
		.addr_offset = 0x00D4,
		.value_mask = 0x0FFFFFFF,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_SLEEP_STATUS"
	},
	{
		.addr_offset = 0x0230,
		.value_mask = 0x071F,
		.expect_value = 0x00,
		.preg_name = "REG_PMU_APB_LIGHT_SLEEP_ENABLE"
	},
	{
		.addr_offset = 0x05D4,
		.value_mask = 0x01,
		.expect_value = 0x01,
		.preg_name = "REG_PMU_APB_AXI_LP_CTRL_DISABLE"
	}
};

static struct reg_check roc1_aon_apb_reg[] = {
	{
		.addr_offset = 0x0000,
		.value_mask = 0xA0,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APB_EB0"
	},
	{
		.addr_offset = 0x0284,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_DEBUG_PWR_LP_CTRL"
	},
	{
		.addr_offset = 0x028C,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_CLUSTER_ATB_LPC_CTRL"
	},
	{
		.addr_offset = 0x0290,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_CLUSTER_APB_LPC_CTRL"
	},
	{
		.addr_offset = 0x0294,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_CLUSTER_GIC_LPC_CTRL"
	},
	{
		.addr_offset = 0x0298,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_GIC600_GIC_LPC_CTRL"
	},
	{
		.addr_offset = 0x029C,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_DBG_BLK_LPC_CTRL"
	},
	{
		.addr_offset = 0x0300,
		.value_mask = 0x01BD,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_TOP_MTX_M0_LPC_CTRL"
	},
	{
		.addr_offset = 0x0320,
		.value_mask = 0x01,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_CLUSTER_SCU_LPC_CTRL"
	},
	{
		.addr_offset = 0x0324,
		.value_mask = 0x03,
		.expect_value = 0x00,
		.preg_name = "REG_AON_APB_APCPU_DDR_AB_LPC_CTRL"
	}
};

static struct intc_info roc1_pintc_info[] = {
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "AP_UART0", "AP_UART1",
		"AP_UART2", "AP_SPI0",	"AP_SPI1", "AP_SPI2",
		"AP_SPI3", "AP_SIM0", "AP_EMMC",	"AP_I2C0",
		"AP_I2C1", "AP_I2C2", "AP_I2C3", "AP_I2C4",
		"AP_IIS0", "AP_IIS1", "AP_IIS2", "AP_SDIO0",
		"AP_SDIO1", "AP_SDIO2", "CE_SEC", "CE_PUB",
		"AP_DMA", "DMA_SEC_AP", "GSP", "DISPC",
		"SLV_FW_AP_INTERRUPT", "DSI_PLL", "DSI0",
		"DSI1"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "VSP", "VDMA",
		"VDSP_MMU", "VDSP_WDOG", "VDSP", "VDSP_STAT",
		"AP_BUSMON", "CPP_ARM_INT",
		"JPG_ARM_INT", "ISP_CH0", "ISP_CH1",
		"CSI0_CAL_FAILED", "CSI0_CAL_DONE",
		"CSI0_R2", "CSI0_R1", "CSI1_CAL_FAILED",
		"CSI1_CAL_DONE", "CSI1_R2", "CSI1_R1",
		"CSI2_CAL_FAILED", "CSI2_CAL_DONE",
		"CSI2_R2", "CSI2_R1", "DCAM0_ARM",
		"DCAM1_ARM", "DCAM2_ARM", "GPU", "GPIO",
		"THM0", "THM1"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = { "null", "null", "THM2", "KPD",
		"AON_I2C", "OTG", "ADI", "AON_TMR",
		"EIC", "AP_TMR0", "AP_TMR1", "AP_TMR2",
		"AP_TMR3", "AP_TMR4", "AP_SYST", "APCPU_WDG",
		"AP_WDG", "BUSMON_AON", "MBOX_SRC_AP",
		"MBOX_TAR_AP", "MBOX_TAR_SIPC_AP_NOWAKEUP",
		"PWR_UP_AP", "PWR_UP_PUB", "PWR_UP_ALL",
		"PWR_DOWN_ALL", "SEC_EIC", "SEC_EIC_NON_LAT",
		"SEC_WDG", "SEC_RTC", "SEC_TMR", "SEC_GPIO",
		"SLV_FW_AON"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "MEM_FW_AON", "AON_32K_DET",
		"SCC", "IIS", "DEBUG_FILTER", "EXT_RSTB_APCPU",
		"AON_SYST", "MEM_FW_PUB", "PUB_HDW_DFS_EXIT",
		"PUB_DFS_ERROR", "PUB_DFS_COMPLETE", "PUB_PTM",
		"DFI_BUS_MONITOR_PUB", "REG_PUB_DMC_MPU_VIO",
		"NPMUIRQ0", "NPMUIRQ1", "NPMUIRQ2", "NPMUIRQ3",
		"NPMUIRQ4", "NPMUIRQ5", "NPMUIRQ6", "NPMUIRQ7",
		"NCTIIRQ0", "NCTIIRQ1", "NCTIIRQ2", "NCTIIRQ3",
		"NCTIIRQ4", "NCTIIRQ5", "NCTIIRQ6", "NCTIIRQ7"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "NERRIRQ",
		"AON_I2C1", "AI_POWERVR",
		"AI_CAMBRICOM_INT_VALID", "AI_MMU_INT", "USB3_IPA",
		"SLV_FW_IPA", "PAM_IPA_TO_AP_IPA", "MAP_IPA",
		"UNIPRO_INT", "HCI_INT", "FD", "PAMU3_IPA",
		"PAM_WIFI_TO_AP_IPA", "IPA_WDG_RST", "NFAULTIRQ",
		"IRQ_IPA", "FIQ_IPA", "PCIE_G2_MSI_CTRL_IPA",
		"PCIE_G2_CFG_AER_RC_ERR_IPA", "PCIE_G2_CFG_AER_RC_ERR_MSI_IPA",
		"G2_RX_MSG_IPA", "G2_ERR_IPA", "PCIE_G3_MSI_CTRL_IPA",
		"PCIE_G3_CFG_AER_RC_ERR_IPA", "PCIE_G3_CFG_AER_RC_ERR_MSI_IPA",
		"G3_RX_MSG_IPA", "G3_ERR_IPA", "SLV_FW_1_IPA",
		"PAMU3_COMFIFO_IPA"}
	},
	{
		.addr_offset = 0x0000,
		.pint_name = {"null", "null", "IPACM4INT2AP_IPA", "null",
		"NERRIRQ0", "NFAULTIRQ0", "APCPU_PMU",
		"APCPU_ERR", "APCPU_FAULTY", "ACC_PROT_PMU",
		"ACC_PROT_AON_APB_REG",	"APCPU_MODE_ST", "NCLUSTERPMUIRQ",
		"ANA", "WTLCP_TG_WDG_RST", "WTLCP_LTE_WDG_RST",
		"MDAR", "AUDCP_CHN_START_CHN0",
		"AUDCP_CHN_START_CHN1", "AUDCP_CHN_START_CHN2",
		"AUDCP_CHN_START_CHN3", "AUDCP_DMA",
		"AUDCP_MCDT", "AUDCP_VBC_AUDRCD",
		"AUDCP_VBC_AUDPLY", "AUDCP_WDG_RST",
		"ACC_PROT_PMU", "ACC_PROT_AON_APB_REG",
		"EIC_NON_LAT", "PUB_DFS_GIVEUP",
		"PUB_DFS_DENY", "PUB_DFS_VOTE_DONE"}
	}
};

static void roc1_output_2nd_irq_source(void *pentry, u32 hw_irq_nr)
{
	struct power_debug *pdbg = (struct power_debug *)pentry;

	if (pdbg_roc1.pdbg != pdbg)
		return;

	if (hw_irq_nr == AP_INTC_PMIC_INDEX)
		sc2730_output_irq_source();
}

static struct power_debug_desc roc1_pdbg_desc = {
	.name = "roc1-pdbg",

	.pmu_pdm_num = sizeof(roc1_pdm_info)/sizeof(struct pdm_info),
	.ap_ahb_reg_num = sizeof(roc1_ap_ahb_reg)/sizeof(struct reg_check),
	.ap_apb_reg_num = sizeof(roc1_ap_apb_reg)/sizeof(struct reg_check),
	.pmu_apb_reg_num = sizeof(roc1_pmu_apb_reg)/sizeof(struct reg_check),
	.aon_apb_reg_num = sizeof(roc1_aon_apb_reg)/sizeof(struct reg_check),
	.ap_intc_num = sizeof(roc1_pintc_info)/sizeof(struct intc_info),
	.irq_mask = {0, 0, 0, 0, 0, 0x2000, 0, 0},

	.ppdm_info = roc1_pdm_info,
	.ap_ahb_reg = roc1_ap_ahb_reg,
	.ap_apb_reg = roc1_ap_apb_reg,
	.pmu_apb_reg = roc1_pmu_apb_reg,
	.aon_apb_reg = roc1_aon_apb_reg,
	.pintc_info = roc1_pintc_info,
	.log_2nd_irq_source = roc1_output_2nd_irq_source
};

/**
 * dev - Parse the dts node information of this driver, and
 * construct the core structure used in this driver.
 * intc_num - the intc number of ap in aon system
 */
static struct power_debug_cfg *sprd_pdbg_parse_cfg(struct device *dev,
					u32 intc_num)
{
	int result;
	u32 i;
	struct power_debug_cfg *pcfg;
	struct device_node *psub_node;
	struct device_node *pnode = dev->of_node;

	if (!pnode) {
		dev_err(dev, "Power debug device node not found\n");
		return ERR_PTR(-ENODEV);
	}

	pcfg = devm_kzalloc(dev, (sizeof(struct power_debug_cfg) +
			intc_num * sizeof(struct regmap *)), GFP_KERNEL);
	if (!pcfg)
		return ERR_PTR(-ENOMEM);

	result = of_property_read_u32(
		pnode, "sprd,enable", &pcfg->pdbg_enable);
	if (result)
		pcfg->pdbg_enable = 1;

	result = of_property_read_u32(
		pnode, "sprd,scan-interval", &pcfg->scan_interval);
	if (result)
		pcfg->scan_interval = 30;

	pcfg->ap_ahb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-ap-ahb");
	if (IS_ERR(pcfg->ap_ahb)) {
		dev_warn(dev, "Not to get ap-ahb regmap\n");
		pcfg->ap_ahb = NULL;
	}

	pcfg->ap_apb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-ap-apb");
	if (IS_ERR(pcfg->ap_apb)) {
		dev_warn(dev, "Not to get ap-apb regmap\n");
		pcfg->ap_apb = NULL;
	}

	pcfg->pmu_apb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-pmu-apb");
	if (IS_ERR(pcfg->pmu_apb)) {
		dev_err(dev, "Failed to get pmu-apb regmap\n");
		devm_kfree(dev, pcfg);
		return ERR_PTR(-EINVAL);
	}

	pcfg->aon_apb = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-aon-apb");
	if (IS_ERR(pcfg->aon_apb)) {
		dev_warn(dev, "Not to get aon-apb regmap\n");
		pcfg->aon_apb = NULL;
	}

	pcfg->aon_sec = syscon_regmap_lookup_by_phandle(
		pnode, "sprd,sys-aon-sec");
	if (IS_ERR(pcfg->aon_sec)) {
		dev_warn(dev, "Not to get aon-sec regmap\n");
		pcfg->aon_sec = NULL;
	}

	for (i = 0; i < intc_num; i++) {
		psub_node = of_parse_phandle(pnode, "sprd,sys-ap-intc", i);
		if (psub_node) {
			pcfg->ap_intc[i] = syscon_node_to_regmap(psub_node);
			of_node_put(psub_node);
			if (IS_ERR(pcfg->ap_intc[i])) {
				dev_err(dev,
					"Failed to get ap-intc[%d] regmap\n",
					i);
				devm_kfree(dev, pcfg);
				return ERR_PTR(-EINVAL);
			}
		} else {
			devm_kfree(dev, pcfg);
			dev_err(dev,
				"Device node ap-intc[%d] not found\n", i);
			return ERR_PTR(-ENODEV);
		}
	}

	return pcfg;
}

/**
 * sprd_powerdebug_probe - add the power debug driver
 */
static int sprd_pdbg_roc1_probe(struct platform_device *pdev)
{
	struct power_debug_cfg *pcfg;

	dev_dbg(&pdev->dev, "##### Power debug driver init start #####\n");

	pcfg = sprd_pdbg_parse_cfg(&pdev->dev, roc1_pdbg_desc.ap_intc_num);
	if (IS_ERR(pcfg))
		return (uintptr_t)pcfg;

	pdbg_roc1.pdbg = sprd_power_debug_register(&pdev->dev,
				&roc1_pdbg_desc,	pcfg);
	if (!pdbg_roc1.pdbg) {
		devm_kfree(&pdev->dev, pcfg);
		dev_dbg(&pdev->dev, "##### Power debug driver init failure #####\n");
		return -EFAULT;
	}
	pdbg_roc1.pcfg = pcfg;
	pdbg_roc1.pdev = pdev;

	dev_dbg(&pdev->dev,
		"##### Power debug driver init successfully #####\n");

	return 0;
}

/**
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_pdbg_roc1_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "##### Power debug driver remove #####\n");
	sprd_power_debug_unregister(pdbg_roc1.pdbg);
	pdbg_roc1.pdbg = NULL;

	devm_kfree(&pdev->dev, pdbg_roc1.pcfg);
	pdbg_roc1.pcfg = NULL;
	pdbg_roc1.pdev = NULL;

	return 0;
}

static const struct of_device_id sprd_pdbg_roc1_of_match[] = {
	{
		.compatible = "sprd,power-debug-roc1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_roc1_of_match);

static struct platform_driver sprd_pdbg_roc1_driver = {
	.probe = sprd_pdbg_roc1_probe,
	.remove = sprd_pdbg_roc1_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_pdbg_roc1_of_match,
	},
};

module_platform_driver(sprd_pdbg_roc1_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd roc1 power debug driver");
