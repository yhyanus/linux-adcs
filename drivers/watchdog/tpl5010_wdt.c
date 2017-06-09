/*
 * Driver for watchdog device controlled through GPIO-line
 *
 * Author: 2013, Alexander Shiyan <shc_work@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#define dev_dbg dev_err
#define SOFT_TIMEOUT_MIN	1
#define SOFT_TIMEOUT_DEF	60
#define SOFT_TIMEOUT_MAX	0xffff
#define dev_dbg dev_err

enum {
	HW_ALGO_TOGGLE,
	HW_ALGO_LEVEL,
};

struct tpl5010_wdt_priv {
	int			gpio;
	bool			active_low;
	bool			state;
	bool			always_running;
	bool			armed;
	unsigned int		hw_algo;
	unsigned int		hw_margin;
	unsigned long		last_jiffies;
	struct notifier_block	notifier;
//	struct timer_list	timer;
	struct watchdog_device	wdd;
	int	  gpio_reset, gpio_timer;
};

static void tpl5010_wdt_disable(struct tpl5010_wdt_priv *priv)
{
	gpio_set_value_cansleep(priv->gpio, !priv->active_low);

	/* Put GPIO back to tristate */
	if (priv->hw_algo == HW_ALGO_TOGGLE)
		gpio_direction_input(priv->gpio);
}

static void tpl5010_wdt_start_impl(struct tpl5010_wdt_priv *priv)
{
	priv->state = priv->active_low;
	gpio_direction_output(priv->gpio, priv->state);
	priv->last_jiffies = jiffies;
//	mod_timer(&priv->timer, priv->last_jiffies + priv->hw_margin);
}

static int tpl5010_wdt_start(struct watchdog_device *wdd)
{
	struct tpl5010_wdt_priv *priv = watchdog_get_drvdata(wdd);

	tpl5010_wdt_start_impl(priv);
	priv->armed = true;

	return 0;
}

static int tpl5010_wdt_stop(struct watchdog_device *wdd)
{
	struct tpl5010_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->armed = false;
	if (!priv->always_running) {
		//mod_timer(&priv->timer, 0);
		tpl5010_wdt_disable(priv);
	}

	return 0;
}

static int tpl5010_wdt_ping(struct watchdog_device *wdd)
{
	struct tpl5010_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->last_jiffies = jiffies;

	return 0;
}

static int tpl5010_wdt_set_timeout(struct watchdog_device *wdd, unsigned int t)
{
	wdd->timeout = t;

	return tpl5010_wdt_ping(wdd);
}

static void tpl5010_wdt_hwping(unsigned long data)
{
	struct watchdog_device *wdd = (struct watchdog_device *)data;
	struct tpl5010_wdt_priv *priv = watchdog_get_drvdata(wdd);
#if 0
	if (priv->armed && time_after(jiffies, priv->last_jiffies +
				      msecs_to_jiffies(wdd->timeout * 1000))) {
		dev_crit(wdd->dev, "Timer expired. System will reboot soon!\n");
		return;
	}

	/* Restart timer */
	mod_timer(&priv->timer, jiffies + priv->hw_margin);
#endif
	switch (priv->hw_algo) {
	case HW_ALGO_TOGGLE:
		/* Toggle output pin */
		priv->state = !priv->state;
		gpio_set_value_cansleep(priv->gpio, priv->state);
		break;
	case HW_ALGO_LEVEL:
		/* Pulse */
		gpio_set_value_cansleep(priv->gpio, !priv->active_low);
		udelay(1);
		gpio_set_value_cansleep(priv->gpio, priv->active_low);
		break;
	}
}

static int tpl5010_wdt_notify_sys(struct notifier_block *nb, unsigned long code,
			       void *unused)
{
	struct tpl5010_wdt_priv *priv = container_of(nb, struct tpl5010_wdt_priv,
						  notifier);

	//mod_timer(&priv->timer, 0);

	switch (code) {
	case SYS_HALT:
	case SYS_POWER_OFF:
		tpl5010_wdt_disable(priv);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}
static irqreturn_t wdog_timer(int irq, void *dev_id)  
{  
 
	tpl5010_wdt_hwping((unsigned long) dev_id);
    return IRQ_HANDLED;  
}  
static irqreturn_t  wdog_irqhandler(int irq, void *dev_id)
{
    printk(KERN_INFO "In hard irq handler.\n");
    return IRQ_WAKE_THREAD;
}
static const struct watchdog_info tpl5010_wdt_ident = {
	.options	= WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING |
			  WDIOF_SETTIMEOUT,
	.identity	= "TPL5010 Watchdog",
};

static const struct watchdog_ops tpl5010_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= tpl5010_wdt_start,
	.stop		= tpl5010_wdt_stop,
	.ping		= tpl5010_wdt_ping,
	.set_timeout	= tpl5010_wdt_set_timeout,
};
static irqreturn_t reset_timer(int irq, void *dev_id)  
{  
 	printk( "reset_timer called.\n");
		//tpl5010_wdt_hwping((unsigned long) dev_id);
    return IRQ_HANDLED;  
}  
static irqreturn_t  reset_irqhandler(int irq, void *dev_id)
{
    printk( "In hard irq handler. irq=%d\n",irq);
    return IRQ_WAKE_THREAD;
}
static int tpl5010_wdt_probe(struct platform_device *pdev)
{
	struct tpl5010_wdt_priv *priv;
	enum of_gpio_flags flags;
	unsigned int hw_margin;
	unsigned long f = 0;
	const char *algo;
	int ret;
dev_dbg(&pdev->dev, "%s enter \n",__func__);

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	
	priv->gpio_reset = of_get_named_gpio_flags(pdev->dev.of_node, "gpio_reset", 0, &flags);
	priv->gpio_timer = of_get_named_gpio_flags(pdev->dev.of_node, "gpio_timer", 0, &flags);
	


	priv->gpio = of_get_gpio_flags(pdev->dev.of_node, 0, &flags);
	if (!gpio_is_valid(priv->gpio))
	{
		dev_err(&pdev->dev, "invalid   gpio  \n");
		return priv->gpio;
	}
	priv->active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = of_property_read_string(pdev->dev.of_node, "hw_algo", &algo);
	if (ret)
		return ret;
	if (!strncmp(algo, "toggle", 6)) {
		priv->hw_algo = HW_ALGO_TOGGLE;
		f = GPIOF_IN;
	} else if (!strncmp(algo, "level", 5)) {
		priv->hw_algo = HW_ALGO_LEVEL;
		f = priv->active_low ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
	} else {
		dev_err(&pdev->dev, "hw_algo error \n");
		return -EINVAL;
	}
 
	ret = devm_gpio_request_one(&pdev->dev, priv->gpio, f,
				    dev_name(&pdev->dev));
	if (ret)
	{
		dev_err(&pdev->dev, "devm_gpio_request_one fail \n");
		return ret;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "hw_margin_ms", &hw_margin);
	if (ret)
		return ret;
	/* Disallow values lower than 2 and higher than 65535 ms */
	if (hw_margin < 2 || hw_margin > 65535)
		return -EINVAL;
 
	/* Use safe value (1/2 of real timeout) */
	priv->hw_margin = msecs_to_jiffies(hw_margin / 2);

	priv->always_running = of_property_read_bool(pdev->dev.of_node,
						     "always-running");

	watchdog_set_drvdata(&priv->wdd, priv);

	priv->wdd.info		= &tpl5010_wdt_ident;
	priv->wdd.ops		= &tpl5010_wdt_ops;
	priv->wdd.min_timeout	= SOFT_TIMEOUT_MIN;
	priv->wdd.max_timeout	= SOFT_TIMEOUT_MAX;

	if (watchdog_init_timeout(&priv->wdd, 0, &pdev->dev) < 0)
		priv->wdd.timeout = SOFT_TIMEOUT_DEF;

//	setup_timer(&priv->timer, tpl5010_wdt_hwping, (unsigned long)&priv->wdd);
	if (!gpio_is_valid(priv->gpio_timer))
	{
		gpio_request(priv->gpio_timer, dev_name(&pdev->dev));  
		gpio_direction_input(priv->gpio_timer); 		 
		gpio_free(priv->gpio_timer);   
		//request_irq( gpio_to_irq(priv->gpio_timer)  , wdog_timer, IRQF_TRIGGER_FALLING, dev_name(&pdev->dev) , &priv->wdd);  
		devm_request_threaded_irq(&pdev->dev, gpio_to_irq(priv->gpio_timer)  , wdog_irqhandler, wdog_timer, IRQF_TRIGGER_FALLING, dev_name(&pdev->dev) , &priv->wdd);
		dev_dbg(&pdev->dev, "devm_request_threaded_irq %s \n",dev_name(&pdev->dev));
	}
	if (!gpio_is_valid(priv->gpio_reset))
	{
		gpio_request(priv->gpio_reset, dev_name(&pdev->dev));  
		gpio_direction_input(priv->gpio_reset); 		 
		gpio_free(priv->gpio_reset);   
		//request_irq( gpio_to_irq(priv->gpio_timer)  , wdog_timer, IRQF_TRIGGER_FALLING, dev_name(&pdev->dev) , &priv->wdd);  
		devm_request_threaded_irq(&pdev->dev, gpio_to_irq(priv->gpio_reset)  , reset_irqhandler, reset_timer, IRQF_TRIGGER_FALLING, dev_name(&pdev->dev) , &priv->wdd);
		dev_dbg(&pdev->dev, "devm_request_threaded_irq %s \n",dev_name(&pdev->dev));
	}
	ret = watchdog_register_device(&priv->wdd);
	if (ret)
	{
		dev_err(&pdev->dev, "watchdog_register_device fail \n");
		return ret;
	}
	priv->notifier.notifier_call = tpl5010_wdt_notify_sys;
	ret = register_reboot_notifier(&priv->notifier);
	if (ret)
	{
		dev_err(&pdev->dev, "register_reboot_notifier fail \n");
		goto error_unregister;
	}
	if (priv->always_running)
		tpl5010_wdt_start_impl(priv);

	dev_info(&pdev->dev, "tpl5010 watchdog started \n");	
	return 0;

error_unregister:
 
	watchdog_unregister_device(&priv->wdd);
	return ret;
}

static int tpl5010_wdt_remove(struct platform_device *pdev)
{
	struct tpl5010_wdt_priv *priv = platform_get_drvdata(pdev);

	//del_timer_sync(&priv->timer);
	unregister_reboot_notifier(&priv->notifier);
	watchdog_unregister_device(&priv->wdd);

	return 0;
}

static const struct of_device_id tpl5010_wdt_dt_ids[] = {
	{ .compatible = "linux,wdt-tpl5010", },
	{ }
};
MODULE_DEVICE_TABLE(of, tpl5010_wdt_dt_ids);

static struct platform_driver tpl5010_wdt_driver = {
	.driver	= {
		.name		= "tpl5010-wdt",
		.of_match_table	= tpl5010_wdt_dt_ids,
	},
	.probe	= tpl5010_wdt_probe,
	.remove	= tpl5010_wdt_remove,
};
module_platform_driver(tpl5010_wdt_driver);

MODULE_AUTHOR("Kyle Yan <jerry@gmail.com>");
MODULE_DESCRIPTION("TPL5010 Watchdog");
MODULE_LICENSE("GPL");
