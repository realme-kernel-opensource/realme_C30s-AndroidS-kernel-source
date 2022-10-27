/*
 * file name: Uart0_log.c
 */

#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/device_info.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>


MODULE_AUTHOR("dongfang.Zhang");
MODULE_LICENSE("GPL");

struct uart0_log_data {
	struct platform_device *uart0_log;
	struct pinctrl *uart0_tx;
	struct pinctrl *uart0_rx;
	struct pinctrl_state *uart0_tx_fun1;
	struct pinctrl_state *uart0_tx_fun4;
	struct pinctrl_state *uart0_rx_fun1;
	struct pinctrl_state *uart0_rx_fun4;
	struct delayed_work uart0_log_work;
	int uart0_log_mode;
};

static struct uart0_log_data *uart0_log;

static int Uart0_log_driver_remove(struct platform_device *pdev);
static int uart0_log_pin_set_function(void);
static int uart0_log_pull_gpio(void);

static ssize_t uart0_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
    return sprintf(buf , "uart0_log_mode :%d\n", uart0_log->uart0_log_mode); 
}

static ssize_t uart0_log_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	printk("---uart0_log buf[0] = %d \n",buf[0]);  

    if (buf[0] == 0x31) {
        
        uart0_log->uart0_log_mode = 1;
    } else {
       
        uart0_log->uart0_log_mode = 0;
    }

	
	return count;
}	


static DEVICE_ATTR(uart0_log_flag, S_IRUGO | S_IWUSR, uart0_log_show, uart0_log_store);

static struct attribute *uart0_log_mode_attrs[] = {
	&dev_attr_uart0_log_flag.attr,
	NULL,
};

static struct attribute_group fts_uart0_log_group = {
	.attrs = uart0_log_mode_attrs,
};

static void uart0_log_work_func(struct work_struct *work)
{
     

	if (uart0_log->uart0_log_mode) {
		uart0_log_pin_set_function();
		uart0_log_pull_gpio();		

	} else {
		schedule_delayed_work(&uart0_log->uart0_log_work,
			msecs_to_jiffies(20000));
	}

}



static int uart0_log_pin_set_function(void)
{    
    int ret = 0;       
                 
  
    ret = pinctrl_select_state(uart0_log->uart0_tx, uart0_log->uart0_tx_fun4);  
	if(ret)
		printk("---uart0_log a pinctrl state to HW--- \n"); 
	
	ret = pinctrl_select_state(uart0_log->uart0_rx, uart0_log->uart0_rx_fun4);  
	if(ret)
		printk("---uart0_log a pinctrl state  uart0_rx_fun4 to HW--- \n");  
        
    return 0;
        
}
/*功能为gpio时操作gpio引脚*/
//function：设置gpio num的引脚电平为value，切换为GPIO之后，需要调用该接口将GPIO引脚设置为输出，并设置电平
//num：引脚编号
//value：拉低写0，拉高写1
#define UART0_TX_GPIO   252   	//192+60
#define UART0_RX_GPIO	253    //192+61
static int uart0_log_pull_gpio(void)
{
	 struct gpio_desc *desc;
    int ret = 0;
      
         
        //int gpio_num = base +num; //base需要通过“cat d/gpio查看并替换为具体数字”
		desc=gpio_to_desc(UART0_RX_GPIO);
		
        ret = gpio_request(UART0_RX_GPIO, "change_to_gpio");
		if(ret)
			printk("uart0_log_pull_gpio rx reqyest ret = %d  \n",ret);
		
        //ret = gpio_direction_output(UART0_RX_GPIO, 0);
		ret = gpio_direction_input(UART0_RX_GPIO);
  		if(ret)
			printk("gpio_direction_output rx output \n");  

		
        desc=gpio_to_desc(UART0_TX_GPIO);
		
        ret = gpio_request(UART0_TX_GPIO, "change_to_gpio");
		if(ret)
			printk("uart0_log_pull_gpio reqyest ret = %d  \n",ret);
		
        //ret = gpio_direction_output(UART0_TX_GPIO, 0);
		ret = gpio_direction_input(UART0_TX_GPIO);
  		if(ret)
			printk("gpio_direction_output output \n");   


        return 0;
}


static int Uart0_log_driver_probe(struct platform_device *pdev)
{
    int ret = -1;
	 printk("Uart0_log_driver_probe  enter \n");
	uart0_log = (struct uart0_log_data *)kzalloc(sizeof(struct uart0_log_data), GFP_KERNEL);
	if (uart0_log == NULL) {
		printk("failed to allocated memory for uart0_log data\n");
		return -ENOMEM;
	}
	
	uart0_log->uart0_log_mode = 0;
	
	uart0_log->uart0_tx = devm_pinctrl_get(&pdev->dev);
   if (IS_ERR_OR_NULL(uart0_log->uart0_tx)) {
        printk("Failed to get uart0_tx, please check dts");
        ret = PTR_ERR(uart0_log->uart0_tx);
        goto err;
    }
	
	uart0_log->uart0_tx_fun1 = pinctrl_lookup_state(uart0_log->uart0_tx, "gpio_60_f0");
    if (IS_ERR_OR_NULL(uart0_log->uart0_tx_fun1)) {
        printk("uart0_tx_fun1 state[active] not found");
        ret = PTR_ERR(uart0_log->uart0_tx_fun1);
        goto err;
    } 

	uart0_log->uart0_tx_fun4 = pinctrl_lookup_state(uart0_log->uart0_tx, "gpio_60_f3");
    if (IS_ERR_OR_NULL(uart0_log->uart0_tx_fun4)) {
        printk("uart0_tx_fun4 state[active] not found");
        ret = PTR_ERR(uart0_log->uart0_tx_fun4);
        goto err;
    }

	uart0_log->uart0_rx = devm_pinctrl_get(&pdev->dev);
   if (IS_ERR_OR_NULL(uart0_log->uart0_rx)) {
        printk("Failed to get uart0_rx, please check dts");
        ret = PTR_ERR(uart0_log->uart0_rx);
        goto err;
    }
	
	uart0_log->uart0_rx_fun1 = pinctrl_lookup_state(uart0_log->uart0_rx, "gpio_61_f0");
    if (IS_ERR_OR_NULL(uart0_log->uart0_rx_fun1)) {
        printk("uart0_rx_fun1 state[active] not found");
        ret = PTR_ERR(uart0_log->uart0_rx_fun1);
        goto err;
    } 

	uart0_log->uart0_rx_fun4 = pinctrl_lookup_state(uart0_log->uart0_rx, "gpio_61_f3");
    if (IS_ERR_OR_NULL(uart0_log->uart0_rx_fun4)) {
        printk("uart0_rx_fun4 state[active] not found");
        ret = PTR_ERR(uart0_log->uart0_rx_fun4);
        goto err;
    }	


	ret = sysfs_create_group(&pdev->dev.kobj, &fts_uart0_log_group);
	if (ret < 0) {
		printk("Uart0_log Failed to crate sysfs attributes.\n");
		goto err0;
	}
	
	INIT_DELAYED_WORK(&uart0_log->uart0_log_work, uart0_log_work_func);
			schedule_delayed_work(&uart0_log->uart0_log_work,
				      msecs_to_jiffies(2000));
	//uart0_log_pin_set_function();
	//uart0_log_pull_gpio();

 printk("Uart0_log_driver_probe  successful! \n");
    return 0;
err0:
	Uart0_log_driver_remove(pdev);
err:
    return ret;
}

static int Uart0_log_driver_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&(pdev->dev.kobj), &fts_uart0_log_group);

    return 0;
}

static const struct of_device_id uart_log_dt_match[] = {
    {.compatible = "tinno,uart_log"},
    {}
};

static struct platform_driver Uart0_log_driver = {
    .probe = Uart0_log_driver_probe,
    .remove = Uart0_log_driver_remove,
    .driver = {
        .name = "Uart0_log",
        .of_match_table = uart_log_dt_match,
    },
};

static int __init Uart0_log_init(void)
{
    int ret = -1;

    ret = platform_driver_register(&Uart0_log_driver);
    if (ret) {
        printk("Uart0_log_init registed failed! (ret=%d)\n", ret);
    }
 printk("Uart0_log_init registed successful! \n");
    return ret;
}

static void __exit Uart0_log_exit(void)
{
    platform_driver_unregister(&Uart0_log_driver);
}

module_init(Uart0_log_init);
module_exit(Uart0_log_exit);
