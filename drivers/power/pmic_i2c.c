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
#include <i2c.h>

int pmic_i2c_reg_write(struct udevice *dev, unsigned reg, unsigned val)
{
	struct pmic_platdata *p;
	unsigned char buf[4] = { 0 };

	if (!dev)
		return -ENODEV;

	p = dev->platdata;

	if (pmic_check_reg(p, reg))
		return -EINVAL;

	I2C_SET_BUS(p->bus);

	switch (pmic_i2c_tx_num) {
	case 3:
		if (p->byte_order == PMIC_SENSOR_BYTE_ORDER_BIG) {
			buf[2] = (cpu_to_le32(val) >> 16) & 0xff;
			buf[1] = (cpu_to_le32(val) >> 8) & 0xff;
			buf[0] = cpu_to_le32(val) & 0xff;
		} else {
			buf[0] = (cpu_to_le32(val) >> 16) & 0xff;
			buf[1] = (cpu_to_le32(val) >> 8) & 0xff;
			buf[2] = cpu_to_le32(val) & 0xff;
		}
		break;
	case 2:
		if (p->byte_order == PMIC_SENSOR_BYTE_ORDER_BIG) {
			buf[1] = (cpu_to_le32(val) >> 8) & 0xff;
			buf[0] = cpu_to_le32(val) & 0xff;
		} else {
			buf[0] = (cpu_to_le32(val) >> 8) & 0xff;
			buf[1] = cpu_to_le32(val) & 0xff;
		}
		break;
	case 1:
		buf[0] = cpu_to_le32(val) & 0xff;
		break;
	default:
		printf("%s: invalid tx_num: %d", __func__, pmic_i2c_tx_num);
		return -1;
	}

	if (i2c_write(pmic_i2c_addr, reg, 1, buf, pmic_i2c_tx_num))
		return -1;

	return 0;
}

int pmic_i2c_reg_read(struct udevice *dev, unsigned reg, unsigned *val)
{
	struct pmic_platdata *p;
	unsigned char buf[4] = { 0 };
	u32 ret_val = 0;

	if (!dev)
		return -ENODEV;

	p = dev->platdata;

	if (pmic_check_reg(p, reg))
		return -EINVAL;

	I2C_SET_BUS(p->bus);

	if (i2c_read(pmic_i2c_addr, reg, 1, buf, pmic_i2c_tx_num))
		return -1;

	switch (pmic_i2c_tx_num) {
	case 3:
		if (p->byte_order == PMIC_SENSOR_BYTE_ORDER_BIG)
			ret_val = le32_to_cpu(buf[2] << 16
					      | buf[1] << 8 | buf[0]);
		else
			ret_val = le32_to_cpu(buf[0] << 16 |
					      buf[1] << 8 | buf[2]);
		break;
	case 2:
		if (p->byte_order == PMIC_SENSOR_BYTE_ORDER_BIG)
			ret_val = le32_to_cpu(buf[1] << 8 | buf[0]);
		else
			ret_val = le32_to_cpu(buf[0] << 8 | buf[1]);
		break;
	case 1:
		ret_val = le32_to_cpu(buf[0]);
		break;
	default:
		printf("%s: invalid tx_num: %d", __func__, pmic_i2c_tx_num);
		return -1;
	}
	memcpy(val, &ret_val, sizeof(ret_val));

	return 0;
}

int pmic_i2c_probe(struct udevice *dev)
{
	struct pmic_platdata *p;

	if (!dev)
		return -ENODEV;

	p = dev->platdata;

	i2c_set_bus_num(p->bus);
	debug("Bus: %d PMIC:%s probed!\n", p->bus, dev->name);
	if (i2c_probe(pmic_i2c_addr)) {
		printf("Can't find PMIC:%s\n", dev->name);
		return -1;
	}

	return 0;
}
