/*
 * Copyright (C) 2014 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * Copyright (C) 2011 Samsung Electronics
 * Lukasz Majewski <l.majewski@samsung.com>
 *
 * (C) Copyright 2010
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de
 *
 * (C) Copyright 2008-2009 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <linux/types.h>
#include <power/pmic.h>
#include <errno.h>
#include <dm.h>
#include <spi.h>

static struct spi_slave *slave;

void pmic_spi_free(struct spi_slave *slave)
{
	if (slave)
		spi_free_slave(slave);
}

struct spi_slave *pmic_spi_probe(struct udevice *dev)
{
	struct pmic_platdata *p;

	if (!dev)
		return NULL;

	p = dev->platdata;

	if (pmic_check_reg(p, reg))
		return NULL;

	return spi_setup_slave(p->bus,
		p->hw.spi.cs,
		p->hw.spi.clk,
		p->hw.spi.mode);
}

static int pmic_spi_reg(struct udevice *dev, unsigned reg, unsigned *val,
			int write)
{
	struct pmic_platdata *p;
	u32 pmic_tx, pmic_rx;
	u32 tmp;

	if (!dev)
		return -EINVAL;

	p = dev->platdata;

	if (pmic_check_reg(p, reg))
		return -EFAULT;

	if (!slave) {
		slave = pmic_spi_probe(p);

		if (!slave)
			return -ENODEV;
	}

	if (pmic_check_reg(p, reg))
		return -EFAULT;

	if (spi_claim_bus(slave))
		return -EIO;

	pmic_tx = p->hw.spi.prepare_tx(reg, val, write);

	tmp = cpu_to_be32(pmic_tx);

	if (spi_xfer(slave, pmic_spi_bitlen, &tmp, &pmic_rx,
		     pmic_spi_flags)) {
		spi_release_bus(slave);
		return -EIO;
	}

	if (write) {
		pmic_tx = p->hw.spi.prepare_tx(reg, val, 0);
		tmp = cpu_to_be32(pmic_tx);
		if (spi_xfer(slave, pmic_spi_bitlen, &tmp, &pmic_rx,
			     pmic_spi_flags)) {
			spi_release_bus(slave);
			return -EIO;
		}
	}

	spi_release_bus(slave);
	*val = cpu_to_be32(pmic_rx);

	return 0;
}

int pmic_spi_reg_write(struct udevice *dev, unsigned reg, unsigned val)
{
	struct pmic_platdata *p;

	if (!dev)
		return -EINVAL;

	p = dev->platdata;

	if (pmic_check_reg(p, reg))
		return -EFAULT;

	if (pmic_spi_reg(p, reg, &val, 1))
		return -1;

	return 0;
}

int pmic_spi_reg_read(struct udevice *dev, unsigned reg, unsigned *val)
{
	struct pmic_platdata *p;

	if (!dev)
		return -EINVAL;

	p = dev->platdata;

	if (pmic_check_reg(p, reg))
		return -EFAULT;

	if (pmic_spi_reg(p, reg, val, 0))
		return -1;

	return 0;
}
