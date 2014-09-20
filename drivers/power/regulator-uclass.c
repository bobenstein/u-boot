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

int regulator_get_cnt(struct udevice *dev, int type, int *cnt)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_cnt)
		return -EPERM;

	return ops->get_cnt(dev, type, cnt);
}

int regulator_get_value_desc(struct udevice *dev, int type, int number,
			     struct regulator_value_desc **desc)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENXIO;

	if (!ops->get_value_desc)
		return -EPERM;

	return ops->get_value_desc(dev, type, number, desc);
}

int regulator_get_mode_desc(struct udevice *dev, int type, int number,
			    int *mode_cnt, struct regulator_mode_desc **desc)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENXIO;

	if (!ops->get_mode_desc_array)
		return -EPERM;

	return ops->get_mode_desc_array(dev, type, number, mode_cnt, desc);
}

int regulator_get_value(struct udevice *dev, int type, int number, int *value)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_value)
		return -EPERM;

	return ops->get_value(dev, type, number, value);
}

int regulator_set_value(struct udevice *dev, int type, int number, int value)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_value)
		return -EPERM;

	return ops->set_value(dev, type, number, value);
}

int regulator_get_state(struct udevice *dev, int type, int number, int *state)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_state)
		return -EPERM;

	return ops->get_state(dev, type, number, state);
}

int regulator_set_state(struct udevice *dev, int type, int number, int state)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_state)
		return -EPERM;

	return ops->set_state(dev, type, number, state);
}

int regulator_get_mode(struct udevice *dev, int type, int number, int *mode)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_mode)
		return -EPERM;

	return ops->get_mode(dev, type, number, mode);
}

int regulator_set_mode(struct udevice *dev, int type, int number, int mode)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->set_mode)
		return -EPERM;

	return ops->set_mode(dev, type, number, mode);
}

int regulator_get(char *name, struct udevice **regulator)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_PMIC_REGULATOR, &uc);
	if (ret) {
		error("Regulator uclass not initialized!");
		return ret;
	}

	uclass_foreach_dev(dev, uc) {
		if (!dev)
			continue;

		if (!strncmp(name, dev->name, strlen(name))) {
			ret = device_probe(dev);
			if (ret)
				error("Dev: %s probe failed", dev->name);
			*regulator = dev;
			return ret;
		}
	}

	*regulator = NULL;
	return -EINVAL;
}

int regulator_i2c_get(int bus, int addr, struct udevice **regulator)
{
	struct udevice *pmic;
	int ret;

	/* First get parent pmic device */
	ret = pmic_i2c_get(bus, addr, &pmic);
	if (ret) {
		error("Chip: %d on bus %d not found!", addr, bus);
		return ret;
	}

	/* Get the first regulator child of pmic device */
	if (device_get_first_child_by_uclass_id(pmic, UCLASS_PMIC_REGULATOR,
						regulator)) {
		error("PMIC: %s regulator child not found!", pmic->name);
		*regulator = NULL;
		return ret;
	}

	return 0;
}

int regulator_spi_get(int bus, int cs, struct udevice **regulator)
{
	struct udevice *pmic;
	int ret;

	/* First get parent pmic device */
	ret = pmic_spi_get(bus, cs, &pmic);
	if (ret) {
		error("Chip: %d on bus %d not found!", cs, bus);
		return ret;
	}

	/* Get the first regulator child of pmic device */
	if (device_get_first_child_by_uclass_id(pmic, UCLASS_PMIC_REGULATOR,
						regulator)) {
		error("PMIC: %s regulator child not found!", pmic->name);
		*regulator = NULL;
		return ret;
	}

	return 0;
}

UCLASS_DRIVER(regulator) = {
	.id		= UCLASS_PMIC_REGULATOR,
	.name		= "regulator",
};
