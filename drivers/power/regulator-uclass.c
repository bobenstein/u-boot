/*
 * Copyright (C) 2014-2015 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <linux/types.h>
#include <fdtdec.h>
#include <dm.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <compiler.h>
#include <dm/device.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

int regulator_info(struct udevice *dev, struct dm_regulator_info **infop)
{
	if (!dev || !dev->uclass_priv)
		return -ENODEV;

	*infop = dev->uclass_priv;

	return 0;
}

int regulator_mode(struct udevice *dev, struct dm_regulator_mode **modep)
{
	struct dm_regulator_info *info;
	int ret;

	ret = regulator_info(dev, &info);
	if (ret)
		return ret;

	*modep = info->mode;
	return info->mode_count;
}

int regulator_get_value(struct udevice *dev)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_value)
		return -EPERM;

	return ops->get_value(dev);
}

int regulator_set_value(struct udevice *dev, int uV)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_value)
		return -EPERM;

	return ops->set_value(dev, uV);
}

int regulator_get_current(struct udevice *dev)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_current)
		return -EPERM;

	return ops->get_current(dev);
}

int regulator_set_current(struct udevice *dev, int uA)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_current)
		return -EPERM;

	return ops->set_current(dev, uA);
}

bool regulator_get_enable(struct udevice *dev)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_enable)
		return -EPERM;

	return ops->get_enable(dev);
}

int regulator_set_enable(struct udevice *dev, bool enable)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_enable)
		return -EPERM;

	return ops->set_enable(dev, enable);
}

int regulator_get_mode(struct udevice *dev)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_mode)
		return -EPERM;

	return ops->get_mode(dev);
}

int regulator_set_mode(struct udevice *dev, int mode)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_mode)
		return -EPERM;

	return ops->set_mode(dev, mode);
}

int regulator_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_regulator_info *info = dev->uclass_priv;
	int offset = dev->of_offset;
	int len;

	/* Mandatory constraints */
	info->name = strdup(fdt_getprop(gd->fdt_blob, offset,
					"regulator-name", &len));
	if (!info->name)
		return -ENXIO;

	info->min_uV = fdtdec_get_int(gd->fdt_blob, offset,
				      "regulator-min-microvolt", -1);
	if (info->min_uV < 0)
		return -ENXIO;

	info->max_uV = fdtdec_get_int(gd->fdt_blob, offset,
				      "regulator-max-microvolt", -1);
	if (info->max_uV < 0)
		return -ENXIO;

	/* Optional constraints */
	info->min_uA = fdtdec_get_int(gd->fdt_blob, offset,
				      "regulator-min-microamp", -1);
	info->max_uA = fdtdec_get_int(gd->fdt_blob, offset,
				      "regulator-max-microamp", -1);
	info->always_on = fdtdec_get_bool(gd->fdt_blob, offset,
					 "regulator-always-on");
	info->boot_on = fdtdec_get_bool(gd->fdt_blob, offset,
					 "regulator-boot-on");

	return 0;
}

int regulator_get(char *name, struct udevice **devp)
{
	struct dm_regulator_info *info;
	struct udevice *dev;
	int ret;

	*devp = NULL;

	for (ret = uclass_first_device(UCLASS_REGULATOR, &dev);
	     dev;
	     ret = uclass_next_device(&dev)) {
		info = dev->uclass_priv;
		if (!info)
			continue;

		if (!strcmp(name, info->name)) {
			*devp = dev;
			return ret;
		}
	}

	return -ENODEV;
}

UCLASS_DRIVER(regulator) = {
	.id		= UCLASS_REGULATOR,
	.name		= "regulator",
	.per_device_auto_alloc_size = sizeof(struct dm_regulator_info),
};
