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
 * all its child devices, like in this example:
 *
 * my_pmic.c - pmic driver:
 *
 * int my_pmic_bind(struct udevice *my_pmic)
 * {
 *         // A list of other drivers for this pmic
 *         char *my_pmic_drv_list = {"my_regulator", "my_charger", "my_rtc"};
 *         struct udevice *my_pmic_child_dev[3];
 * ...
 *         for (i=0; i < ARRAY_SIZE(my_pmic_drv_list); i++) {
 *                 // Look for child device driver
 *                 drv_my_reg = lists_driver_lookup_name(my_pmic_drv_list[i]);
 *
 *                 // If driver found, then bind the new device to it
 *                 if (drv_my_reg)
 *                         ret = device_bind(my_pmic, my_pmic_drv_list[i],
 *                                           my_pmic->name, my_pmic->platdata,
 *                                           my_pmic->of_offset,
 *                                           my_pmic_child_dev[i]);
 *                 ...
 *         }
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
 * In this example pmic driver is always a parent for other pmic devices,
 * because the first argument of device_bind() call is the parent device,
 * and here it is 'my_pmic'.
 *
 * For all childs the device->platdata is the same, and provide an info about
 * the same interface - it's used for getting the devices by interface address
 * and uclass types.
 */

/**
 * struct pmic_platdata - PMIC chip interface info
 *
 * This is required for pmic command purposes and should be shared
 * by pmic device and it's childs as the same platdata pointer.
 *
 * @if_type:       one of: PMIC_I2C, PMIC_SPI, PMIC_NONE
 * @if_bus_num:    usually alias seq of i2c/spi bus device
 * @if_addr_cd:    i2c/spi chip addr or cs
 * @if_max_offset: max register offset within the chip
 */
struct pmic_platdata {
	int if_type;
	int if_bus_num;
	int if_addr_cs;
	int if_max_offset;
};

/**
 * struct dm_pmic_ops - PMIC device I/O interface
 *
 * Should be implemented by UCLASS_PMIC device drivers. The standard
 * device operations provides the I/O interface for it's childs.
 *
 * @read:   called for read "reg" value into "val" through "pmic" parent
 * @write:  called for write "val" into "reg" through the "pmic" parent
 */
struct dm_pmic_ops {
	int (*read)(struct udevice *pmic, unsigned reg, unsigned char *val);
	int (*write)(struct udevice *pmic, unsigned reg, unsigned char val);
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
 * pmic_..._get: looking for the pmic device using the specified arguments:
 *
 * NAME:
 *  @name    - device name (useful for single specific names)
 *
 * or SPI/I2C interface:
 *  @bus     - device I2C/SPI bus number
 *  @addr_cs - device specific address for I2C or chip select for SPI,
 *             on the given bus number
 *
 * Returns:
 * @pmic    - returned pointer to the pmic device
 * Returns: 0 on success or negative value of errno.
 *
 * The returned pmic device can be used with:
 * - pmic_read/write calls
 * - pmic_if_* calls
 */
int pmic_get(char *name, struct udevice **pmic);
int pmic_i2c_get(int bus, int addr, struct udevice **pmic);
int pmic_spi_get(int bus, int cs, struct udevice **pmic);

/**
 * pmic_io_dev: returns given pmic/regulator, uclass pmic I/O udevice
 *
 * @pmic    - pointer to pmic_io child device
 *
 * Returns:
 * @pmic_io - pointer to the I/O chip device
 *
 * This is used for pmic command, and also can be used if
 * needs raw read/write operations for uclass regulator device.
 * Example of use:
 *  pmic_io_dev(regulator, &pmic_io);
 *  pmic_write(pmic_io, 0x0, 0x1);
 *
 * This base on pmic_platdata info, so for the given pmic i2c/spi chip,
 * as '@pmic', it should return the same chip, if platdata is filled.
 */
int pmic_io_dev(struct udevice *pmic, struct udevice **pmic_io);

/**
 * pmic_if_* functions: returns one of given pmic interface attribute:
 * - type                         - one of enum { PMIC_I2C, PMIC_SPI, PMIC_NONE}
 * - bus number                   - usually i2c/spi bus seq number
 * - addres for i2c or cs for spi - basaed on dm_i2c/spi_chip
 * - max offset                   - device internal address range
 * - string - "I2C"/"SPI"/"--"
 *
 * @pmic - pointer to the pmic device
 *
 * Returns: one of listed attribute on success, or a negative value of errno.
 */
int pmic_if_type(struct udevice *pmic);
int pmic_if_bus_num(struct udevice *pmic);
int pmic_if_addr_cs(struct udevice *pmic);
int pmic_if_max_offset(struct udevice *pmic);
const char *pmic_if_str(struct udevice *pmic);

/**
 * pmic_read/write: read/write to the UCLASS_PMIC device
 *
 * The required pmic device can be obtained by:
 * - pmic_i2c_get()
 * - pmic_spi_get()
 * - pmic_io_dev() - for pmic command purposes
 *
 * @pmic - pointer to the UCLASS_PMIC device
 * @reg  - device register offset
 * @val  - value for write or its pointer to read
 *
 * Returns: offset value on success or negative value of errno.
 */
int pmic_read(struct udevice *pmic, unsigned reg, unsigned char *val);
int pmic_write(struct udevice *pmic, unsigned reg, unsigned char val);
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
