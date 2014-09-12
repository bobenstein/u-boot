/*
 *  Copyright (C) 2014-2015 Samsung Electronics
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 *  Copyright (C) 2011-2012 Samsung Electronics
 *  Lukasz Majewski <l.majewski@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CORE_PMIC_H_
#define __CORE_PMIC_H_

#include <linux/list.h>
#include <spi.h>
#include <i2c.h>
#include <power/power_chrg.h>

enum { PMIC_I2C, PMIC_SPI, PMIC_NONE};

#ifdef CONFIG_POWER
enum { I2C_PMIC, I2C_NUM, };
enum { PMIC_READ, PMIC_WRITE, };
enum { PMIC_SENSOR_BYTE_ORDER_LITTLE, PMIC_SENSOR_BYTE_ORDER_BIG, };

enum {
	PMIC_CHARGER_DISABLE,
	PMIC_CHARGER_ENABLE,
};

struct p_i2c {
	unsigned char addr;
	unsigned char *buf;
	unsigned char tx_num;
};

struct p_spi {
	unsigned int cs;
	unsigned int mode;
	unsigned int bitlen;
	unsigned int clk;
	unsigned int flags;
	u32 (*prepare_tx)(u32 reg, u32 *val, u32 write);
};

struct pmic;
struct power_fg {
	int (*fg_battery_check) (struct pmic *p, struct pmic *bat);
	int (*fg_battery_update) (struct pmic *p, struct pmic *bat);
};

struct power_chrg {
	int (*chrg_type) (struct pmic *p);
	int (*chrg_bat_present) (struct pmic *p);
	int (*chrg_state) (struct pmic *p, int state, int current);
};

struct power_battery {
	struct battery *bat;
	int (*battery_init) (struct pmic *bat, struct pmic *p1,
			     struct pmic *p2, struct pmic *p3);
	int (*battery_charge) (struct pmic *bat);
	/* Keep info about power devices involved with battery operation */
	struct pmic *chrg, *fg, *muic;
};

struct pmic {
	const char *name;
	unsigned char bus;
	unsigned char interface;
	unsigned char sensor_byte_order;
	unsigned int number_of_regs;
	union hw {
		struct p_i2c i2c;
		struct p_spi spi;
	} hw;

	void (*low_power_mode) (void);
	struct power_battery *pbat;
	struct power_chrg *chrg;
	struct power_fg *fg;

	struct pmic *parent;
	struct list_head list;
};
#endif /* CONFIG_POWER */

#ifdef CONFIG_DM_PMIC
/**
 * Driver model pmic framework.
 * The PMIC_UCLASS uclass is designed to provide a common I/O
 * interface for pmic child devices of various uclass types.
 *
 * Usually PMIC devices provides more than one functionality,
 * which means that we should implement uclass operations for
 * each functionality - one driver per uclass.
 *
 * And usually PMIC devices provide those various functionalities
 * by one or more interfaces. And this could looks like this:
 *
 *_ root device
 * |_ BUS 0 device (e.g. I2C0)                 - UCLASS_I2C/SPI/...
 * | |_ PMIC device (READ/WRITE ops)           - UCLASS_PMIC
 * |   |_ REGULATOR device (ldo/buck/... ops)  - UCLASS_REGULATOR
 * |   |_ CHARGER device (charger ops)         - UCLASS_CHARGER (in the future)
 * |   |_ MUIC device (microUSB connector ops) - UCLASS_MUIC    (in the future)
 * |   |_ ...
 * |
 * |_ BUS 1 device (e.g. I2C1)                 - UCLASS_I2C/SPI/...
 *   |_ PMIC device (READ/WRITE ops)           - UCLASS_PMIC
 *     |_ RTC device (rtc ops)                 - UCLASS_MUIC (in the future)
 *
 * Two PMIC types are possible:
 * - single I/O interface
 *   Then UCLASS_PMIC device should be a parent of all pmic devices, where each
 *   is usually different uclass type, but need to access the same interface
 *
 * - multiple I/O interfaces
 *   For each interface the UCLASS_PMIC device should be a parent of only those
 *   devices (different uclass types), which needs to access the specified
 *   interface.
 *
 * For each case binding should be done automatically. If only device tree
 * nodes/subnodes are proper defined, then:
 * |_ the ROOT driver will bind the device for I2C/SPI node:
 *   |_ the I2C/SPI driver should bind a device for pmic node:
 *     |_ the PMIC driver should bind devices for its childs:
 *       |_ regulator (child)
 *       |_ charger   (child)
 *       |_ other     (child)
 *
 * The same for other bus nodes, if pmic uses more then one interface.
 *
 * Note:
 * Each PMIC interface driver should use different compatible string.
 *
 * If each pmic child device driver need access the pmic specified registers,
 * it need to know only the register address and the access is done through
 * the parent pmic driver. Like in the example:
 *
 *_ root driver
 * |_ dev: bus I2C0                                         - UCLASS_I2C
 * | |_ dev: my_pmic (read/write)              (is parent)  - UCLASS_PMIC
 * |   |_ dev: my_regulator (set value/etc..)  (is child)   - UCLASS_REGULATOR
 *
 *
 * To ensure such device relationship, the pmic device driver should also bind
 * all its child devices, like in the example below. It should be by the call
 * to 'pmic_child_node_scan()' - it allows bind in optional subnode and with
 * optional compatible property, e.g. "regulator-compatible" or "regulator".
 *
 * my_pmic.c - pmic driver:
 *
 * int my_pmic_bind(struct udevice *my_pmic)
 * {
 * ...
 *      ret = pmic_child_node_scan(my_pmic, NULL, "compatible");
 *                 ...
 * ...
 * }
 *
 * my_regulator.c - regulator driver (child of pmic I/O driver):
 *
 * int my_regulator_set_value(struct udevice *dev, int type, int num, int *val)
 * {
 * ...
 *         some_val = ...;
 *
 *         // dev->parent == my_pmic
 *         pmic_write(dev->parent, some_reg, some_val);
 * ...
 * }
 *
 * In this example pmic driver is always a parent for other pmic devices.
 */

/**
 * struct dm_pmic_ops - PMIC device I/O interface
 *
 * Should be implemented by UCLASS_PMIC device drivers. The standard
 * device operations provides the I/O interface for it's childs.
 *
 * @reg_count: devices register count
 * @read:      read 'len' bytes at "reg" and store it into the 'buffer'
 * @write:     write 'len' bytes from the 'buffer' to the register at 'reg' address
 */
struct dm_pmic_ops {
	int reg_count;
	int (*read)(struct udevice *dev, uint reg, uint8_t *buffer, int len);
	int (*write)(struct udevice *dev, uint reg, uint8_t *buffer, int len);
};

/* enum pmic_op_type - used for various pmic devices operation calls,
 * for reduce a number of lines with the same code for read/write or get/set.
 *
 * @PMIC_OP_GET - get operation
 * @PMIC_OP_SET - set operation
*/
enum pmic_op_type {
	PMIC_OP_GET,
	PMIC_OP_SET,
};

/**
 * pmic_get_uclass_ops - inline function for getting device uclass operations
 *
 * @dev       - device to check
 * @uclass_id - device uclass id
 *
 * Returns:
 * void pointer to device operations or NULL pointer on error
 */
static __inline__ const void *pmic_get_uclass_ops(struct udevice *dev,
						  int uclass_id)
{
	const void *ops;

	if (!dev)
		return NULL;

	if (dev->driver->id != uclass_id)
		return NULL;

	ops = dev->driver->ops;
	if (!ops)
		return NULL;

	return ops;
}

/* drivers/power/pmic-uclass.c */

/**
 * pmic_child_node_scan - scan the pmic node or its 'optional_subnode' for its
 * childs, by the compatible property name given by arg 'compatible_prop_name'.
 *
 * Few dts files with a different pmic regulators, uses different property
 * name for its driver 'compatible' string, like:
 * - 'compatible'
 * 'regulator-compatible'
 * The pmic driver should should bind its devices by proper compatible property
 * name.
 *
 * @parent                   - pmic parent device (usually UCLASS_PMIC)
 * @optional_subnode         - optional pmic subnode with the childs within it
 * @compatible_property_name - e.g.: 'compatible'; 'regulator-compatible', ...
 */
int pmic_child_node_scan(struct udevice *parent,
			 const char *optional_subnode,
			 const char *compatible_property_name);

/**
 * pmic_get: get the pmic device using its name
 *
 * @name - device name
 * @devp - returned pointer to the pmic device
 * Returns: 0 on success or negative value of errno.
 *
 * The returned devp device can be used with pmic_read/write calls
 */
int pmic_get(char *name, struct udevice **devp);

/**
 * pmic_reg_count: get the pmic register count
 *
 * The required pmic device can be obtained by 'pmic_get()'
 *
 * @dev - pointer to the UCLASS_PMIC device
 *
 * Returns: register count value on success or negative value of errno.
 */
int pmic_reg_count(struct udevice *dev);

/**
 * pmic_read/write: read/write to the UCLASS_PMIC device
 *
 * The required pmic device can be obtained by 'pmic_get()'
 *
 * @pmic - pointer to the UCLASS_PMIC device
 * @reg  - device register offset
 * @val  - value for write or its pointer to read
 *
 * Returns: 0 on success or negative value of errno.
 */
int pmic_read(struct udevice *dev, uint reg, uint8_t *buffer, int len);
int pmic_write(struct udevice *dev, uint reg, uint8_t *buffer, int len);
#endif /* CONFIG_DM_PMIC */

#ifdef CONFIG_POWER
int pmic_init(unsigned char bus);
int power_init_board(void);
int pmic_dialog_init(unsigned char bus);
int check_reg(struct pmic *p, u32 reg);
struct pmic *pmic_alloc(void);
struct pmic *pmic_get(const char *s);
int pmic_probe(struct pmic *p);
int pmic_reg_read(struct pmic *p, u32 reg, u32 *val);
int pmic_reg_write(struct pmic *p, u32 reg, u32 val);
int pmic_set_output(struct pmic *p, u32 reg, int ldo, int on);
#endif

#define pmic_i2c_addr (p->hw.i2c.addr)
#define pmic_i2c_tx_num (p->hw.i2c.tx_num)

#define pmic_spi_bitlen (p->hw.spi.bitlen)
#define pmic_spi_flags (p->hw.spi.flags)

#endif /* __CORE_PMIC_H_ */
