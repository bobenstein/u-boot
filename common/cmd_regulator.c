/*
 * Copyright (C) 2014-2015 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <fdtdec.h>
#include <dm.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <dm/lists.h>
#include <i2c.h>
#include <compiler.h>
#include <errno.h>

#define LIMIT_SEQ	3
#define LIMIT_DEVNAME	20
#define LIMIT_OFNAME	20
#define LIMIT_INFO	12

static struct udevice *reg_curr;

static int failed(const char *getset, const char *thing,
		  const char *for_dev, int ret)
{
	printf("Can't %s %s %s.\nError: %d (%s)\n", getset, thing, for_dev,
						    ret, errno_str(ret));
	return CMD_RET_FAILURE;
}

static int do_dev(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct dm_regulator_info *info;
	int seq, ret = CMD_RET_FAILURE;

	switch (argc) {
	case 2:
		seq = simple_strtoul(argv[1], NULL, 0);
		uclass_get_device_by_seq(UCLASS_REGULATOR, seq, &reg_curr);
	case 1:
		ret = regulator_info(reg_curr, &info);
		if (ret)
			return failed("get", "the", "device", ret);

		printf("dev: %d @ %s\n", reg_curr->seq, info->name);
	}

	return CMD_RET_SUCCESS;
}

static int get_curr_dev_and_info(struct udevice **devp,
				 struct dm_regulator_info **infop)
{
	int ret = -ENODEV;

	*devp = reg_curr;
	if (!*devp)
		return failed("get", "current", "device", ret);

	ret = regulator_info(*devp, infop);
	if (ret)
		return failed("get", reg_curr->name, "info", ret);

	return CMD_RET_SUCCESS;
}

static int do_list(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct dm_regulator_info *info;
	const char *parent_uc;
	struct udevice *dev;
	int ret;

	printf("|%*s | %*.*s @ %-*.*s| %s @ %s\n",
	       LIMIT_SEQ, "Seq",
	       LIMIT_DEVNAME, LIMIT_DEVNAME, "Name",
	       LIMIT_OFNAME, LIMIT_OFNAME, "fdtname",
	       "Parent", "uclass");

	for (ret = uclass_first_device(UCLASS_REGULATOR, &dev); dev;
	     ret = uclass_next_device(&dev)) {
		if (regulator_info(dev, &info)) {
			printf("(null) info for: %s\n", dev->name);
			continue;
		}

		/* Parent uclass name*/
		parent_uc = dev->parent->uclass->uc_drv->name;

		printf("|%*d | %*.*s @ %-*.*s| %s @ %s\n",
		       LIMIT_SEQ, dev->seq,
		       LIMIT_DEVNAME, LIMIT_DEVNAME, dev->name,
		       LIMIT_OFNAME, LIMIT_OFNAME, info->name,
		       dev->parent->name, parent_uc);
	}

	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_info(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	struct dm_regulator_mode *modes;
	const char *parent_uc;
	int mode_count;
	int ret;
	int i;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	parent_uc = dev->parent->uclass->uc_drv->name;

	printf("Uclass regulator dev %d info:\n", dev->seq);
	printf("%-*s %s @ %s\n%-*s %s\n%-*s %s\n",
	       LIMIT_INFO, "* parent:", dev->parent->name, parent_uc,
	       LIMIT_INFO, "* dev name:", dev->name,
	       LIMIT_INFO, "* fdt name:" ,info->name);

	printf("%-*s %d\n%-*s %d\n%-*s %d\n%-*s %d\n%-*s %s\n%-*s %s\n",
	       LIMIT_INFO, "* min uV:", info->min_uV,
	       LIMIT_INFO, "* max uV:", info->max_uV,
	       LIMIT_INFO, "* min uA:", info->min_uA,
	       LIMIT_INFO, "* max uA:", info->max_uA,
	       LIMIT_INFO, "* always on:", info->always_on ? "true" : "false",
	       LIMIT_INFO, "* boot on:", info->boot_on ? "true" : "false");

	mode_count = regulator_mode(dev, &modes);
	if (!mode_count)
		return failed("get mode", "for mode", "null count", -EPERM);

	if (mode_count < 0)
		return failed("get", info->name, "mode count", mode_count);

	printf("* operation modes:\n");
	for (i = 0; i < mode_count; i++, modes++)
		printf("  - mode %d (%s)\n", modes->id, modes->name);

	return CMD_RET_SUCCESS;
}

static const char *get_mode_name(struct dm_regulator_mode *mode,
				 int mode_count,
				 int mode_id)
{
	while (mode_count--) {
		if (mode->id == mode_id)
			return mode->name;
		mode++;
	}

	return NULL;
}

static int do_status(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	const char *mode_name;
	int value, mode, ret;
	bool enabled;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	/* Value is mandatory */
	value = regulator_get_value(dev);
	if (value < 0)
		return failed("get", info->name, "voltage value", value);

	mode = regulator_get_mode(dev);
	mode_name = get_mode_name(info->mode, info->mode_count, mode);
	enabled = regulator_get_enable(dev);

	printf("Regulator seq %d status:\n", dev->seq);
	printf("%-*s %d\n%-*s %d (%s)\n%-*s %s\n",
	       LIMIT_INFO, " * value:", value,
	       LIMIT_INFO, " * mode:", mode, mode_name,
	       LIMIT_INFO, " * enabled:", enabled ? "true" : "false");

	return CMD_RET_SUCCESS;
}

static int do_value(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	int value;
	int force;
	int ret;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	if (argc == 1) {
		value = regulator_get_value(dev);
		if (value < 0)
			return failed("get", info->name, "voltage", value);

		printf("%d uV\n", value);
		return CMD_RET_SUCCESS;
	}

	if (info->type == REGULATOR_TYPE_FIXED) {
		printf("Fixed regulator value change not allowed.\n");
		return CMD_RET_SUCCESS;
	}

	if (argc == 3)
		force = !strcmp("-f", argv[2]);
	else
		force = 0;

	value = simple_strtoul(argv[1], NULL, 0);
	if ((value < info->min_uV || value > info->max_uV) && !force) {
		printf("Value exceeds regulator constraint limits\n");
		return CMD_RET_FAILURE;
	}

	ret = regulator_set_value(dev, value);
	if (ret)
		return failed("set", info->name, "voltage value", ret);

	return CMD_RET_SUCCESS;
}

static int do_current(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	int current;
	int ret;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	if (argc == 1) {
		current = regulator_get_current(dev);
		if (current < 0)
			return failed("get", info->name, "current", current);

		printf("%d uA\n", current);
		return CMD_RET_SUCCESS;
	}

	if (info->type == REGULATOR_TYPE_FIXED) {
		printf("Fixed regulator current change not allowed.\n");
		return CMD_RET_SUCCESS;
	}

	current = simple_strtoul(argv[1], NULL, 0);
	if (current < info->min_uA || current > info->max_uA) {
		printf("Current exceeds regulator constraint limits\n");
		return CMD_RET_FAILURE;
	}

	ret = regulator_set_current(dev, current);
	if (ret)
		return failed("set", info->name, "current value", ret);

	return CMD_RET_SUCCESS;
}

static int do_mode(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	int new_mode;
	int mode;
	int ret;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	if (info->type == REGULATOR_TYPE_FIXED) {
		printf("Fixed regulator mode option not allowed.\n");
		return CMD_RET_SUCCESS;
	}

	if (argc == 1) {
		mode = regulator_get_mode(dev);
		if (mode < 0)
			return failed("get", info->name, "mode", ret);

		printf("mode id: %d\n", mode);
		return CMD_RET_SUCCESS;
	}

	new_mode = simple_strtoul(argv[1], NULL, 0);

	ret = regulator_set_mode(dev, new_mode);
	if (ret)
		return failed("set", info->name, "mode", ret);

	return CMD_RET_SUCCESS;
}

static int do_enable(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	int ret;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	ret = regulator_set_enable(dev, true);
	if (ret)
		return failed("enable", "regulator", info->name, ret);

	return CMD_RET_SUCCESS;
}

static int do_disable(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	struct dm_regulator_info *info;
	int ret;

	ret = get_curr_dev_and_info(&dev, &info);
	if (ret)
		return CMD_RET_FAILURE;

	ret = regulator_set_enable(dev, false);
	if (ret)
		return failed("disable", "regulator", info->name, ret);

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t subcmd[] = {
	U_BOOT_CMD_MKENT(dev, 2, 1, do_dev, "", ""),
	U_BOOT_CMD_MKENT(list, 1, 1, do_list, "", ""),
	U_BOOT_CMD_MKENT(info, 2, 1, do_info, "", ""),
	U_BOOT_CMD_MKENT(status, 2, 1, do_status, "", ""),
	U_BOOT_CMD_MKENT(value, 3, 1, do_value, "", ""),
	U_BOOT_CMD_MKENT(current, 3, 1, do_current, "", ""),
	U_BOOT_CMD_MKENT(mode, 2, 1, do_mode, "", ""),
	U_BOOT_CMD_MKENT(enable, 1, 1, do_enable, "", ""),
	U_BOOT_CMD_MKENT(disable, 1, 1, do_disable, "", ""),
};

static int do_regulator(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	cmd_tbl_t *cmd;

	argc--;
	argv++;

	cmd = find_cmd_tbl(argv[0], subcmd, ARRAY_SIZE(subcmd));
	if (cmd == NULL || argc > cmd->maxargs)
		return CMD_RET_USAGE;

	return cmd->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(regulator, CONFIG_SYS_MAXARGS, 1, do_regulator,
	"uclass operations",
	"list         - list UCLASS regulator devices\n"
	"regulator dev [id]     - show or [set] operating regulator device\n"
	"regulator [info]       - print constraints info\n"
	"regulator [status]     - print operating status\n"
	"regulator [value] [-f] - print/[set] voltage value [uV] (force)\n"
	"regulator [current]    - print/[set] current value [uA]\n"
	"regulator [mode_id]    - print/[set] operating mode id\n"
	"regulator [enable]     - enable the regulator output\n"
	"regulator [disable]    - disable the regulator output\n"
);
