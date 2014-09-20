/*
 * Copyright (C) 2014 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <linux/types.h>
#include <fdtdec.h>
#include <power/pmic.h>
#include <i2c.h>
#include <compiler.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

int pmic_ldo_cnt(struct udevice *dev)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_ldo_cnt)
		return -EPERM;

	return ops->get_ldo_cnt(dev);
}

int pmic_buck_cnt(struct udevice *dev)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->get_buck_cnt)
		return -EPERM;

	return ops->get_buck_cnt(dev);
}

struct regulator_desc *pmic_ldo_desc(struct udevice *dev, int ldo)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return NULL;

	if (!ops->get_val_desc)
		return NULL;

	return ops->get_val_desc(dev, DESC_TYPE_LDO, ldo);
}

struct regulator_desc *pmic_buck_desc(struct udevice *dev, int buck)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return NULL;

	if (!ops->get_val_desc)
		return NULL;

	return ops->get_val_desc(dev, DESC_TYPE_BUCK, buck);
}

struct regulator_mode_desc *pmic_ldo_mode_desc(struct udevice *dev, int ldo,
					       int *mode_cnt)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return NULL;

	if (!ops->get_mode_desc_array)
		return NULL;

	return ops->get_mode_desc_array(dev, DESC_TYPE_LDO, ldo, mode_cnt);
}

struct regulator_mode_desc *pmic_buck_mode_desc(struct udevice *dev, int buck,
						int *mode_cnt)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return NULL;

	if (!ops->get_mode_desc_array)
		return NULL;

	return ops->get_mode_desc_array(dev, DESC_TYPE_BUCK, buck, mode_cnt);
}

int pmic_get_ldo_val(struct udevice *dev, int ldo)
{
	const struct dm_regulator_ops *ops;
	int val = -1;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->ldo_val)
		return -EPERM;

	if (ops->ldo_val(dev, PMIC_OP_GET, ldo, &val))
		return -EIO;

	return val;
}

int pmic_set_ldo_val(struct udevice *dev, int ldo, int val)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->ldo_val)
		return -EPERM;

	if (ops->ldo_val(dev, PMIC_OP_SET, ldo, &val))
		return -EIO;

	return 0;
}

int pmic_get_ldo_mode(struct udevice *dev, int ldo)
{
	const struct dm_regulator_ops *ops;
	int mode;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->ldo_mode)
		return -EPERM;

	if (ops->ldo_mode(dev, PMIC_OP_GET, ldo, &mode))
		return -EIO;

	return mode;
}

int pmic_set_ldo_mode(struct udevice *dev, int ldo, int mode)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->ldo_mode)
		return -EPERM;

	if (ops->ldo_mode(dev, PMIC_OP_SET, ldo, &mode))
		return -EIO;

	return 0;
}

int pmic_get_buck_val(struct udevice *dev, int buck)
{
	const struct dm_regulator_ops *ops;
	int val;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->buck_val)
		return -EPERM;

	if (ops->buck_val(dev, PMIC_OP_GET, buck, &val))
		return -EIO;

	return val;
}

int pmic_set_buck_val(struct udevice *dev, int buck, int val)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->buck_val)
		return -EPERM;

	if (ops->buck_val(dev, PMIC_OP_SET, buck, &val))
		return -EIO;

	return 0;
}

int pmic_get_buck_mode(struct udevice *dev, int buck)
{
	const struct dm_regulator_ops *ops;
	int mode;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->buck_mode)
		return -EPERM;

	if (ops->buck_mode(dev, PMIC_OP_GET, buck, &mode))
		return -EIO;

	return mode;
}

int pmic_set_buck_mode(struct udevice *dev, int buck, int mode)
{
	const struct dm_regulator_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC_REGULATOR);
	if (!ops)
		return -ENODEV;

	if (!ops->buck_mode)
		return -EPERM;

	if (ops->buck_mode(dev, PMIC_OP_SET, buck, &mode))
		return -EIO;

	return 0;
}

UCLASS_DRIVER(pmic_regulator) = {
	.id		= UCLASS_PMIC_REGULATOR,
	.name		= "regulator",
};
