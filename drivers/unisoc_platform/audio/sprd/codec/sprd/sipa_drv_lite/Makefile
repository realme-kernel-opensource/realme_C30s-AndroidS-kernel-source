# SIA PA object
SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(SPRD_SOUND_TREE)/codec/sprd

DEVICE_TYPE_DIGITAL = n
PLATFORM_TYPE = sprd
EXTRA_CFLAGS += $(call cc-option,-Wno-date-time,)

ifeq ($(DEVICE_TYPE_DIGITAL),y)
EXTRA_CFLAGS += -DSIA91XX_TYPE

ifeq ($(PLATFORM_TYPE),mtk)
EXTRA_CFLAGS += -DPLATFORM_TYPE_MTK
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
endif

ifeq ($(PLATFORM_TYPE),qcom)
EXTRA_CFLAGS += -DPLATFORM_TYPE_QCOM
endif

endif

snd-soc-sipa-objs := \
		sipa.o \
		sipa_regmap.o \
		sipa_aux_dev_if.o \
		sipa_91xx.o \
		sipa_parameter.o

ifeq ($(PLATFORM_TYPE),qcom)
snd-soc-sipa-objs += sipa_afe.o
endif

#ifeq ($(PLATFORM_TYPE),mtk)
#snd-soc-sipa-objs += sipa_tuning_misc.o # sipa_tuning_if_dev.o # sia81xx_ipi.o
#endif

snd-soc-sipa-tuning-objs += sipa_tuning_misc.o

ifeq ($(CONFIG_SND_SOC_UNISOC_CODEC_SIA81XX),m)
EXTRA_CFLAGS += -DSIPA_COMPILE_TO_MODULE
endif

obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SIA81XX) += snd-soc-sipa.o
obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SIA81XX) += snd-soc-sipa-tuning.o
