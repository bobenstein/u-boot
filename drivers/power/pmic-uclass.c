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
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <dm/lists.h>
#include <i2c.h>
#include <compiler.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

int pmic_check_reg(struct pmic_platdata *p, unsigned reg)
{
	if (!p)
		return -ENODEV;

	if (reg >= p->regs_num) {
		error("<reg num> = %d is invalid. Should be less than %d",
		       reg, p->regs_num);
		return -EINVAL;
	}

	return 0;
}

int pmic_reg_write(struct udevice *dev, unsigned reg, unsigned val)
{
	struct pmic_platdata *p;

	p = dev_get_platdata(dev);
	if (!p)
		return -EPERM;

	switch (p->interface) {
	case PMIC_I2C:
#ifdef CONFIG_DM_PMIC_I2C
		return pmic_i2c_reg_write(dev, reg, val);
#else
		return -ENOSYS;
#endif
	case PMIC_SPI:
#ifdef CONFIG_DM_PMIC_SPI
		return pmic_spi_reg_write(dev, reg, val);
#else
		return -ENOSYS;
#endif
	default:
		return -ENODEV;
	}
}

int pmic_reg_read(struct udevice *dev, unsigned reg, unsigned *val)
{
	struct pmic_platdata *p;

	p = dev_get_platdata(dev);
	if (!p)
		return -EPERM;

	switch (p->interface) {
	case PMIC_I2C:
#ifdef CONFIG_DM_PMIC_I2C
		return pmic_i2c_reg_read(dev, reg, val);
#else
		return -ENOSYS;
#endif
	case PMIC_SPI:
#ifdef CONFIG_DM_PMIC_SPI
		return pmic_spi_reg_read(dev, reg, val);
#else
		return -ENOSYS;
#endif
	default:
		return -ENODEV;
	}
}

int pmic_probe(struct udevice *dev, struct spi_slave *spi_slave)
{
	struct pmic_platdata *p;

	p = dev_get_platdata(dev);
	if (!p)
		return -EPERM;

	switch (p->interface) {
	case PMIC_I2C:
#ifdef CONFIG_DM_PMIC_I2C
		return pmic_i2c_probe(dev);
#else
		return -ENOSYS;
#endif
	case PMIC_SPI:
		if (!spi_slave)
			return -EINVAL;
#ifdef CONFIG_DM_PMIC_SPI
		spi_slave = pmic_spi_probe(dev);
		if (!spi_slave)
			return -EIO;

		return 0;
#else
		return -ENOSYS;
#endif
	default:
		return -ENODEV;
	}
}

struct udevice *pmic_get_by_interface(int uclass_id, int bus, int addr_cs)
{
	struct pmic_platdata *pl;
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	if (uclass_id < 0 || uclass_id >= UCLASS_COUNT) {
		error("Bad uclass id.\n");
		return NULL;
	}

	ret = uclass_get(uclass_id, &uc);
	if (ret) {
		error("PMIC uclass: %d not initialized!\n", uclass_id);
		return NULL;
	}

	uclass_foreach_dev(dev, uc) {
		if (!dev || !dev->platdata)
			continue;

		pl = dev_get_platdata(dev);

		if (pl->bus != bus)
			continue;

		switch (pl->interface) {
		case PMIC_I2C:
			if (addr_cs != pl->hw.i2c.addr)
				continue;
			break;
		case PMIC_SPI:
			if (addr_cs != pl->hw.spi.cs)
				continue;
			break;
		default:
			error("Unsupported interface of: %s", dev->name);
			return NULL;
		}

		ret = device_probe(dev);
		if (ret) {
			error("Dev: %s probe failed", dev->name);
			return NULL;
		}
		return dev;
	}

	return NULL;
}

struct udevice *pmic_get_by_name(int uclass_id, char *name)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	if (uclass_id < 0 || uclass_id >= UCLASS_COUNT) {
		error("Bad uclass id.\n");
		return NULL;
	}

	ret = uclass_get(uclass_id, &uc);
	if (ret) {
		error("PMIC uclass: %d not initialized!", uclass_id);
		return NULL;
	}

	uclass_foreach_dev(dev, uc) {
		if (!dev)
			continue;

		if (!strncmp(name, dev->name, strlen(name))) {
			ret = device_probe(dev);
			if (ret)
				error("Dev: %s probe failed", dev->name);
			return dev;
		}
	}

	return NULL;
}

int pmic_init_dm(void)
{
	const void *blob = gd->fdt_blob;
	const struct fdt_property *prop;
	struct udevice *dev = NULL;
	const char *path;
	const char *alias;
	int alias_node, node, offset, ret = 0;
	int alias_len;
	int len;

	alias = "pmic";
	alias_len = strlen(alias);

	alias_node = fdt_path_offset(blob, "/aliases");
	offset = fdt_first_property_offset(blob, alias_node);

	if (offset < 0) {
		error("Alias node not found.");
		return -ENODEV;
	}

	offset = fdt_first_property_offset(blob, alias_node);
	for (; offset > 0; offset = fdt_next_property_offset(blob, offset)) {
		prop = fdt_get_property_by_offset(blob, offset, &len);
		if (!len)
			continue;

		path = fdt_string(blob, fdt32_to_cpu(prop->nameoff));

		if (!strncmp(alias, path, alias_len))
			node = fdt_path_offset(blob, prop->data);
		else
			node = 0;

		if (node <= 0)
			continue;

		ret = lists_bind_fdt(gd->dm_root, blob, node, &dev);
		if (ret < 0)
			continue;

		if (device_probe(dev))
			error("Device: %s, probe error", dev->name);
	}

	return 0;
}

UCLASS_DRIVER(pmic) = {
	.id		= UCLASS_PMIC,
	.name		= "pmic",
};
