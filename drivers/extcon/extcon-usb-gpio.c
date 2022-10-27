// SPDX-License-Identifier: GPL-2.0-only
/**
 * drivers/extcon/extcon-usb-gpio.c - USB GPIO extcon driver
 *
 * Copyright (C) 2015 Texas Instruments Incorporated - http://www.ti.com
 * Author: Roger Quadros <rogerq@ti.com>
 */

#include <linux/extcon-provider.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>

#define USB_GPIO_DEBOUNCE_MS	20	/* ms */

struct usb_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;

	struct gpio_desc *id_gpiod;
	struct gpio_desc *vbus_gpiod;
	int id_irq;
	int vbus_irq;

	unsigned long debounce_jiffies;
	struct delayed_work wq_detcable;
};

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

//otg switch
static int otg_switch_flage = 0;
static struct usb_extcon_info *otg_info = NULL;
static int cur_irq = 0;
static int usb_state = EXTCON_NONE;
static int g_no_otg_id = 0;

/*
 * "USB" = VBUS and "USB-HOST" = !ID, so we have:
 * Both "USB" and "USB-HOST" can't be set as active at the
 * same time so if "USB-HOST" is active (i.e. ID is 0)  we keep "USB" inactive
 * even if VBUS is on.
 *
 *  State              |    ID   |   VBUS
 * ----------------------------------------
 *  [1] USB            |    H    |    H
 *  [2] none           |    H    |    L
 *  [3] USB-HOST       |    L    |    H
 *  [4] USB-HOST       |    L    |    L
 *
 * In case we have only one of these signals:
 * - VBUS only - we want to distinguish between [1] and [2], so ID is always 1.
 * - ID only - we want to distinguish between [1] and [4], so VBUS = ID.
*/
static void usb_extcon_detect_cable(struct work_struct *work)
{
	int id, vbus;
	struct usb_extcon_info *info = container_of(to_delayed_work(work),
						    struct usb_extcon_info,
						    wq_detcable);
#if 0

	/* check ID and VBUS and update cable state */
	id = info->id_gpiod ?
		gpiod_get_value_cansleep(info->id_gpiod) : 1;
	vbus = info->vbus_gpiod ?
		gpiod_get_value_cansleep(info->vbus_gpiod) : id;

	/* at first we clean states which are no longer active */
	if (id)
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
	if (!vbus)
		extcon_set_state_sync(info->edev, EXTCON_USB, false);

	if (!id) {
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, true);
	} else {
		if (vbus)
			extcon_set_state_sync(info->edev, EXTCON_USB, true);
	}
#else
//otg switch
	id = gpiod_get_value_cansleep(info->id_gpiod);
	vbus = gpiod_get_value_cansleep(info->vbus_gpiod);
	dev_info(info->dev, "<<<<<<<<<<<<<usb_extcon_detect_cable id = %d,vbus = %d\n", id, vbus);
	if (info->id_gpiod && cur_irq == info->id_irq) {
		dev_err(info->dev, "<<<<<<<<<<<<current irq %d get id state %d\n",cur_irq, id);
		if (id) {
			extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
			usb_state = EXTCON_NONE;
		} else {
			extcon_set_state_sync(info->edev, EXTCON_USB, false);
			extcon_set_state_sync(info->edev, EXTCON_USB_HOST, true);
			usb_state = EXTCON_USB_HOST;
		}
	} else if (cur_irq == info->vbus_irq){
		dev_info(info->dev, "<<<<<<<<<<<<<<current irq %d get vbus state %d\n",cur_irq, vbus);
		/* at first we clean states which are no longer active */
		if (!vbus) {
			extcon_set_state_sync(info->edev, EXTCON_USB, false);
			usb_state = EXTCON_NONE;
		} else {
			extcon_set_state_sync(info->edev, EXTCON_USB, true);
			usb_state = EXTCON_USB;
		}
	} else {
		dev_info(info->dev, "<<<<<<<<<<<<<<usb_state %d\n",usb_state);
		switch(usb_state){
			case EXTCON_NONE:
				extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
				if (!vbus) {
					extcon_set_state_sync(info->edev, EXTCON_USB, false);
					usb_state = EXTCON_NONE;
				} else {
					extcon_set_state_sync(info->edev, EXTCON_USB, true);
					usb_state = EXTCON_USB;
				}
				break;
			case EXTCON_USB:
				if (!vbus) {
					extcon_set_state_sync(info->edev, EXTCON_USB, false);
					usb_state = EXTCON_NONE;
				} else {
					extcon_set_state_sync(info->edev, EXTCON_USB, true);
					usb_state = EXTCON_USB;
				}
				break;
			case EXTCON_USB_HOST:
				if (id) {
					extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
					usb_state = EXTCON_NONE;
				} else {
					extcon_set_state_sync(info->edev, EXTCON_USB_HOST, true);
					usb_state = EXTCON_USB_HOST;
				}
				break;
			default:
				break;
		}

		dev_err(info->dev, "<<<<<<<<<<<no irq happened, just get state id:%d,vbus:%d\n",id, vbus);
	}
	cur_irq = 0;
#endif
}

static irqreturn_t usb_irq_handler(int irq, void *dev_id)
{
	struct usb_extcon_info *info = dev_id;
//otg switch
	cur_irq = irq;
	dev_info(info->dev, "<<<<<<<<<<<usb_irq_handler");
	queue_delayed_work(system_power_efficient_wq, &info->wq_detcable,
			   info->debounce_jiffies);

	return IRQ_HANDLED;
}

static int boot_mode_check(void)
{
	struct device_node *np;
	const char *cmd_line;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return 0;

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0)
		return 0;

	if (strstr(cmd_line, "androidboot.mode=cali"))
		return 0;

	if (strstr(cmd_line, "androidboot.mode=autotest"))
		return 0;

	if (strstr(cmd_line, "androidboot.mode=charger"))
		return 0;

	if (strstr(cmd_line, "androidboot.mode=recovery"))
			return 0;

	//printk("lj: cmd_line:%s\n", cmd_line);
	if (strstr(cmd_line, "lcd_name=lcd_ft8006s_milan_hdplus_boe"))
		ret = 1;

	return ret;
}


static int usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct usb_extcon_info *info;
//otg switch
	void __iomem  *addr;
	int ret;
	
	g_no_otg_id = boot_mode_check();
	dev_info(dev, "<<<<<<<<<<<usb_extcon_probe=====g_no_otg_id:%d\n", g_no_otg_id);
	if (!np)
		return -EINVAL;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	info->id_gpiod = devm_gpiod_get_optional(&pdev->dev, "id", GPIOD_IN);
	info->vbus_gpiod = devm_gpiod_get_optional(&pdev->dev, "vbus",
						   GPIOD_IN);

//otg switch
	otg_info = info;
	if (!info->id_gpiod && !info->vbus_gpiod) {
		dev_err(dev, "failed to get gpios\n");
		return -ENODEV;
	}

	if (IS_ERR(info->id_gpiod))
		return PTR_ERR(info->id_gpiod);

	if (IS_ERR(info->vbus_gpiod))
		return PTR_ERR(info->vbus_gpiod);

	info->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, info->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return ret;
	}

	if (info->id_gpiod)
		ret = gpiod_set_debounce(info->id_gpiod,
					 USB_GPIO_DEBOUNCE_MS * 1000);
	if (!ret && info->vbus_gpiod)
		ret = gpiod_set_debounce(info->vbus_gpiod,
					 USB_GPIO_DEBOUNCE_MS * 1000);

	if (ret < 0)
		info->debounce_jiffies = msecs_to_jiffies(USB_GPIO_DEBOUNCE_MS);

	INIT_DELAYED_WORK(&info->wq_detcable, usb_extcon_detect_cable);

	if (info->id_gpiod) {
		info->id_irq = gpiod_to_irq(info->id_gpiod);
		if (info->id_irq < 0) {
			dev_err(dev, "failed to get ID IRQ\n");
			return info->id_irq;
		}

	#if 0
		ret = devm_request_threaded_irq(dev, info->id_irq, NULL,
						usb_irq_handler,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						pdev->name, info);
		if (ret < 0) {
			dev_err(dev, "failed to request handler for ID IRQ\n");
			return ret;
		}
	#else
//otg switch
		dev_info(info->dev, "<<<<<<<<<<<usb_extcon_probe  addr = ioremap(0x402A0610, 128);");
		addr = ioremap(0x402A0610, 128);
		if(addr == NULL){
			dev_err(otg_info->dev, "failed to get USB_ID addr\n");
			return -EINVAL;;
		}
		writel_relaxed(0x00082045, addr);//
		
	#endif	
	}

	if (info->vbus_gpiod) {
		info->vbus_irq = gpiod_to_irq(info->vbus_gpiod);
		if (info->vbus_irq < 0) {
			dev_err(dev, "failed to get VBUS IRQ\n");
			return info->vbus_irq;
		}

		ret = devm_request_threaded_irq(dev, info->vbus_irq, NULL,
						usb_irq_handler,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						pdev->name, info);
		if (ret < 0) {
			dev_err(dev, "failed to request handler for VBUS IRQ\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, info);
	device_set_wakeup_capable(&pdev->dev, true);

	/* Perform initial detection */
	usb_extcon_detect_cable(&info->wq_detcable.work);

	return 0;
}

//otg switch
void otg_switch_mode(int value)
{
	int ret = 0;
	int id, vbus;
	void __iomem  *addr;  
	
	dev_info(otg_info->dev, "lj $$$$$$$$$$$$$$ into <value = %d,otg_switch_flage=%d   $$$$$$$$$$$\n", value, otg_switch_flage);

	if (1 == g_no_otg_id)  {
		id = gpiod_get_value(otg_info->id_gpiod);
		vbus = gpiod_get_value(otg_info->vbus_gpiod);
		dev_info(otg_info->dev, "lj 111 <<<<<<<<<<<<<id = %d,vbus=%d\n", id, vbus);
			
		if (id && vbus)	{   //lj maybe nee delete
			dev_err(otg_info->dev, "lj id && vbus always 1, so return null???  maybe nee delete   #####################\n");
		//	return;
		}
			
	 	if (value) {
			dev_err(otg_info->dev, "lj self into host value:%d********b***** \n", value);

			extcon_set_state_sync(otg_info->edev, EXTCON_USB, false);
			extcon_set_state_sync(otg_info->edev, EXTCON_USB_HOST, true);
			//msleep(300);
			disable_irq(otg_info->vbus_irq);
	  	} else {
			dev_err(otg_info->dev, "lj self exit host value:%d********b***** \n", value);
			
			extcon_set_state_sync(otg_info->edev, EXTCON_USB_HOST, false);
			msleep(1000);

			vbus = gpiod_get_value_cansleep(otg_info->vbus_gpiod);
		//	dev_err(otg_info->dev, "lj self seleep 1 s vbus:%d********b***** \n", vbus);

			if (!vbus) {
				extcon_set_state_sync(otg_info->edev, EXTCON_USB, false);
				dev_err(otg_info->dev, "lj usb not connect value:%d vbus:%d  \n", vbus, value);
			} else {
				extcon_set_state_sync(otg_info->edev, EXTCON_USB, true);
				dev_err(otg_info->dev, "lj usb is connect value:%d vbus:%d  \n", vbus, value);
			}

			msleep(1000);

			enable_irq(otg_info->vbus_irq);
	  	}
	} else {

		dev_info(otg_info->dev, "lj 000<<<<<<<<<<<<<value = %d,otg_switch_flage=%d\n", value, otg_switch_flage);
		id = gpiod_get_value(otg_info->id_gpiod);
	    vbus = gpiod_get_value(otg_info->vbus_gpiod);
		dev_info(otg_info->dev, "lj 000 <<<<<<<<<<<<<id = %d,vbus=%d\n", id, vbus);

		if (id && vbus)	
			return;
	  	if(!otg_info->id_gpiod){
	  		dev_err(otg_info->dev, "failed to request otg_info->id_gpiod\n");
	  		return;
		}

	  	addr = ioremap(0x402A0610, 128);
	  	if(addr == NULL){
	  		dev_err(otg_info->dev, "failed to get USB_ID addr\n");
	  		return;
	}

 	if (value) {
  		if (!otg_switch_flage) {
			writel_relaxed(0x00082089, addr);
  			msleep(100);
  			ret = devm_request_threaded_irq(otg_info->dev, otg_info->id_irq, NULL,
  						usb_irq_handler,
  						IRQF_TRIGGER_RISING |
  						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
  						NULL, otg_info);
  			if (ret < 0) {
  				dev_err(otg_info->dev, "<<<<<<<<otg<failed to request handler for ID IRQ\n");
  				writel_relaxed(0x00082045, addr);
 				iounmap(addr);
  				return;
  			}
  			otg_switch_flage = 1;
  			dev_err(otg_info->dev, "switch otg on\n");
  		} else {
  			dev_err(otg_info->dev, "otg is on, not switch\n");
  		}
  	} else {
  		if (otg_switch_flage) {
  			otg_switch_flage = 0;
  			disable_irq(otg_info->id_irq);
  			if (!gpiod_get_value(otg_info->id_gpiod))
  				extcon_set_state_sync(otg_info->edev, EXTCON_USB_HOST, false);
  			devm_free_irq(otg_info->dev, otg_info->id_irq, otg_info);
			writel_relaxed(0x00082045, addr);
  			dev_err(otg_info->dev, "switch otg off\n");
  		} else {
  			dev_err(otg_info->dev, "<<<<<<<<<<otg is off, not switch\n");
  		}
  	}
  iounmap(addr);
  }
		
}

  EXPORT_SYMBOL(otg_switch_mode);

static int usb_extcon_remove(struct platform_device *pdev)
{
	struct usb_extcon_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&info->wq_detcable);
	device_init_wakeup(&pdev->dev, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int usb_extcon_suspend(struct device *dev)
{
	struct usb_extcon_info *info = dev_get_drvdata(dev);
	int ret = 0;
	return ret;

	if (device_may_wakeup(dev)) {
		if (info->id_gpiod) {
			ret = enable_irq_wake(info->id_irq);
			if (ret)
				return ret;
		}
		if (info->vbus_gpiod) {
			ret = enable_irq_wake(info->vbus_irq);
			if (ret) {
				if (info->id_gpiod)
					disable_irq_wake(info->id_irq);

				return ret;
			}
		}
	}

	/*
	 * We don't want to process any IRQs after this point
	 * as GPIOs used behind I2C subsystem might not be
	 * accessible until resume completes. So disable IRQ.
	 */
	if (info->id_gpiod)
		disable_irq(info->id_irq);
	if (info->vbus_gpiod)
		disable_irq(info->vbus_irq);

	if (!device_may_wakeup(dev))
		pinctrl_pm_select_sleep_state(dev);

	return ret;
}

static int usb_extcon_resume(struct device *dev)
{
	struct usb_extcon_info *info = dev_get_drvdata(dev);
	int ret = 0;
	return ret;

	if (!device_may_wakeup(dev))
		pinctrl_pm_select_default_state(dev);

	if (device_may_wakeup(dev)) {
		if (info->id_gpiod) {
			ret = disable_irq_wake(info->id_irq);
			if (ret)
				return ret;
		}
		if (info->vbus_gpiod) {
			ret = disable_irq_wake(info->vbus_irq);
			if (ret) {
				if (info->id_gpiod)
					enable_irq_wake(info->id_irq);

				return ret;
			}
		}
	}

	if (info->id_gpiod)
		enable_irq(info->id_irq);
	if (info->vbus_gpiod)
		enable_irq(info->vbus_irq);

	queue_delayed_work(system_power_efficient_wq,
			   &info->wq_detcable, 0);

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(usb_extcon_pm_ops,
			 usb_extcon_suspend, usb_extcon_resume);

static const struct of_device_id usb_extcon_dt_match[] = {
	{ .compatible = "linux,extcon-usb-gpio", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, usb_extcon_dt_match);

static const struct platform_device_id usb_extcon_platform_ids[] = {
	{ .name = "extcon-usb-gpio", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, usb_extcon_platform_ids);

static struct platform_driver usb_extcon_driver = {
	.probe		= usb_extcon_probe,
	.remove		= usb_extcon_remove,
	.driver		= {
		.name	= "extcon-usb-gpio",
		.pm	= &usb_extcon_pm_ops,
		.of_match_table = usb_extcon_dt_match,
	},
	.id_table = usb_extcon_platform_ids,
};

module_platform_driver(usb_extcon_driver);

MODULE_AUTHOR("Roger Quadros <rogerq@ti.com>");
MODULE_DESCRIPTION("USB GPIO extcon driver");
MODULE_LICENSE("GPL v2");
