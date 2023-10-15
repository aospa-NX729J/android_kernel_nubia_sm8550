// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2099, Nubia ltd. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "usb_switch_dp.h"

struct dp_switch_priv {
	struct platform_device *pdev;
	struct device *dev;
	unsigned int switch_en;
	unsigned int switch_en_flag;
	unsigned int switch_mode;
	unsigned int switch_mode_flag;
	unsigned int usb_mode;
	bool is_enabled;
} *dp_switch = NULL;

static void usb_switch_dp_enable(int en)
{
	if (en) {
		gpio_set_value(dp_switch->switch_en, 0);
		if (dp_switch->usb_mode == SWITCH_USBC_ORIENTATION_CC1)
			gpio_set_value(dp_switch->switch_mode, 0);
		else
			gpio_set_value(dp_switch->switch_mode, 1);
	} else {
		gpio_set_value(dp_switch->switch_en, 1);
	}
}

int dp_switch_event(struct device_node *node,
		enum switch_function event)
{
	if (!dp_switch)
		return -EINVAL;
	dp_switch->usb_mode = event;
	switch (event) {
	case SWITCH_MIC_GND_SWAP:
		break;
	case SWITCH_USBC_ORIENTATION_CC1:
	case SWITCH_USBC_ORIENTATION_CC2:
		usb_switch_dp_enable(1);
		dp_switch->is_enabled = 1;
		break;
	case SWITCH_USBC_DISPLAYPORT_DISCONNECTED:
		usb_switch_dp_enable(0);
		dp_switch->is_enabled = 0;
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(dp_switch_event);

static int parse_usb_dp_switch_dt(struct device_node *node)
{
	int rc = 0;
	u32 value;

	dp_switch->switch_en =
			of_get_named_gpio_flags(node,
			"qcom,switch-en-gpio", 0, NULL);
	if (!gpio_is_valid(dp_switch->switch_en)) {
		dev_err(dp_switch->dev, "TLMM switch enable gpio not found\n");
		return -EPROBE_DEFER;
	}
	rc = gpio_request(dp_switch->switch_en, "USB_SWITCH_DP_EN");
	if (rc < 0) {
		dev_err(dp_switch->dev, "parse dp_switch->switch_en error\n");
		rc = -ENODEV;
	}
	rc = of_property_read_u32(node, "qcom,switch-en-flag", &value);
	if (rc < 0) {
		dev_err(dp_switch->dev, "parse switch-en-flag error\n");
		return rc;
	} else {
		dp_switch->switch_en_flag = value;
	}
	rc = gpio_direction_output(dp_switch->switch_en,
			dp_switch->switch_en_flag);
	if (rc < 0) {
		dev_err(dp_switch->dev,
			"parse switch_en_direction_output error\n");
		return rc;
	}
	dp_switch->switch_mode =
			of_get_named_gpio(node, "qcom,switch-mode-gpio", 0);
	if (!gpio_is_valid(dp_switch->switch_mode)) {
		dev_err(dp_switch->dev, "TLMM switch mode gpio not found\n");
		return -EPROBE_DEFER;
	}
	rc = gpio_request(dp_switch->switch_mode, "USB_SWITCH_DP_MODE");
	if (rc < 0) {
		pr_err("[msm-dp-err]Failed to request GPIO:%d, ERRNO:%d",
				(s32)dp_switch->switch_en, rc);
		rc = -ENODEV;
	}
	rc = of_property_read_u32(node, "qcom,switch-mode-flag", &value);
	if (rc < 0) {
		dev_err(dp_switch->dev, "parse switch-en-flag error\n");
		return rc;
	} else {
		dp_switch->switch_mode_flag = value;
	}
	rc = gpio_direction_output(dp_switch->switch_mode,
			dp_switch->switch_mode_flag);
	if (rc) {
		pr_err("[msm-dp-err]Failed to set gpio %d to %d\n",
			dp_switch->switch_mode, dp_switch->switch_mode_flag);
		return rc;
	}
	pr_info("[msm-dp-info]%s:dp_switch->switch_en = %d,"
			" dp_switch->switch_mode = %d\n",
			__func__, dp_switch->switch_en, dp_switch->switch_mode);
	return rc;
}

static ssize_t dp_cc_statue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	switch (dp_switch->usb_mode) {
	case SWITCH_USBC_ORIENTATION_CC1:
		return sprintf(buf, "%d\n", 1);
		break;
	case SWITCH_USBC_ORIENTATION_CC2:
		return sprintf(buf, "%d\n", 2);
		break;
	}
	return sprintf(buf, "%d\n", 0);
}

static ssize_t dp_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", dp_switch->is_enabled);
}

static ssize_t dp_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	switch (val) {
	case 0:
	case 1:
		usb_switch_dp_enable(0);
		break;
	default:
		dev_err(dev, "Invalid argument\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, dp_enable_show, dp_enable_store);
static DEVICE_ATTR(cc_statue, S_IWUSR | S_IRUGO, dp_cc_statue_show, NULL);

static struct attribute *usb_switch_dp_attributes[] = {
		&dev_attr_enable.attr, &dev_attr_cc_statue.attr, NULL
};

static struct attribute_group usb_switch_dp_attribute_group = {
		.attrs = usb_switch_dp_attributes
};

static int usb_switch_dp_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret;
	struct device_node *node;

	pr_info("[msm-dp-info]%s: +\n", __func__);
	if (!pdev) {
		pr_err("[msm-dp-err]%s pdev is null\n", __func__);
		return -EPROBE_DEFER;
	}
	node = pdev->dev.of_node;
	dp_switch = kzalloc(sizeof(struct dp_switch_priv), GFP_KERNEL);
	if (!dp_switch) {
		dev_err(&pdev->dev, "cann't allocate device memory\n");
		return -ENOMEM;
	}
	dp_switch->dev = &pdev->dev;
	dp_switch->pdev = pdev;
	dp_switch->is_enabled = 0;
	rc = parse_usb_dp_switch_dt(node);
	if (rc < 0)
		return rc;
	ret = sysfs_create_group(&dp_switch->dev->kobj,
			&usb_switch_dp_attribute_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s error creating sysfs attr files\n",
				__func__);
	}
	pr_info("[msm-dp-info]%s: -\n", __func__);
	return 0;
}

static int usb_switch_dp_remove(struct platform_device *pdev)
{
	if (!dp_switch)
		return -EINVAL;
	sysfs_remove_group(&dp_switch->dev->kobj,
			&usb_switch_dp_attribute_group);
	return 0;
}

static const struct of_device_id of_match[] = {
	{ .compatible = "nubia,usb_switch_dp" },
	{}
};

static struct platform_driver usb_switch_dp_driver = {
	.probe = usb_switch_dp_probe,
	.remove = usb_switch_dp_remove,
	.driver = {
		.name = "nubia,usb_switch_dp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_match),
	},
};

int __init nubia_usb_switch_dp_init(void)
{
	platform_driver_register(&usb_switch_dp_driver);
	return 0;
}

void __exit nubia_usb_switch_dp_exit(void)
{
	platform_driver_unregister(&usb_switch_dp_driver);
}

MODULE_AUTHOR("NUBIA");
MODULE_DESCRIPTION("Nubia DisplayPort");
MODULE_LICENSE("GPL");
