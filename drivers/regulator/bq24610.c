/*
 * Support for TI bq24610 (bqTINY-II) Dual Input (USB/AC Adpater)
 * 1-Cell Li-Ion Charger connected via GPIOs.
 *
 * Copyright (c) 2008 Philipp Zabel
 * Copyright (c) 2011 Jens Andersen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/regulator/bq24610.h>
#include <linux/regulator/driver.h>


static int bq24610_enable(struct regulator_dev *rdev)
{
	struct bq24610_mach_info *pdata = rdev_get_drvdata(rdev);

	dev_dbg(rdev_get_dev(rdev), "enabling charger\n");

	gpio_set_value(pdata->gpio_nce, 0);
	return 0;
}

static int bq24610_disable(struct regulator_dev *rdev)
{
	struct bq24610_mach_info *pdata = rdev_get_drvdata(rdev);

	dev_dbg(rdev_get_dev(rdev), "disabling charger\n");

	gpio_set_value(pdata->gpio_nce, 1);
	return 0;
}

static int bq24610_is_enabled(struct regulator_dev *rdev)
{
	struct bq24610_mach_info *pdata = rdev_get_drvdata(rdev);

	return !gpio_get_value(pdata->gpio_nce);
}

static struct regulator_ops bq24610_ops = {
/*	.set_current_limit = bq24610_set_current_limit,
	.get_current_limit = bq24610_get_current_limit,*/
	.enable            = bq24610_enable,
	.disable           = bq24610_disable,
	.is_enabled        = bq24610_is_enabled,
};

static struct regulator_desc bq24610_desc = {
	.name  = "bq24610",
	.ops   = &bq24610_ops,
	.type  = REGULATOR_CURRENT,
	.owner = THIS_MODULE,
};

static int __init bq24610_probe(struct platform_device *pdev)
{
	struct bq24610_mach_info *pdata = pdev->dev.platform_data;
	struct regulator_dev *bq24610;
	int ret;

	if (!pdata || !pdata->gpio_nce)
		return -EINVAL;

	ret = gpio_request(pdata->gpio_nce, "ncharge_en");
	if (ret) {
		dev_dbg(&pdev->dev, "couldn't request nCE GPIO: %d\n",
			pdata->gpio_nce);
		goto err_ce;
	}

	ret = gpio_direction_output(pdata->gpio_nce, 1);

	bq24610 = regulator_register(&bq24610_desc, &pdev->dev,
				     pdata->init_data, pdata);
	if (IS_ERR(bq24610)) {
		dev_dbg(&pdev->dev, "couldn't register regulator\n");
		ret = PTR_ERR(bq24610);
		goto err_reg;
	}
	platform_set_drvdata(pdev, bq24610);
	dev_dbg(&pdev->dev, "registered regulator\n");

	return 0;
err_reg:
	gpio_free(pdata->gpio_nce);
err_ce:
	return ret;
}

static int __devexit bq24610_remove(struct platform_device *pdev)
{
	struct bq24610_mach_info *pdata = pdev->dev.platform_data;
	struct regulator_dev *bq24610 = platform_get_drvdata(pdev);

	regulator_unregister(bq24610);
	gpio_free(pdata->gpio_nce);

	return 0;
}

static struct platform_driver bq24610_driver = {
	.driver = {
		.name = "bq24610",
	},
	.remove = __devexit_p(bq24610_remove),
};

static int __init bq24610_init(void)
{
	return platform_driver_probe(&bq24610_driver, bq24610_probe);
}

static void __exit bq24610_exit(void)
{
	platform_driver_unregister(&bq24610_driver);
}

module_init(bq24610_init);
module_exit(bq24610_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("TI bq24610 Li-Ion Charger driver");
MODULE_LICENSE("GPL");
