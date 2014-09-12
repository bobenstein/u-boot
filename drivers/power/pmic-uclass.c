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
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <dm/lists.h>
#include <compiler.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

int pmic_get(char *name, struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	*devp = NULL;

	for (ret = uclass_first_device(UCLASS_PMIC, &dev);
	     dev;
	     ret = uclass_next_device(&dev)) {
		if (!strcmp(name, dev->name)) {
			*devp = dev;
			return ret;
		}
	}

	return -ENODEV;
}

int pmic_reg_count(struct udevice *dev)
{
	const struct dm_pmic_ops *ops;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC);
	if (!ops)
		return -ENODEV;

	return ops->reg_count;
}

int pmic_read(struct udevice *dev, uint reg, uint8_t *buffer, int len)
{
	const struct dm_pmic_ops *ops;

	if (!buffer)
		return -EFAULT;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC);
	if (!ops)
		return -ENODEV;

	if (!ops->read)
		return -EPERM;

	if (ops->read(dev, reg, buffer, len))
		return -EIO;

	return 0;
}

int pmic_write(struct udevice *dev, uint reg, uint8_t *buffer, int len)
{
	const struct dm_pmic_ops *ops;

	if (!buffer)
		return -EFAULT;

	ops = pmic_get_uclass_ops(dev, UCLASS_PMIC);
	if (!ops)
		return -ENODEV;

	if (!ops->write)
		return -EPERM;

	if (ops->write(dev, reg, buffer, len))
		return -EIO;

	return 0;
}

int pmic_child_node_scan(struct udevice *parent,
			 const char *optional_subnode,
			 const char *compatible_property_name)
{
	const void *blob = gd->fdt_blob;
	struct udevice *childp;
	int offset = parent->of_offset;
	int childs = 0;
	int ret;

	debug("%s: bind child for %s\n", __func__, parent->name);

	if (optional_subnode) {
		debug("Looking for %s optional subnode: %s \n", parent->name,
		      optional_subnode);
		offset = fdt_subnode_offset(blob, offset, optional_subnode);
		if (offset <= 0) {
			debug("Pmic: %s subnode: %s not found!",
			      parent->name, optional_subnode);
			return -ENXIO;
		}
	}

	for (offset = fdt_first_subnode(blob, offset); offset > 0;
	     offset = fdt_next_subnode(blob, offset)) {
		ret = lists_bind_fdt_by_prop(parent, blob, offset,
					     compatible_property_name, &childp);
		if (ret)
			continue;

		childs++;
	}

	return childs;
}

UCLASS_DRIVER(pmic) = {
	.id		= UCLASS_PMIC,
	.name		= "pmic",
};
