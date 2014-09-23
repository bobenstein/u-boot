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
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <fdtdec.h>
#include <power/pmic.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <dm/lists.h>
#include <i2c.h>
#include <compiler.h>
#include <errno.h>

#define LINE_BUF_LIMIT	80
#define STR_BUF_LEN	26
#define ID_STR_LIMIT	4
#define UC_STR_LIMIT	16
#define DRV_STR_LIMIT	26
#define IF_STR_LIMIT	12

DECLARE_GLOBAL_DATA_PTR;

static struct udevice *pmic_curr;
static struct udevice *reg_curr;

enum info_type {
	INFO_NAME,
	INFO_STATE,
	INFO_DESC,
	INFO_DESC_MODE,
	INFO_DESC_VAL,
};

static char * const pmic_interfaces[] = {
	"I2C",
	"SPI",
	"--",
};

const char *pmic_if_str(int interface)
{
	if (interface >= ARRAY_SIZE(pmic_interfaces))
		return NULL;

	return pmic_interfaces[interface];
}

char *desc_type_str(int desc_type)
{
	static char *type_name[] = {"LDO", "BUCK"};

	switch (desc_type) {
	case DESC_TYPE_LDO:
	case DESC_TYPE_BUCK:
		return type_name[desc_type];
	default:
		return NULL;
	}
}

static int set_curr_dev(struct udevice *dev)
{
	if (!dev)
		return -EINVAL;

	if (!dev->driver)
		return -EINVAL;

	switch (dev->driver->id) {
	case UCLASS_PMIC:
		pmic_curr = dev;
		break;
	case UCLASS_PMIC_REGULATOR:
		reg_curr = dev;
		break;
	default:
		error("Bad driver uclass!\n");
		return -EINVAL;
	}

	return 0;
}

static struct udevice *get_curr_dev(int uclass_id)
{
	switch (uclass_id) {
	case UCLASS_PMIC:
		return pmic_curr;
	case UCLASS_PMIC_REGULATOR:
		return reg_curr;
	default:
		error("Bad uclass ID!\n");
		return NULL;
	}
}

static int curr_dev_name(int uclass_id)
{
	struct udevice *dev = get_curr_dev(uclass_id);

	if (dev) {
		printf("PMIC: %s\n", dev->name);
		return 0;
	} else {
		puts("PMIC dev is not set!\n");
		return -ENODEV;
	}
}

static int pmic_dump(int uclass_id)
{
	struct udevice *dev;
	struct pmic_platdata *p;
	int i, ret;
	u32 val;

	dev = get_curr_dev(uclass_id);
	if (!dev)
		return -ENODEV;

	p = dev->platdata;
	if (!p)
		return -EPERM;

	ret = curr_dev_name(uclass_id);
	if (ret)
		return CMD_RET_USAGE;

	printf("Register count: %u\n", p->regs_num);

	for (i = 0; i < p->regs_num; i++) {
		ret = pmic_reg_read(dev, i, &val);
		if (ret) {
			printf("PMIC: Registers dump failed: %d\n", ret);
			return -EIO;
		}

		if (!(i % 8))
			printf("\n0x%02x: ", i);

		printf("%08x ", val);
	}
	puts("\n");
	return 0;
}

/**
 * display_column - display a string from buffer limited by the column len.
 *                  Allows display well aligned string columns.
 *
 * @buf: a pointer to the buffer with the string to display as a column
 * @str_len: len of the string
 * @column_len: a number of characters to be printed, but it is actually
 *              decremented by 1 for the ending character: '|'
*/
static int display_column(char *buf, unsigned int str_len,
			  unsigned int column_len)
{
	int i = column_len;

	if (str_len < column_len - 1) {
		for (i = str_len; i < column_len; i++)
			buf[i] = ' ';
	}

	buf[column_len - 1] = '|';
	buf[column_len] = '\0';

	puts(buf);

	return i;
}

static int pmic_list_uclass_devices(int uclass_id)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, buf, LINE_BUF_LIMIT);
	struct uclass_driver *uc_drv = NULL;
	struct udevice *dev = NULL;
	struct driver *drv = NULL;
	struct uclass *uc = NULL;
	struct pmic_platdata *pl;
	int addr_cs;
	int len;
	int ret;

	puts("| Id | Uclass        | Driver                  | Interface |\n");

	ret = uclass_get(uclass_id, &uc);
	if (ret) {
		error("PMIC uclass: %d not found!\n", uclass_id);
		return -EINVAL;
	}

	uc_drv = uc->uc_drv;
	uclass_foreach_dev(dev, uc) {
		if (dev)
			device_probe(dev);
		else
			continue;

		printf("| %2.d |", dev->seq);

		len = snprintf(buf, STR_BUF_LEN, " %2.d@%.10s",
				uc_drv->id,  uc_drv->name);
		display_column(buf, len, UC_STR_LIMIT);

		drv = dev->driver;
		if (drv && drv->name)
			len = snprintf(buf, STR_BUF_LEN, " %.26s", drv->name);
		else
			len = snprintf(buf, STR_BUF_LEN, " - ");

		display_column(buf, len, DRV_STR_LIMIT);

		pl = dev_get_platdata(dev);
		if (!pl) {
			len = snprintf(buf, STR_BUF_LEN, " - ");
			display_column(buf, len, IF_STR_LIMIT);
			puts("\n");
			continue;
		}

		if (pl->interface == PMIC_I2C)
			addr_cs = pl->hw.i2c.addr;
		else
			addr_cs = pl->hw.spi.cs;

		len = snprintf(buf, STR_BUF_LEN, " %s%d@0x%x",
			       pmic_if_str(pl->interface),
			       pl->bus, addr_cs);
		display_column(buf, len, IF_STR_LIMIT);

		puts("\n");
	}

	return 0;
}

static int pmic_cmd(char *const argv[], int argc, int uclass_id)
{
	const char *cmd = argv[0];
	struct udevice *dev = NULL;
	int seq;

	if (strcmp(cmd, "list") == 0)
		return pmic_list_uclass_devices(uclass_id);

	if (strcmp(cmd, "dev") == 0) {
		switch (argc) {
		case 2:
			seq = simple_strtoul(argv[1], NULL, 0);
			uclass_get_device_by_seq(uclass_id, seq, &dev);
			if (dev)
				set_curr_dev(dev);
			else
				printf("Device: %d not found\n", seq);
		case 1:
			return curr_dev_name(uclass_id);
		}
	}

	if (strcmp(cmd, "dump") == 0)
		return pmic_dump(uclass_id);

	if (!get_curr_dev(uclass_id))
		return -ENODEV;

	return 1;
}

static char *get_reg_mode_name(int reg, int mode,
			       struct regulator_mode_desc *mode_desc,
			       int desc_cnt)
{
	int i;
	char *mode_name = NULL;

	if (!mode_desc)
		return NULL;

	for (i = 0; i < desc_cnt; i++) {
		if (mode_desc[i].mode == mode)
			mode_name = mode_desc[i].name;
	}

	return mode_name;
}

/**
 * display_reg_info: list regualtor(s) info
 * @start_reg; @end_reg - first and last regulator number to list
 * if start_reg == end_reg - then displays only one regulator info
 *
 * @info_type options:
 *
 * =?INFO_NAME:
 * regN: name
 *
 * =?INFO_STATE:
 * regN: name
 *  - Vout: val mV mode: N (name)
 *
 * =?INFO_DESC
 * regN: name
 *  - Vout: val mV
 *  - Vmin: val mV
 *  - Vmax: val mV
 *  - mode: 1 (name1) [active]
 *  - mode: 2 (name2) [active]
 *  - mode: N (nameN) [active]
 *
 * =?INFO_DESC_VAL
 * regN:
 *  - Vout: val mV
 *  - Vmin: val mV
 *  - Vmax: val mV
 *
 * =?INFO_DESC_VAL
 * regN:
 *  - mode: 1 (name1) [active]
 *  - mode: 2 (name2) [active]
 *  - mode: N (nameN) [active]
 */
static int display_reg_info(int desc_type, int start_reg,
			    int end_reg, int info_type)
{
	struct udevice *dev;
	struct regulator_desc *desc;
	struct regulator_mode_desc *mode_desc;
	char *mode_name;
	int mode_cnt;
	int mode;
	int reg_val, i, j;
	int div = 1000;

	if (start_reg > end_reg)
		return -EINVAL;

	dev = get_curr_dev(UCLASS_PMIC_REGULATOR);
	if (!dev)
		return -ENODEV;

	switch (info_type) {
	case INFO_STATE:
		puts(" RegN:  val mV   mode");
		break;
	case INFO_DESC:
		puts(" RegN:  @descriptors data");
		break;
	case INFO_DESC_VAL:
	case INFO_DESC_MODE:
		break;
	case INFO_NAME:
		puts(" RegN: name");
		break;
	default:
		return -EINVAL;
	}

	for (i = start_reg; i <= end_reg; i++) {
		switch (desc_type) {
		case DESC_TYPE_LDO:
			desc = pmic_ldo_desc(dev, i);
			mode_desc = pmic_ldo_mode_desc(dev, i, &mode_cnt);
			mode = pmic_get_ldo_mode(dev, i);
			break;
		case DESC_TYPE_BUCK:
			desc = pmic_buck_desc(dev, i);
			mode_desc = pmic_buck_mode_desc(dev, i, &mode_cnt);
			mode = pmic_get_buck_mode(dev, i);
			break;
		default:
			return -EINVAL;
		}

		if (desc_type == DESC_TYPE_LDO)
			reg_val = pmic_get_ldo_val(dev, i);
		else
			reg_val = pmic_get_buck_val(dev, i);

		/* no such regulator number */
		if (reg_val < 0)
			continue;

		/* Display in mV */
		reg_val /= div;

		printf("\n%s%.2d:", desc_type_str(desc_type), i);

		switch (info_type) {
		case INFO_STATE:
			printf(" %d mV ",  reg_val);

			mode_name = get_reg_mode_name(i, mode, mode_desc,
							mode_cnt);
			if (mode_name)
				printf("  %d@%s", mode, mode_name);

			continue;
		case INFO_DESC:
		case INFO_DESC_VAL:
			if (desc)
				printf("\n @name: %s", desc->name);

			/* display voltage range */
			printf("\n @Vout: %d mV", reg_val);

			if (!desc)
				puts("\n @no value descriptors");
			else
				printf("\n @Vmin: %d mV\n @Vmax: %d mV",
							(desc->min_uV / div),
							(desc->max_uV / div));

			if (info_type != INFO_DESC)
				continue;
		case INFO_DESC_MODE:
			if (!mode_desc) {
				puts("\n @no mode descriptors");
				continue;
			}

			/* display operation modes info */
			for (j = 0; j < mode_cnt; j++) {
				printf("\n @mode: %d (%s)", mode_desc[j].mode,
							  mode_desc[j].name);
				if (mode_desc[j].mode == mode)
					puts(" (active)");
			}
			continue;
		case INFO_NAME:
			if (desc)
				printf(" %s", desc->name);
			else
				puts(" -");
			continue;
		}
	}

	puts("\n");

	return 0;
}

/**
 * check_reg_mode: check if the given regulator supports the given mode
 *
 * @dev: device to check
 * @reg_num: given devices regulator number
 * @set_mode: mode to check - a mode value of struct regulator_mode_desc
 * @desc_type: descriptor type: DESC_TYPE_LDO or DESC_TYPE_BUCK
 */
static int check_reg_mode(struct udevice *dev, int reg_num, int set_mode,
			  int desc_type)
{
	struct regulator_mode_desc *mode_desc;
	int i, mode_cnt;

	switch (desc_type) {
	case DESC_TYPE_LDO:
		mode_desc = pmic_ldo_mode_desc(dev, reg_num, &mode_cnt);
		if (!mode_desc)
			goto nodesc;

		break;
	case DESC_TYPE_BUCK:
		mode_desc = pmic_buck_mode_desc(dev, reg_num, &mode_cnt);
		if (!mode_desc)
			goto nodesc;

		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < mode_cnt; i++) {
		if (mode_desc[i].mode == set_mode)
			return 0;
	}

	printf("Mode:%d not found in the descriptor:", set_mode);

	display_reg_info(desc_type, reg_num, reg_num, INFO_DESC_MODE);

	return -EINVAL;

nodesc:
	printf("%s%.2d - no descriptor found\n", desc_type_str(desc_type),
						 reg_num);
	return -ENODEV;
}

static int set_reg_mode(int desc_type, int reg_num, int set_mode)
{
	int ret;
	struct udevice *dev;

	dev = get_curr_dev(UCLASS_PMIC_REGULATOR);
	if (!dev)
		return -ENODEV;

	if (check_reg_mode(dev, reg_num, set_mode, desc_type))
		return -EPERM;

	printf("Set %s%.2d mode: %d\n", desc_type_str(desc_type),
					     reg_num, set_mode);

	if (desc_type == DESC_TYPE_LDO)
		ret = pmic_set_ldo_mode(dev, reg_num, set_mode);
	else
		ret = pmic_set_buck_mode(dev, reg_num, set_mode);

	return ret;
}

/**
 * check_reg_val: check if the given regulator value meets
 *                the descriptor min and max values
 *
 * @dev: device to check
 * @reg_num: given devices regulator number
 * @set_val: value to check - in uV
 * @desc_type: descriptor type: DESC_TYPE_LDO or DESC_TYPE_BUCK
 */
static int check_reg_val(struct udevice *dev, int reg_num, int set_val,
			 int desc_type)
{
	struct regulator_desc *desc;

	switch (desc_type) {
	case DESC_TYPE_LDO:
		desc = pmic_ldo_desc(dev, reg_num);
		if (!desc)
			goto nodesc;

		break;
	case DESC_TYPE_BUCK:
		desc = pmic_buck_desc(dev, reg_num);
		if (!desc)
			goto nodesc;

		break;
	default:
		return -EINVAL;
	}

	if (set_val >= desc->min_uV && set_val <= desc->max_uV)
		return 0;

	printf("Value: %d mV exceeds descriptor limits:", (set_val / 1000));

	display_reg_info(desc_type, reg_num, reg_num, INFO_DESC_VAL);

	return -EINVAL;
nodesc:
	printf("%s%.2d - no descriptor found\n", desc_type_str(desc_type),
						 reg_num);
	return -ENODEV;
}

static int set_reg_val(int desc_type, int reg_num, int set_val, int set_force)
{
	int ret;
	struct udevice *dev;

	dev = get_curr_dev(UCLASS_PMIC_REGULATOR);
	if (!dev)
		return -ENODEV;

	/* get mV and set as uV */
	set_val *= 1000;

	if (!set_force && check_reg_val(dev, reg_num, set_val, desc_type))
		return -EPERM;

	printf("Set %s%.2d val: %d mV", desc_type_str(desc_type),
					reg_num, (set_val / 1000));

	if (set_force)
		puts(" (force)\n");
	else
		puts("\n");

	if (desc_type == DESC_TYPE_LDO)
		ret = pmic_set_ldo_val(dev, reg_num, set_val);
	else
		ret = pmic_set_buck_val(dev, reg_num, set_val);

	return ret;
}

static int reg_subcmd(char * const argv[], int argc, int desc_type,
		      int reg_num, int reg_cnt)
{
	int start_reg, end_reg;
	int set_val, set_force;
	const char *cmd;

	/* If reg number is given */
	if (reg_num >= 0) {
		start_reg = reg_num;
		end_reg = reg_num;
	} else {
		start_reg = 0;
		end_reg = reg_cnt;
	}

	cmd = argv[0];
	if (!strcmp("name", cmd))
		return display_reg_info(desc_type, start_reg, end_reg,
					INFO_NAME);
	else if (!strcmp("state", cmd))
		return display_reg_info(desc_type, start_reg, end_reg,
					INFO_STATE);
	else if (!strcmp("desc", cmd))
		return display_reg_info(desc_type, start_reg, end_reg,
					INFO_DESC);

	/* Check if regulator number is given */
	if (reg_num < 0) {
		puts("reg num?\n");
		return -EINVAL;
	}

	/* Check argument value */
	if (argc < 2 || !isdigit(*argv[1])) {
		puts("Expected positive value!\n");
		return -EINVAL;
	}

	set_val = simple_strtoul(argv[1], NULL, 0);
	set_force = 0;

	if (!strcmp("setval", cmd)) {
		if (argc == 3 && !strcmp("-f", argv[2]))
			set_force = 1;

		return set_reg_val(desc_type, reg_num, set_val, set_force);
	}

	if (!strcmp("setmode", cmd))
		return set_reg_mode(desc_type, reg_num, set_val);

	return -EINVAL;
}

static int do_regulator(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	struct udevice *dev = NULL;
	int desc_type;
	int cmd_len;
	char *cmd;
	int ret = 0;
	int cmd_type_len = 0;
	int reg_num = -1;
	int reg_cnt;
	char cmd_type_str[5];

	if (!argc)
		return CMD_RET_USAGE;

	/* list, dev, dump */
	ret = pmic_cmd(argv, argc, UCLASS_PMIC_REGULATOR);
	if (ret != 1)
		goto finish;

	cmd = argv[0];
	cmd_len = strlen(cmd);

	dev = get_curr_dev(UCLASS_PMIC_REGULATOR);
	if (!dev)
		return CMD_RET_USAGE;

	/* cmd_type: ldo, buck, ldoN, buckN */
	if (!strncmp(cmd, "ldo", 3)) {
		reg_cnt = pmic_ldo_cnt(dev);
		desc_type = DESC_TYPE_LDO;
		cmd_type_len = 3;
		strncpy(cmd_type_str, cmd, cmd_type_len);
		cmd_type_str[cmd_type_len] = '\0';
	} else if (!strncmp(cmd, "buck", 4)) {
		reg_cnt = pmic_buck_cnt(dev);
		desc_type = DESC_TYPE_BUCK;
		cmd_type_len = 4;
		strncpy(cmd_type_str, cmd, cmd_type_len);
		cmd_type_str[cmd_type_len] = '\0';
	} else {
		ret = -EINVAL;
		goto finish;
	}

	/* Get regulator number */
	reg_num = -1;
	if (cmd_len > cmd_type_len && isdigit(cmd[cmd_type_len]))
		reg_num = (int)simple_strtoul(cmd + cmd_type_len, NULL, 0);

	/* Subcommand missed? */
	if (argc == 1) {
		printf("name/state/desc/setval/setmode ?\n");
		return CMD_RET_SUCCESS;
	}

	argv++;
	argc--;
	ret = reg_subcmd(argv, argc, desc_type, reg_num, reg_cnt);

finish:
	if (ret < 0) {
		printf("Error: %s\n", errno_str(ret));
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_pmic(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	static struct udevice *dev;
	unsigned reg, val;
	char *cmd;
	int ret = 0;

	if (!argc)
		return CMD_RET_USAGE;

	/* list, dev, dump */
	ret = pmic_cmd(argv, argc, UCLASS_PMIC);
	if (ret != 1)
		goto finish;

	dev = get_curr_dev(UCLASS_PMIC);

	cmd = argv[0];
	if (strcmp(cmd, "read") == 0) {
		if (argc != 2) {
			ret = -EINVAL;
			goto finish;
		}

		reg = simple_strtoul(argv[1], NULL, 0);
		ret = pmic_reg_read(dev, reg, &val);
		if (ret)
			puts("PMIC: Register read failed\n");
		else
			printf("0x%02x: 0x%08x\n", reg, val);

		goto finish;
	}

	if (strcmp(cmd, "write") == 0) {
		if (argc != 3) {
			ret = -EINVAL;
			goto finish;
		}

		reg = simple_strtoul(argv[1], NULL, 0);
		val = simple_strtoul(argv[2], NULL, 0);

		printf("Write reg:%#x  val: %#x\n", reg, val);

		ret = pmic_reg_write(dev, reg, val);
		if (ret)
			puts("PMIC: Register write failed\n");

		goto finish;
	}

finish:
	if (ret < 0) {
		printf("Error: %s\n", errno_str(ret));
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_pmic[] = {
	U_BOOT_CMD_MKENT(pmic, 5, 1, do_pmic, "", ""),
	U_BOOT_CMD_MKENT(regulator, 5, 1, do_regulator, "", ""),
};

static int do_pmicops(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cmd;

	cmd = find_cmd_tbl(argv[0], cmd_pmic, ARRAY_SIZE(cmd_pmic));
	if (cmd == NULL || argc > cmd->maxargs)
		return CMD_RET_USAGE;

	argc--;
	argv++;

	return cmd->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(pmic,	CONFIG_SYS_MAXARGS, 1, do_pmicops,
	"PMIC",
	"list                - list available PMICs\n"
	"pmic dev <id>            - set id to current pmic device\n"
	"pmic dump                - dump registers\n"
	"pmic read <reg>          - read register\n"
	"pmic write <reg> <value> - write register\n"
);

U_BOOT_CMD(regulator, CONFIG_SYS_MAXARGS, 1, do_pmicops,
	"PMIC Regulator",
	"list\n\t- list UCLASS regulator devices\n"
	"regulator dev [id]\n\t- show or set operating regulator device\n"
	"regulator dump\n\t- dump registers of current regulator\n"
	"regulator [ldo/buck][N] [name/state/desc]"
	"\n\t- print regulator(s) info\n"
	"regulator [ldoN/buckN] [setval/setmode] [mV/modeN] [-f]"
	"\n\t- set val (mV) (forcibly) or mode - only if desc available\n\n"
	"Example of usage:\n"
	"First get available commands:\n"
	" # regulator\n"
	"Look after the regulator device 'Id' number:\n"
	" # regulator list\n"
	"Set the operating device:\n"
	" # regulator dev 'Id'\n"
	"List state of device ldo's:\n"
	" # regulator ldo state\n"
	"List descriptors of device ldo's:\n"
	" # regulator ldo desc\n"
	"Set the voltage of ldo number 1 to 1200mV\n"
	" # regulator ldo1 setval 1200\n"
	"Use option: '-f', if given value exceeds the limits(be careful!):\n"
	" # regulator ldo1 setval 1200 -f\n"
	"Set the mode of ldo number 1 to '5' (force not available):\n"
	" # regulator ldo1 setmode 5\n"
);
