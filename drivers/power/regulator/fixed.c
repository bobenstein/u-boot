/*
 *  Copyright (C) 2015 Samsung Electronics
 *
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <i2c.h>
#include <dm.h>
#include <asm/gpio.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <errno.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

struct fixed_regulator_priv {
	struct gpio_desc gpio;
	bool gpio_open_drain;
	bool enable_active_high;
	unsigned startup_delay_us;
};

static int fixed_regulator_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_regulator_info *info = dev->uclass_priv;
	struct fixed_regulator_priv *priv = dev->priv;
	int ret, offset = dev->of_offset;

	/* Get the basic regulator constraints */
	ret = regulator_ofdata_to_platdata(dev);
	if (ret) {
		error("Can't get regulator constraints for %s", dev->name);
		return ret;
	}

	/* Get fixed regulator gpio desc */
	ret = gpio_request_by_name_nodev(gd->fdt_blob, offset, "gpio", 0,
					 &priv->gpio, GPIOD_IS_OUT);
	if (ret) {
		error("Fixed regulator gpio - not found! Error: %d", ret);
		return ret;
	}

	/* Get fixed regulator addidional constraints */
	priv->gpio_open_drain = fdtdec_get_bool(gd->fdt_blob, offset,
						"gpio-open-drain");
	priv->enable_active_high = fdtdec_get_bool(gd->fdt_blob, offset,
						   "enable-active-high");
	priv->startup_delay_us = fdtdec_get_int(gd->fdt_blob, offset,
						"startup-delay-us", 0);

	/* Set type to fixed - used by regulator command */
	info->type = REGULATOR_TYPE_FIXED;

	debug("%s:%d\n", __func__, __LINE__);
	debug(" name:%s, boot_on:%d, active_hi: %d start_delay:%u\n",
		info->name, info->boot_on, priv->enable_active_high,
		priv->startup_delay_us);

	return 0;
}

static int fixed_regulator_get_value(struct udevice *dev)
{
	struct dm_regulator_info *info;
	int ret;

	ret = regulator_info(dev, &info);
	if (ret)
		return ret;

	if (info->min_uV == info->max_uV)
		return info->min_uV;

	error("Invalid constraints for: %s\n", info->name);

	return -EINVAL;
}

static bool fixed_regulator_get_enable(struct udevice *dev)
{
	struct fixed_regulator_priv *priv = dev->priv;

	return dm_gpio_get_value(&priv->gpio);
}

static int fixed_regulator_set_enable(struct udevice *dev, bool enable)
{
	struct fixed_regulator_priv *priv = dev->priv;
	int ret;

	ret = dm_gpio_set_value(&priv->gpio, enable);
	if (ret) {
		error("Can't set regulator : %s gpio to: %d\n", dev->name,
		      enable);
		return ret;
	}
	return 0;
}

static const struct dm_regulator_ops fixed_regulator_ops = {
	.get_value  = fixed_regulator_get_value,
	.get_enable = fixed_regulator_get_enable,
	.set_enable = fixed_regulator_set_enable,
};

static const struct udevice_id fixed_regulator_ids[] = {
	{ .compatible = "regulator-fixed" },
	{ },
};

U_BOOT_DRIVER(fixed_regulator) = {
	.name = "fixed regulator",
	.id = UCLASS_REGULATOR,
	.ops = &fixed_regulator_ops,
	.of_match = fixed_regulator_ids,
	.ofdata_to_platdata = fixed_regulator_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct fixed_regulator_priv),
};
