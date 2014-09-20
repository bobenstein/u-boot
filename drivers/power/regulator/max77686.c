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
#include <power/max77686_pmic.h>
#include <errno.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

struct max77686_regulator_info {
	int ldo_cnt;
	int buck_cnt;
	struct regulator_desc ldo_desc[MAX77686_LDO_NUM + 1];
	struct regulator_desc buck_desc[MAX77686_BUCK_NUM + 1];
};

#define MODE(_mode, _val, _name) { \
	.mode = _mode, \
	.reg_val = _val, \
	.name = _name, \
}

/* LDO: 1,3,4,5,9,17,18,19,20,21,22,23,24,26,26,27 */
static struct regulator_mode_desc max77686_ldo_mode_standby1[] = {
	MODE(OPMODE_OFF, MAX77686_LDO_MODE_OFF, "OFF"),
	MODE(OPMODE_LPM, MAX77686_LDO_MODE_LPM, "LPM"),
	MODE(OPMODE_STANDBY_LPM, MAX77686_LDO_MODE_STANDBY_LPM, "ON/LPM"),
	MODE(OPMODE_ON, MAX77686_LDO_MODE_ON, "ON"),
};

/* LDO: 2,6,7,8,10,11,12,14,15,16 */
static struct regulator_mode_desc max77686_ldo_mode_standby2[] = {
	MODE(OPMODE_OFF, MAX77686_LDO_MODE_OFF, "OFF"),
	MODE(OPMODE_STANDBY, MAX77686_LDO_MODE_STANDBY, "ON/OFF"),
	MODE(OPMODE_STANDBY_LPM, MAX77686_LDO_MODE_STANDBY_LPM, "ON/LPM"),
	MODE(OPMODE_ON, MAX77686_LDO_MODE_ON, "ON"),
};

/* Buck: 1 */
static struct regulator_mode_desc max77686_buck_mode_standby[] = {
	MODE(OPMODE_OFF, MAX77686_BUCK_MODE_OFF, "OFF"),
	MODE(OPMODE_STANDBY, MAX77686_BUCK_MODE_STANDBY, "ON/OFF"),
	MODE(OPMODE_ON, MAX77686_BUCK_MODE_ON, "ON"),
};

/* Buck: 2,3,4 */
static struct regulator_mode_desc max77686_buck_mode_lpm[] = {
	MODE(OPMODE_OFF, MAX77686_BUCK_MODE_OFF, "OFF"),
	MODE(OPMODE_STANDBY, MAX77686_BUCK_MODE_STANDBY, "ON/OFF"),
	MODE(OPMODE_LPM, MAX77686_BUCK_MODE_LPM, "LPM"),
	MODE(OPMODE_ON, MAX77686_BUCK_MODE_ON, "ON"),
};

/* Buck: 5,6,7,8,9 */
static struct regulator_mode_desc max77686_buck_mode_onoff[] = {
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

static int max77686_get_ldo_cnt(struct udevice *dev)
{
	return MAX77686_LDO_NUM;
}

static int max77686_get_buck_cnt(struct udevice *dev)
{
	return MAX77686_BUCK_NUM;
}

struct regulator_desc *max77686_get_val_desc(struct udevice *dev, int d_type,
					     int d_num)
{
	struct max77686_regulator_info *dev_info;

	if (!dev)
		return NULL;

	if (!dev->priv)
		return NULL;

	dev_info = dev->priv;

	switch (d_type) {
	case DESC_TYPE_LDO:
		if (d_num < 1 || d_num > 26)
			return NULL;

		return &dev_info->ldo_desc[d_num];
	case DESC_TYPE_BUCK:
		if (d_num < 1 || d_num > 9)
			return NULL;

		return &dev_info->buck_desc[d_num];
	default:
		return NULL;
	}
}

static struct regulator_mode_desc *
max77686_get_mode_desc_array(struct udevice *dev, int d_type, int d_num,
			     int *d_mode_cnt)
{
	struct max77686_regulator_info *dev_info;

	if (!dev)
		return NULL;

	if (!dev->priv)
		return NULL;

	dev_info = dev->priv;
	*d_mode_cnt = 0;

	if (d_type == DESC_TYPE_LDO) {
		if (d_num < 1 || d_num > dev_info->ldo_cnt)
			return NULL;

		switch (d_num) {
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
			*d_mode_cnt = ARRAY_SIZE(max77686_ldo_mode_standby2);
			return max77686_ldo_mode_standby2;
		default:
			*d_mode_cnt = ARRAY_SIZE(max77686_ldo_mode_standby1);
			return max77686_ldo_mode_standby1;
		}
	} else if (d_type == DESC_TYPE_BUCK) {
		if (d_num < 1 || d_num > dev_info->buck_cnt)
			return NULL;

		switch (d_num) {
		case 1:
			*d_mode_cnt = ARRAY_SIZE(max77686_buck_mode_standby);
			return max77686_buck_mode_standby;
		case 2:
		case 3:
		case 4:
			*d_mode_cnt = ARRAY_SIZE(max77686_buck_mode_lpm);
			return max77686_buck_mode_lpm;
		default:
			*d_mode_cnt = ARRAY_SIZE(max77686_buck_mode_onoff);
			return max77686_buck_mode_onoff;
		}
	}

	return NULL;
}

static int max77686_ldo_val(struct udevice *dev, int op,
				int ldo, int *uV)
{
	unsigned int val, ret, hex, adr;

	if (op == PMIC_OP_GET)
		*uV = 0;

	if (ldo < 1 || ldo > 26) {
		debug("%s: %d is wrong ldo number\n", __func__, ldo);
		return -EINVAL;
	}

	adr = MAX77686_REG_PMIC_LDO1CTRL1 + ldo - 1;

	ret = pmic_reg_read(dev, adr, &val);
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
	ret |= pmic_reg_write(dev, adr, val);

	return ret;
}

static int max77686_buck_val(struct udevice *dev, int op,
				int buck, int *uV)
{
	unsigned int val, hex, ret, mask, adr;

	if (buck < 1 || buck > 9) {
		debug("%s: %d is wrong buck number\n", __func__, buck);
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

	ret = pmic_reg_read(dev, adr, &val);
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
	ret = pmic_reg_write(dev, adr, val);

	return ret;
}

static int max77686_ldo_mode(struct udevice *dev, int op, int ldo, int *opmode)
{
	unsigned int val, ret, adr, mode;

	if (op == PMIC_OP_GET)
		*opmode = -EINVAL;

	if (ldo < 1 || ldo > 26) {
		debug("%s: %d is wrong ldo number\n", __func__, ldo);
		return -EINVAL;
	}

	adr = MAX77686_REG_PMIC_LDO1CTRL1 + ldo - 1;

	ret = pmic_reg_read(dev, adr, &val);
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
		debug("%s: %d is not supported on LDO%d\n", __func__, *opmode,
							    ldo);
		return -EINVAL;
	}

	val &= ~MAX77686_LDO_MODE_MASK;
	val |= mode;
	ret |= pmic_reg_write(dev, adr, val);

	return ret;
}

static int max77686_buck_mode(struct udevice *dev, int op, int buck,
			      int *opmode)
{
	unsigned int val, ret, mask, adr, mode, mode_shift;

	if (buck < 1 || buck > 9) {
		debug("%s: %d is wrong buck number\n", __func__, buck);
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

	ret = pmic_reg_read(dev, adr, &val);
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
		debug("%s: %d is not supported on BUCK%d\n", __func__, *opmode,
							     buck);
		return -EINVAL;
	}

	val &= ~mask;
	val |= mode;
	ret |= pmic_reg_write(dev, adr, val);

	return ret;
}

int fill_reg_desc(int offset, struct regulator_desc *desc, int reg_num,
		  int desc_type)
{
	const void *blob = gd->fdt_blob;
	char *reg_name;
	int reg_min, reg_max, len;

	desc->type = desc_type;
	desc->number = reg_num;

	reg_name = (char *)fdt_getprop(blob, offset, "regulator-name", &len);
	if (reg_name) {
		strncpy(&desc->name[0], reg_name, DESC_NAME_LEN);
		desc->name[DESC_NAME_LEN - 1] = '\0';
	} else {
		sprintf(&desc->name[0], "-");
	}

	reg_min = fdtdec_get_int(blob, offset, "regulator-min-microvolt", -1);
	desc->min_uV = reg_min;

	reg_max = fdtdec_get_int(blob, offset, "regulator-min-microvolt", -1);
	desc->max_uV = reg_max;

	return 0;
}

static int get_desc_for_compat(int regulators_node, char *compat, int desc_type,
			       struct regulator_desc *desc, int desc_num)
{
	const void *blob = gd->fdt_blob;
	const char *reg_compat;
	int compat_len = strlen(compat);
	int reg_num;
	int offset;
	int cnt = 0;

	/* First subnode of "voltage_regulators" node */
	offset = fdt_first_subnode(blob, regulators_node);

	while (offset > 0) {
		/* Get regulator properties */
		reg_compat = (char *)fdt_getprop(blob, offset,
			     "regulator-compatible", NULL);

		/* Check compat and compare the number only */
		reg_num = 0;
		if (strstr(reg_compat, compat))
			reg_num = simple_strtoul(reg_compat + compat_len,
						 NULL, 0);
		else
			goto next_offset;

		if (reg_num && reg_num <= desc_num)
			fill_reg_desc(offset, &desc[reg_num], reg_num,
				      desc_type);

		cnt++;
		if (cnt == desc_num)
			return cnt;

next_offset:
		offset = fdt_next_subnode(blob, offset);
	}

	return cnt;
}

static int reg_max77686_ofdata_to_platdata(struct udevice *dev)
{
	struct max77686_regulator_info *dev_info = dev->priv;
	const void *blob = gd->fdt_blob;
	int node = dev->of_offset;
	int offset;

	if (!dev->platdata) {
		error("Device %s: no platdata", dev->name);
		return 0;
	}

	if (node < 0) {
		error("Device: %s - bad of_offset", dev->name);
		return 0;
	}

	offset = fdt_subnode_offset(blob, node,  "voltage-regulators");
	if (offset < 0) {
		error("Node %s has no 'voltage-regulators' subnode", dev->name);
		return 0;
	}

	dev_info->ldo_cnt = get_desc_for_compat(offset, "LDO", DESC_TYPE_LDO,
						dev_info->ldo_desc,
						MAX77686_LDO_NUM);

	dev_info->buck_cnt = get_desc_for_compat(offset, "BUCK", DESC_TYPE_BUCK,
						 dev_info->buck_desc,
						 MAX77686_BUCK_NUM);

	/* Even if there is no voltage regulators node in dts,
	 * always return success and allow the driver to bind.
	 * Then we can still do read/write pmic registers.
	 */
	return 0;
}

static const struct dm_regulator_ops reg_max77686_ops = {
	.get_ldo_cnt	= max77686_get_ldo_cnt,
	.get_buck_cnt	= max77686_get_buck_cnt,
	.get_val_desc	= max77686_get_val_desc,
	.get_mode_desc_array	= max77686_get_mode_desc_array,
	.ldo_val	= max77686_ldo_val,
	.buck_val	= max77686_buck_val,
	.ldo_mode	= max77686_ldo_mode,
	.buck_mode	= max77686_buck_mode,
};

U_BOOT_DRIVER(max77686_regulator) = {
	.name = "max77686 regulator",
	.id = UCLASS_PMIC_REGULATOR,
	.ops = &reg_max77686_ops,
	.ofdata_to_platdata = reg_max77686_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct max77686_regulator_info),
};
