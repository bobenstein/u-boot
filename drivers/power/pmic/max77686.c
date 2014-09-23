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

static int max77686_write(struct udevice *pmic, unsigned reg, unsigned char val)
{
	unsigned char buf[4] = { 0 };

	buf[0] = cpu_to_le32(val) & 0xff;

	if (dm_i2c_write(pmic, reg, buf, 1)) {
		error("write error to device: %p register: %#x!", pmic, reg);
		return -EIO;
	}

	return 0;
}

static int max77686_read(struct udevice *pmic, unsigned reg, unsigned char *val)
{
	unsigned char buf[4] = { 0 };

	if (dm_i2c_read(pmic, reg, buf, 1)) {
		error("read error from device: %p register: %#x!", pmic, reg);
		return -EIO;
	}

	*val = le32_to_cpu(buf[0]);

	return 0;
}

static int max77686_bind(struct udevice *pmic)
{
	struct dm_i2c_chip *i2c_chip = dev_get_parent_platdata(pmic);
	struct pmic_platdata *pl = pmic->platdata;
	struct udevice *reg_dev;
	struct driver *reg_drv;
	int ret;

	/* The same platdata is used for the regulator driver */
	pl->if_type = PMIC_I2C;
	pl->if_bus_num = pmic->parent->req_seq;
	pl->if_addr_cs = i2c_chip->chip_addr;
	pl->if_max_offset = MAX77686_NUM_OF_REGS;

	reg_drv = lists_driver_lookup_name("max77686 regulator");
	if (!reg_drv) {
		error("PMIC: %s regulator driver not found!\n", pmic->name);
		return 0;
	}

	/**
	 * Try bind the child regulator driver
	 * |-- I2C bus
	 *     '---max77686 pmic I/O driver
	 *         '--max77686 regulator driver
	 */
	ret = device_bind(pmic, reg_drv, pmic->name, pmic->platdata,
			  pmic->of_offset, &reg_dev);
	if (ret)
		error("%s regulator bind failed", pmic->name);

	/* Always return success for this device */
	return 0;
}

static struct dm_pmic_ops max77686_ops = {
	.read = max77686_read,
	.write = max77686_write,
};

static const struct udevice_id max77686_ids[] = {
	{ .compatible = "maxim,max77686_pmic"},
	{ }
};

U_BOOT_DRIVER(pmic_max77686) = {
	.name = "max77686 pmic",
	.id = UCLASS_PMIC,
	.of_match = max77686_ids,
	.bind = max77686_bind,
	.ops = &max77686_ops,
	.platdata_auto_alloc_size = sizeof(struct pmic_platdata),
};
