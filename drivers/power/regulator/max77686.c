/*
 *  Copyright (C) 2012-2015 Samsung Electronics
 *
 *  Rajeshwari Shinde <rajeshwari.s@samsung.com>
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <i2c.h>
#include <dm.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/max77686_pmic.h>
#include <errno.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

#define MODE(_id, _val, _name) { \
	.id = _id, \
	.register_value = _val, \
	.name = _name, \
}

/* LDO: 1,3,4,5,9,17,18,19,20,21,22,23,24,26,26,27 */
static struct dm_regulator_mode max77686_ldo_mode_standby1[] = {
	MODE(OPMODE_OFF, MAX77686_LDO_MODE_OFF, "OFF"),
	MODE(OPMODE_LPM, MAX77686_LDO_MODE_LPM, "LPM"),
	MODE(OPMODE_STANDBY_LPM, MAX77686_LDO_MODE_STANDBY_LPM, "ON/LPM"),
	MODE(OPMODE_ON, MAX77686_LDO_MODE_ON, "ON"),
};

/* LDO: 2,6,7,8,10,11,12,14,15,16 */
static struct dm_regulator_mode max77686_ldo_mode_standby2[] = {
	MODE(OPMODE_OFF, MAX77686_LDO_MODE_OFF, "OFF"),
	MODE(OPMODE_STANDBY, MAX77686_LDO_MODE_STANDBY, "ON/OFF"),
	MODE(OPMODE_STANDBY_LPM, MAX77686_LDO_MODE_STANDBY_LPM, "ON/LPM"),
	MODE(OPMODE_ON, MAX77686_LDO_MODE_ON, "ON"),
};

/* Buck: 1 */
static struct dm_regulator_mode max77686_buck_mode_standby[] = {
	MODE(OPMODE_OFF, MAX77686_BUCK_MODE_OFF, "OFF"),
	MODE(OPMODE_STANDBY, MAX77686_BUCK_MODE_STANDBY, "ON/OFF"),
	MODE(OPMODE_ON, MAX77686_BUCK_MODE_ON, "ON"),
};

/* Buck: 2,3,4 */
static struct dm_regulator_mode max77686_buck_mode_lpm[] = {
	MODE(OPMODE_OFF, MAX77686_BUCK_MODE_OFF, "OFF"),
	MODE(OPMODE_STANDBY, MAX77686_BUCK_MODE_STANDBY, "ON/OFF"),
	MODE(OPMODE_LPM, MAX77686_BUCK_MODE_LPM, "LPM"),
	MODE(OPMODE_ON, MAX77686_BUCK_MODE_ON, "ON"),
};

/* Buck: 5,6,7,8,9 */
static struct dm_regulator_mode max77686_buck_mode_onoff[] = {
	MODE(OPMODE_OFF, MAX77686_BUCK_MODE_OFF, "OFF"),
	MODE(OPMODE_ON, MAX77686_BUCK_MODE_ON, "ON"),
};

static const char max77686_buck_addr[] = {
	0xff, 0x10, 0x12, 0x1c, 0x26, 0x30, 0x32, 0x34, 0x36, 0x38
};

static int max77686_buck_volt2hex(int buck, int uV)
{
	unsigned int hex = 0;
	unsigned int hex_max = 0;

	switch (buck) {
	case 2:
	case 3:
	case 4:
		/* hex = (uV - 600000) / 12500; */
		hex = (uV - MAX77686_BUCK_UV_LMIN) / MAX77686_BUCK_UV_LSTEP;
		hex_max = MAX77686_BUCK234_VOLT_MAX_HEX;
		/**
		 * This uses voltage scaller - temporary not implemented
		 * so return just 0
		 */
		return 0;
	default:
		/* hex = (uV - 750000) / 50000; */
		hex = (uV - MAX77686_BUCK_UV_HMIN) / MAX77686_BUCK_UV_HSTEP;
		hex_max = MAX77686_BUCK_VOLT_MAX_HEX;
		break;
	}

	if (hex >= 0 && hex <= hex_max)
		return hex;

	error("Value: %d uV is wrong for BUCK%d", uV, buck);
	return -EINVAL;
}

static int max77686_buck_hex2volt(int buck, int hex)
{
	unsigned uV = 0;
	unsigned int hex_max = 0;

	if (hex < 0)
		goto bad_hex;

	switch (buck) {
	case 2:
	case 3:
	case 4:
		hex_max = MAX77686_BUCK234_VOLT_MAX_HEX;
		if (hex > hex_max)
			goto bad_hex;

		/* uV = hex * 12500 + 600000; */
		uV = hex * MAX77686_BUCK_UV_LSTEP + MAX77686_BUCK_UV_LMIN;
		break;
	default:
		hex_max = MAX77686_BUCK_VOLT_MAX_HEX;
		if (hex > hex_max)
			goto bad_hex;

		/* uV = hex * 50000 + 750000; */
		uV = hex * MAX77686_BUCK_UV_HSTEP + MAX77686_BUCK_UV_HMIN;
		break;
	}

	return uV;

bad_hex:
	error("Value: %#x is wrong for BUCK%d", hex, buck);
	return -EINVAL;
}

static int max77686_ldo_volt2hex(int ldo, int uV)
{
	unsigned int hex = 0;

	switch (ldo) {
	case 1:
	case 2:
	case 6:
	case 7:
	case 8:
	case 15:
		hex = (uV - MAX77686_LDO_UV_MIN) / MAX77686_LDO_UV_LSTEP;
		/* hex = (uV - 800000) / 25000; */
		break;
	default:
		hex = (uV - MAX77686_LDO_UV_MIN) / MAX77686_LDO_UV_HSTEP;
		/* hex = (uV - 800000) / 50000; */
	}

	if (hex >= 0 && hex <= MAX77686_LDO_VOLT_MAX_HEX)
		return hex;

	error("Value: %d uV is wrong for LDO%d", uV, ldo);
	return -EINVAL;
}

static int max77686_ldo_hex2volt(int ldo, int hex)
{
	unsigned int uV = 0;

	if (hex > MAX77686_LDO_VOLT_MAX_HEX)
		goto bad_hex;

	switch (ldo) {
	case 1:
	case 2:
	case 6:
	case 7:
	case 8:
	case 15:
		/* uV = hex * 25000 + 800000; */
		uV = hex * MAX77686_LDO_UV_LSTEP + MAX77686_LDO_UV_MIN;
		break;
	default:
		/* uV = hex * 50000 + 800000; */
		uV = hex * MAX77686_LDO_UV_HSTEP + MAX77686_LDO_UV_MIN;
	}

	return uV;

bad_hex:
	error("Value: %#x is wrong for ldo%d", hex, ldo);
	return -EINVAL;
}

static int max77686_ldo_hex2mode(int ldo, int hex)
{
	if (hex > MAX77686_LDO_MODE_MASK)
		return -EINVAL;

	switch (hex) {
	case MAX77686_LDO_MODE_OFF:
		return OPMODE_OFF;
	case MAX77686_LDO_MODE_LPM: /* == MAX77686_LDO_MODE_STANDBY: */
		/* The same mode values but different meaning for each ldo */
		switch (ldo) {
		case 2:
		case 6:
		case 7:
		case 8:
		case 10:
		case 11:
		case 12:
		case 14:
		case 15:
		case 16:
			return OPMODE_STANDBY;
		default:
			return OPMODE_LPM;
		}
	case MAX77686_LDO_MODE_STANDBY_LPM:
		return OPMODE_STANDBY_LPM;
	case MAX77686_LDO_MODE_ON:
		return OPMODE_ON;
	default:
		return -EINVAL;
	}
}

static int max77686_buck_hex2mode(int buck, int hex)
{
	if (hex > MAX77686_BUCK_MODE_MASK)
		return -EINVAL;

	switch (hex) {
	case MAX77686_BUCK_MODE_OFF:
		return OPMODE_OFF;
	case MAX77686_BUCK_MODE_ON:
		return OPMODE_ON;
	case MAX77686_BUCK_MODE_STANDBY:
		switch (buck) {
		case 1:
		case 2:
		case 3:
		case 4:
			return OPMODE_STANDBY;
		default:
			return -EINVAL;
		}
	case MAX77686_BUCK_MODE_LPM:
		switch (buck) {
		case 2:
		case 3:
		case 4:
			return OPMODE_LPM;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static int max77686_buck_modes(int buck, struct dm_regulator_mode **modesp)
{
	int ret = -EINVAL;

	if (buck < 1 || buck > MAX77686_BUCK_NUM)
		return ret;

	switch (buck) {
	case 1:
		*modesp = max77686_buck_mode_standby;
		ret = ARRAY_SIZE(max77686_buck_mode_standby);
	case 2:
	case 3:
	case 4:
		*modesp = max77686_buck_mode_lpm;
		ret = ARRAY_SIZE(max77686_buck_mode_lpm);
	default:
		*modesp = max77686_buck_mode_onoff;
		ret = ARRAY_SIZE(max77686_buck_mode_onoff);
	}

	return ret;
}

static int max77686_ldo_modes(int ldo, struct dm_regulator_mode **modesp)
{
	int ret = -EINVAL;

	if (ldo < 1 || ldo > MAX77686_LDO_NUM)
		return ret;

	switch (ldo) {
	case 2:
	case 6:
	case 7:
	case 8:
	case 10:
	case 11:
	case 12:
	case 14:
	case 15:
	case 16:
		*modesp = max77686_ldo_mode_standby2;
		ret = ARRAY_SIZE(max77686_ldo_mode_standby2);
	default:
		*modesp = max77686_ldo_mode_standby1;
		ret = ARRAY_SIZE(max77686_ldo_mode_standby1);
	}

	return ret;
}

static int dev2num(struct udevice *dev)
{
	return dev->of_id->data;
}

static int max77686_ldo_val(struct udevice *dev, int op, int *uV)
{
	unsigned int ret, hex, adr;
	unsigned char val;
	int ldo;

	if (op == PMIC_OP_GET)
		*uV = 0;

	ldo = dev2num(dev);
	if (ldo < 1 || ldo > MAX77686_LDO_NUM) {
		error("Wrong ldo number: %d", ldo);
		return -EINVAL;
	}

	adr = MAX77686_REG_PMIC_LDO1CTRL1 + ldo - 1;

	ret = pmic_read(dev->parent, adr, &val, 1);
	if (ret)
		return ret;

	if (op == PMIC_OP_GET) {
		val &= MAX77686_LDO_VOLT_MASK;
		ret = max77686_ldo_hex2volt(ldo, val);
		if (ret < 0)
			return ret;
		*uV = ret;
		return 0;
	}

	hex = max77686_ldo_volt2hex(ldo, *uV);
	if (hex < 0)
		return -EINVAL;

	val &= ~MAX77686_LDO_VOLT_MASK;
	val |= hex;
	ret = pmic_write(dev->parent, adr, &val, 1);

	return ret;
}

static int max77686_buck_val(struct udevice *dev, int op, int *uV)
{
	unsigned int hex, ret, mask, adr;
	unsigned char val;
	int buck;

	buck = dev2num(dev);
	if (buck < 1 || buck > MAX77686_BUCK_NUM) {
		error("Wrong buck number: %d", buck);
		return -EINVAL;
	}

	if (op == PMIC_OP_GET)
		*uV = 0;

	/* &buck_out = ctrl + 1 */
	adr = max77686_buck_addr[buck] + 1;

	/* mask */
	switch (buck) {
	case 2:
	case 3:
	case 4:
		/* Those uses voltage scallers - will support in the future */
		mask = MAX77686_BUCK234_VOLT_MASK;
		return -EPERM;
	default:
		mask = MAX77686_BUCK_VOLT_MASK;
	}

	ret = pmic_read(dev->parent, adr, &val, 1);
	if (ret)
		return ret;

	if (op == PMIC_OP_GET) {
		val &= mask;
		ret = max77686_buck_hex2volt(buck, val);
		if (ret < 0)
			return ret;
		*uV = ret;
		return 0;
	}

	hex = max77686_buck_volt2hex(buck, *uV);
	if (hex < 0)
		return -EINVAL;

	val &= ~mask;
	val |= hex;
	ret = pmic_write(dev->parent, adr, &val, 1);

	return ret;
}

static int max77686_ldo_mode(struct udevice *dev, int op, int *opmode)
{
	unsigned int ret, adr, mode;
	unsigned char val;
	int ldo;

	if (op == PMIC_OP_GET)
		*opmode = -EINVAL;

	ldo = dev2num(dev);
	if (ldo < 1 || ldo > MAX77686_LDO_NUM) {
		error("Wrong ldo number: %d", ldo);
		return -EINVAL;
	}

	adr = MAX77686_REG_PMIC_LDO1CTRL1 + ldo - 1;

	ret = pmic_read(dev->parent, adr, &val, 1);
	if (ret)
		return ret;

	if (op == PMIC_OP_GET) {
		val &= MAX77686_LDO_MODE_MASK;
		ret = max77686_ldo_hex2mode(ldo, val);
		if (ret < 0)
			return ret;
		*opmode = ret;
		return 0;
	}

	/* mode */
	switch (*opmode) {
	case OPMODE_OFF:
		mode = MAX77686_LDO_MODE_OFF;
		break;
	case OPMODE_LPM:
		switch (ldo) {
		case 2:
		case 6:
		case 7:
		case 8:
		case 10:
		case 11:
		case 12:
		case 14:
		case 15:
		case 16:
			return -EINVAL;
		default:
			mode = MAX77686_LDO_MODE_LPM;
		}
		break;
	case OPMODE_STANDBY:
		switch (ldo) {
		case 2:
		case 6:
		case 7:
		case 8:
		case 10:
		case 11:
		case 12:
		case 14:
		case 15:
		case 16:
			mode = MAX77686_LDO_MODE_STANDBY;
			break;
		default:
			return -EINVAL;
		}
		break;
	case OPMODE_STANDBY_LPM:
		mode = MAX77686_LDO_MODE_STANDBY_LPM;
		break;
	case OPMODE_ON:
		mode = MAX77686_LDO_MODE_ON;
		break;
	default:
		mode = 0xff;
	}

	if (mode == 0xff) {
		error("Wrong mode: %d for ldo%d", *opmode, ldo);
		return -EINVAL;
	}

	val &= ~MAX77686_LDO_MODE_MASK;
	val |= mode;
	ret = pmic_write(dev->parent, adr, &val, 1);

	return ret;
}

static int max77686_ldo_enable(struct udevice *dev, int op, bool *enable)
{
	int ret, on_off;

	if (op == PMIC_OP_GET) {
		ret = max77686_ldo_mode(dev, op, &on_off);
		if (ret)
			return ret;

		switch (on_off) {
		case OPMODE_OFF:
			*enable = 0;
			break;
		case OPMODE_ON:
			*enable = 1;
			break;
		default:
			return -EINVAL;
		}
	} else if (op == PMIC_OP_SET) {
		switch (*enable) {
		case 0:
			on_off = OPMODE_OFF;
			break;
		case 1:
			on_off = OPMODE_ON;
			break;
		default:
			return -EINVAL;
		}

		ret = max77686_ldo_mode(dev, op, &on_off);
		if (ret)
			return ret;
	}

	return 0;
}

static int max77686_buck_mode(struct udevice *dev, int op, int *opmode)
{
	unsigned int ret, mask, adr, mode, mode_shift;
	unsigned char val;
	int buck;

	buck = dev2num(dev);
	if (buck < 1 || buck > MAX77686_BUCK_NUM) {
		error("Wrong buck number: %d", buck);
		return -EINVAL;
	}

	adr = max77686_buck_addr[buck];

	/* mask */
	switch (buck) {
	case 2:
	case 3:
	case 4:
		mode_shift = MAX77686_BUCK_MODE_SHIFT_2;
		break;
	default:
		mode_shift = MAX77686_BUCK_MODE_SHIFT_1;
	}

	mask = MAX77686_BUCK_MODE_MASK << mode_shift;

	ret = pmic_read(dev->parent, adr, &val, 1);
	if (ret)
		return ret;

	if (op == PMIC_OP_GET) {
		val &= mask;
		val >>= mode_shift;
		ret = max77686_buck_hex2mode(buck, val);
		if (ret < 0)
			return ret;
		*opmode = ret;
		return 0;
	}

	/* mode */
	switch (*opmode) {
	case OPMODE_OFF:
		mode = MAX77686_BUCK_MODE_OFF;
		break;
	case OPMODE_STANDBY:
		switch (buck) {
		case 1:
		case 2:
		case 3:
		case 4:
			mode = MAX77686_BUCK_MODE_STANDBY << mode_shift;
			break;
		default:
			mode = 0xff;
		}
		break;
	case OPMODE_LPM:
		switch (buck) {
		case 2:
		case 3:
		case 4:
			mode = MAX77686_BUCK_MODE_LPM << mode_shift;
			break;
		default:
			mode = 0xff;
		}
		break;
	case OPMODE_ON:
		mode = MAX77686_BUCK_MODE_ON << mode_shift;
		break;
	default:
		mode = 0xff;
	}

	if (mode == 0xff) {
		error("Wrong mode: %d for buck: %d\n", *opmode, buck);
		return -EINVAL;
	}

	val &= ~mask;
	val |= mode;
	ret = pmic_write(dev->parent, adr, &val, 1);

	return ret;
}

static int max77686_buck_enable(struct udevice *dev, int op, bool *enable)
{
	int ret, on_off;

	if (op == PMIC_OP_GET) {
		ret = max77686_buck_mode(dev, op, &on_off);
		if (ret)
			return ret;

		switch (on_off) {
		case OPMODE_OFF:
			*enable = false;
			break;
		case OPMODE_ON:
			*enable = true;
			break;
		default:
			return -EINVAL;
		}
	} else if (op == PMIC_OP_SET) {
		switch (*enable) {
		case 0:
			on_off = OPMODE_OFF;
			break;
		case 1:
			on_off = OPMODE_ON;
			break;
		default:
			return -EINVAL;
		}

		ret = max77686_buck_mode(dev, op, &on_off);
		if (ret)
			return ret;
	}

	return 0;
}

static int max77686_ldo_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_regulator_info *info = dev->uclass_priv;
	int ret;

	ret = regulator_ofdata_to_platdata(dev);
	if (ret)
		return ret;

	info->type = REGULATOR_TYPE_LDO;
	info->mode_count = max77686_ldo_modes(dev2num(dev), &info->mode);

	return 0;
}

static int ldo_get_value(struct udevice *dev)
{
	int uV;
	int ret;

	ret = max77686_ldo_val(dev, PMIC_OP_GET, &uV);
	if (ret)
		return ret;

	return uV;
}

static int ldo_set_value(struct udevice *dev, int uV)
{
	return max77686_ldo_val(dev, PMIC_OP_SET, &uV);
}

static bool ldo_get_enable(struct udevice *dev)
{
	bool enable = false;
	int ret;

	ret = max77686_ldo_enable(dev, PMIC_OP_GET, &enable);
	if (ret)
		return ret;

	return enable;
}

static int ldo_set_enable(struct udevice *dev, bool enable)
{
	return max77686_ldo_enable(dev, PMIC_OP_SET, &enable);
}

static int ldo_get_mode(struct udevice *dev)
{
	int mode;
	int ret;

	ret = max77686_ldo_mode(dev, PMIC_OP_GET, &mode);
	if (ret)
		return ret;

	return mode;
}

static int ldo_set_mode(struct udevice *dev, int mode)
{
	return max77686_ldo_mode(dev, PMIC_OP_SET, &mode);
}

static int max77686_buck_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_regulator_info *info = dev->uclass_priv;
	int ret;

	ret = regulator_ofdata_to_platdata(dev);
	if (ret)
		return ret;

	info->type = REGULATOR_TYPE_BUCK;
	info->mode_count = max77686_buck_modes(dev2num(dev), &info->mode);

	return 0;
}

static int buck_get_value(struct udevice *dev)
{
	int uV;
	int ret;

	ret = max77686_buck_val(dev, PMIC_OP_GET, &uV);
	if (ret)
		return ret;

	return uV;
}

static int buck_set_value(struct udevice *dev, int uV)
{
	return max77686_buck_val(dev, PMIC_OP_SET, &uV);
}

static bool buck_get_enable(struct udevice *dev)
{
	bool enable = false;
	int ret;

	ret = max77686_buck_enable(dev, PMIC_OP_GET, &enable);
	if (ret)
		return ret;

	return enable;
}

static int buck_set_enable(struct udevice *dev, bool enable)
{
	return max77686_buck_enable(dev, PMIC_OP_SET, &enable);
}

static int buck_get_mode(struct udevice *dev)
{
	int mode;
	int ret;

	ret = max77686_buck_mode(dev, PMIC_OP_GET, &mode);
	if (ret)
		return ret;

	return mode;
}

static int buck_set_mode(struct udevice *dev, int mode)
{
	return max77686_buck_mode(dev, PMIC_OP_SET, &mode);
}

static const struct dm_regulator_ops max77686_ldo_ops = {
	.get_value  = ldo_get_value,
	.set_value  = ldo_set_value,
	.get_enable = ldo_get_enable,
	.set_enable = ldo_set_enable,
	.get_mode   = ldo_get_mode,
	.set_mode   = ldo_set_mode,
};

static const struct udevice_id max77686_ldo_ids[] = {
	{ .compatible = "LDO1", .data = 1 },
	{ .compatible = "LDO2", .data = 2 },
	{ .compatible = "LDO3", .data = 3 },
	{ .compatible = "LDO4", .data = 4 },
	{ .compatible = "LDO5", .data = 5 },
	{ .compatible = "LDO6", .data = 6 },
	{ .compatible = "LDO7", .data = 7 },
	{ .compatible = "LDO8", .data = 8 },
	{ .compatible = "LDO9", .data = 9 },
	{ .compatible = "LDO10", .data = 10 },
	{ .compatible = "LDO11", .data = 11 },
	{ .compatible = "LDO12", .data = 12 },
	{ .compatible = "LDO13", .data = 13 },
	{ .compatible = "LDO14", .data = 14 },
	{ .compatible = "LDO15", .data = 15 },
	{ .compatible = "LDO16", .data = 16 },
	{ .compatible = "LDO17", .data = 17 },
	{ .compatible = "LDO18", .data = 18 },
	{ .compatible = "LDO19", .data = 19 },
	{ .compatible = "LDO20", .data = 20 },
	{ .compatible = "LDO21", .data = 21 },
	{ .compatible = "LDO22", .data = 22 },
	{ .compatible = "LDO23", .data = 23 },
	{ .compatible = "LDO24", .data = 24 },
	{ .compatible = "LDO25", .data = 25 },
	{ .compatible = "LDO26", .data = 26 },
	{ },
};

U_BOOT_DRIVER(max77686_ldo) = {
	.name = "max77686 ldo",
	.id = UCLASS_REGULATOR,
	.ops = &max77686_ldo_ops,
	.of_match = max77686_ldo_ids,
	.ofdata_to_platdata = max77686_ldo_ofdata_to_platdata,
};

static const struct dm_regulator_ops max77686_buck_ops = {
	.get_value  = buck_get_value,
	.set_value  = buck_set_value,
	.get_enable = buck_get_enable,
	.set_enable = buck_set_enable,
	.get_mode   = buck_get_mode,
	.set_mode   = buck_set_mode,
};

static const struct udevice_id max77686_buck_ids[] = {
	{ .compatible = "BUCK1", .data = 1 },
	{ .compatible = "BUCK2", .data = 2 },
	{ .compatible = "BUCK3", .data = 3 },
	{ .compatible = "BUCK4", .data = 4 },
	{ .compatible = "BUCK5", .data = 5 },
	{ .compatible = "BUCK6", .data = 6 },
	{ .compatible = "BUCK7", .data = 7 },
	{ .compatible = "BUCK8", .data = 8 },
	{ .compatible = "BUCK9", .data = 9 },
	{ },
};

U_BOOT_DRIVER(max77686_buck) = {
	.name = "max77686 buck",
	.id = UCLASS_REGULATOR,
	.ops = &max77686_buck_ops,
	.of_match = max77686_buck_ids,
	.ofdata_to_platdata = max77686_buck_ofdata_to_platdata,
};
