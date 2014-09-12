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

static char * const pmic_interfaces[] = {
	"I2C",
	"SPI",
	"--",
};

const char *pmic_if_str(struct udevice *pmic)
{
	int if_types = ARRAY_SIZE(pmic_interfaces);
	int if_type;

	if_type = pmic_if_type(pmic);
	if (if_type < 0 || if_type >= if_types)
		return pmic_interfaces[if_types - 1];

	return pmic_interfaces[if_type];
}

int pmic_if_type(struct udevice *pmic)
{
	struct pmic_platdata *pl = pmic->platdata;

	return pl->if_type;
}

int pmic_if_bus_num(struct udevice *pmic)
{
	struct pmic_platdata *pl = pmic->platdata;

	return pl->if_bus_num;
}

int pmic_if_addr_cs(struct udevice *pmic)
{
	struct pmic_platdata *pl = pmic->platdata;

	return pl->if_addr_cs;
}

int pmic_if_max_offset(struct udevice *pmic)
{
	struct pmic_platdata *pl = pmic->platdata;

	return pl->if_max_offset;
}

int pmic_read(struct udevice *pmic, unsigned reg, unsigned char *val)
{
	const struct dm_pmic_ops *ops;

	ops = pmic_get_uclass_ops(pmic, UCLASS_PMIC);
	if (!ops)
		return -ENODEV;

	if (!ops->read)
		return -EPERM;

	if (ops->read(pmic, reg, val))
		return -EIO;

	return 0;
}

int pmic_write(struct udevice *pmic, unsigned reg, unsigned char val)
{
	const struct dm_pmic_ops *ops;

	ops = pmic_get_uclass_ops(pmic, UCLASS_PMIC);
	if (!ops)
		return -ENODEV;

	if (!ops->write)
		return -EPERM;

	if (ops->write(pmic, reg, val))
		return -EIO;

	return 0;
}

int pmic_get(char *name, struct udevice **pmic)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_PMIC, &uc);
	if (ret) {
		error("PMIC uclass not initialized!");
		return ret;
	}

	uclass_foreach_dev(dev, uc) {
		if (!dev)
			continue;

		if (!strncmp(name, dev->name, strlen(name))) {
			ret = device_probe(dev);
			if (ret)
				error("Dev: %s probe failed", dev->name);
			*pmic = dev;
			return ret;
		}
	}

	*pmic = NULL;
	return -EINVAL;
}

int pmic_i2c_get(int bus, int addr, struct udevice **pmic)
{
	struct udevice *dev;
	int ret;

	ret = i2c_get_chip_for_busnum(bus, addr, 1, &dev);
	if (ret) {
		error("Chip: %d on bus %d not found!", addr, bus);
		*pmic = NULL;
		return ret;
	}

	*pmic = dev;
	return 0;
}

int pmic_spi_get(int bus, int cs, struct udevice **pmic)
{
	struct udevice *busp, *devp;
	int ret;

	ret = spi_find_bus_and_cs(bus, cs, &busp, &devp);
	if (ret) {
		error("Chip: %d on bus %d not found!", cs, bus);
		*pmic = NULL;
		return ret;
	}

	*pmic = devp;
	return 0;
}

int pmic_io_dev(struct udevice *pmic, struct udevice **pmic_io)
{
	struct pmic_platdata *pl;
	int ret;

	pl = dev_get_platdata(pmic);
	if (!pl) {
		error("pmic: %s platdata not found!", pmic->name);
		return -EINVAL;
	}

	switch (pl->if_type) {
	case PMIC_I2C:
		ret = pmic_i2c_get(pl->if_bus_num, pl->if_addr_cs, pmic_io);
		break;
	case PMIC_SPI:
		ret = pmic_spi_get(pl->if_bus_num, pl->if_addr_cs, pmic_io);
		break;
	default:
		error("Unsupported interface for: %s", pmic->name);
		ret = -ENXIO;
	}

	return ret;
}

UCLASS_DRIVER(pmic) = {
	.id		= UCLASS_PMIC,
	.name		= "pmic",
};
