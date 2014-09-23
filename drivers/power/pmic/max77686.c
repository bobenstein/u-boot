/*
 *  Copyright (C) 2012 Samsung Electronics
 *  Rajeshwari Shinde <rajeshwari.s@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <i2c.h>
#include <power/pmic.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <dm/lists.h>
#include <power/max77686_pmic.h>
#include <errno.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

static int max77686_ofdata_to_platdata(struct udevice *dev)
{
	struct pmic_platdata *pl = dev->platdata;
	const void *blob = gd->fdt_blob;
	int node = dev->of_offset;
	int parent;

	pl->interface = PMIC_I2C;
	pl->regs_num = PMIC_NUM_OF_REGS;

	parent = fdt_parent_offset(blob, node);

	if (parent < 0) {
		error("%s: Cannot find node parent", __func__);
		return -EINVAL;
	}

	pl->bus = i2c_get_bus_num_fdt(parent);
	if (pl->bus < 0) {
		debug("%s: Cannot find bus num\n", __func__);
		return -EINVAL;
	}

	pl->hw.i2c.addr = fdtdec_get_int(blob, node, "reg", MAX77686_I2C_ADDR);
	pl->hw.i2c.tx_num = 1;

	return 0;
}

static int max77686_probe(struct udevice *parent)
{
	struct udevice *reg_dev;
	struct driver *reg_drv;
	int ret;

	reg_drv = lists_driver_lookup_name("max77686 regulator");
	if (!reg_drv) {
		error("%s regulator driver not found!\n", parent->name);
		return 0;
	}

	if (!parent->platdata) {
		error("%s platdata not found!\n", parent->name);
		return -EINVAL;
	}

	ret = device_bind(parent, reg_drv, parent->name, parent->platdata,
			  parent->of_offset, &reg_dev);
	if (ret)
		error("%s regulator bind failed", parent->name);

	/* Return error only if no parent platdata set */
	return 0;
}

static const struct udevice_id max77686_ids[] = {
	{ .compatible = "maxim,max77686_pmic"},
	{ }
};

U_BOOT_DRIVER(pmic_max77686) = {
	.name = "max77686 pmic",
	.id = UCLASS_PMIC,
	.of_match = max77686_ids,
	.probe = max77686_probe,
	.ofdata_to_platdata = max77686_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct pmic_platdata),
};
