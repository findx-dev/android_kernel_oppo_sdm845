/************************************************************************************
** Copyright (C), 2008-2018, OPPO Mobile Comm Corp., Ltd
** VENDOR_EDIT
** File: oppo_proximity_power.c
**
** Description:
**      OPPO proximity power control driver.
**************************************************************************************/
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#include "oppo_proximity_power.h"

static int proximity_power_set(struct oppo_proximity_chip *chip, bool enable)
{
	struct pinctrl_state *state;
	int ret;

	state = enable ? chip->power_high : chip->power_low;
	ret = pinctrl_select_state(chip->pctrl, state);
	if (ret)
		dev_err(chip->dev, "failed to set proximity power %s: %d\n",
			enable ? "high" : "low", ret);

	return ret;
}

static ssize_t oppo_proximity_power_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct oppo_proximity_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			gpio_get_value(chip->power_gpio));
}

static ssize_t oppo_proximity_power_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct oppo_proximity_chip *chip = dev_get_drvdata(dev);
	unsigned int enable;
	int ret;

	ret = kstrtouint(buf, 10, &enable);
	if (ret)
		return ret;
	if (enable > 1)
		return -EINVAL;

	ret = proximity_power_set(chip, enable);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,
		   oppo_proximity_power_show, oppo_proximity_power_store);

static struct attribute *oppo_proximity_power_attrs[] = {
	&dev_attr_enable.attr,
	NULL
};

static const struct attribute_group oppo_proximity_power_attr_group = {
	.attrs = oppo_proximity_power_attrs,
};

static int oppo_proximity_power_probe(struct platform_device *pdev)
{
	struct oppo_proximity_chip *chip;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	chip->pctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR(chip->pctrl))
		return PTR_ERR(chip->pctrl);

	chip->power_low = pinctrl_lookup_state(chip->pctrl,
					       "power_gpio_low");
	if (IS_ERR(chip->power_low))
		return PTR_ERR(chip->power_low);

	chip->power_high = pinctrl_lookup_state(chip->pctrl,
						"power_gpio_high");
	if (IS_ERR(chip->power_high))
		return PTR_ERR(chip->power_high);

	chip->power_gpio = of_get_named_gpio(chip->dev->of_node,
					     "qcom,power_gpio", 0);
	if (!gpio_is_valid(chip->power_gpio))
		return chip->power_gpio;

	ret = devm_gpio_request(chip->dev, chip->power_gpio,
				"proximity_power_gpio");
	if (ret)
		return ret;

	ret = proximity_power_set(chip, true);
	if (ret)
		return ret;

	ret = sysfs_create_group(&chip->dev->kobj,
				 &oppo_proximity_power_attr_group);
	if (ret)
		return ret;

	dev_info(chip->dev, "proximity power control ready on GPIO %u\n",
		 chip->power_gpio);
	return 0;
}

static int oppo_proximity_power_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj,
			   &oppo_proximity_power_attr_group);
	return 0;
}

static const struct of_device_id oppo_proximity_power_of_match[] = {
	{ .compatible = "proximity_power" },
	{ }
};
MODULE_DEVICE_TABLE(of, oppo_proximity_power_of_match);

static struct platform_driver oppo_proximity_power_driver = {
	.probe = oppo_proximity_power_probe,
	.remove = oppo_proximity_power_remove,
	.driver = {
		.name = "proximity_power",
		.of_match_table = oppo_proximity_power_of_match,
	},
};
module_platform_driver(oppo_proximity_power_driver);

MODULE_DESCRIPTION("OPPO proximity power control driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("mofei");
