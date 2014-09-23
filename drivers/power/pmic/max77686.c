/*
 *  Copyright (C) 2014-2015 Samsung Electronics
 *  Przemyslaw Marczak  <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <i2c.h>
#include <dm.h>
#include <power/pmic.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <dm/lists.h>
#include <power/max77686_pmic.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

static int max77686_write(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	if (dm_i2c_write(dev, reg, buff, len)) {
		error("write error to device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int max77686_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	if (dm_i2c_read(dev, reg, buff, len)) {
		error("read error from device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int max77686_bind(struct udevice *pmic)
{
	int ret;

	ret = pmic_child_node_scan(pmic, "voltage-regulators",
					 "regulator-compatible");
	if (ret < 0)
		debug("%s: %s subnode scan error: %d.\n", __func__, pmic->name,
							  ret);
	else
		debug("%s: %s subnode found %d childs.\n", __func__, pmic->name,
							   ret);

	/* Always return success for this device */
	return 0;
}

static struct dm_pmic_ops max77686_ops = {
	.reg_count = MAX77686_NUM_OF_REGS,
	.read = max77686_read,
	.write = max77686_write,
};

static const struct udevice_id max77686_ids[] = {
	{ .compatible = "maxim,max77686"},
	{ }
};

U_BOOT_DRIVER(pmic_max77686) = {
	.name = "max77686 pmic",
	.id = UCLASS_PMIC,
	.of_match = max77686_ids,
	.bind = max77686_bind,
	.ops = &max77686_ops,
};
