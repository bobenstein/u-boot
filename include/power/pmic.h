/*
 *  Copyright (C) 2014 Samsung Electronics
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 *  Copyright (C) 2011-2012 Samsung Electronics
 *  Lukasz Majewski <l.majewski@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CORE_PMIC_H_
#define __CORE_PMIC_H_

#include <dm.h>
#include <linux/list.h>
#include <spi.h>
#include <i2c.h>
#include <power/power_chrg.h>
#include <power/regulator.h>

enum { PMIC_I2C, PMIC_SPI, PMIC_NONE};
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

#ifdef CONFIG_DM_PMIC
/* struct pmic_platdata - a standard descriptor for pmic device, which holds
 * an informations about interface. It is common for all pmic devices.
 *
 * Note:
 * Interface fields are the same as in: struct pmic.
 * Note: struct pmic will be removed in the future after drivers migration
 *
 * @bus        - a physical bus on which device is connected
 * @interface  - an interface type, one of enum: PMIC_I2C, PMIC_SPI, PMIC_NONE
 * @byte_order - one of enum: PMIC_SENSOR_BYTE_ORDER*_LITTLE/_BIG
 * @regs_num   - number of device registers
 * @hw         - one of union structure: p_i2c or p_spi
 *               based on @interface field
*/
struct pmic_platdata {
	int bus;
	int interface;
	int byte_order;
	int regs_num;
	union hw hw;
};

/* enum pmic_op_type - used for various pmic devices operation calls,
 * for decrease a number of functions with the same code for read/write
 * or get/set.
 *
 * @PMIC_OP_GET - get operation
 * @PMIC_OP_SET - set operation
*/
enum pmic_op_type {
	PMIC_OP_GET,
	PMIC_OP_SET,
};

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
int power_init_dm(void);
struct udevice *pmic_get_by_name(int uclass_id, char *name);
struct udevice *pmic_get_by_interface(int uclass_id, int bus, int addr_cs);
const char *pmic_if_str(int interface);
int pmic_check_reg(struct pmic_platdata *p, unsigned reg);
int pmic_reg_write(struct udevice *dev, unsigned reg, unsigned val);
int pmic_reg_read(struct udevice *dev, unsigned reg, unsigned *val);
int pmic_probe(struct udevice *dev, struct spi_slave *spi_slave);

/* drivers/power/pmic_i2c.c */
int pmic_i2c_reg_write(struct udevice *dev, unsigned reg, unsigned val);
int pmic_i2c_reg_read(struct udevice *dev, unsigned reg, unsigned *val);
int pmic_i2c_probe(struct udevice *dev);

/* drivers/power/pmic_spi.c */
int pmic_spi_reg_write(struct udevice *dev, unsigned reg, unsigned val);
int pmic_spi_reg_read(struct udevice *dev, unsigned reg, unsigned *val);
struct spi_slave *pmic_spi_probe(struct udevice *dev);
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
