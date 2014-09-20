/*
 *  Copyright (C) 2014-2015 Samsung Electronics
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _INCLUDE_REGULATOR_H_
#define _INCLUDE_REGULATOR_H_

#define DESC_NAME_LEN	20

/* enum regulator_type - used for regulator_*() variant calls */
enum regulator_type {
	REGULATOR_TYPE_LDO = 0,
	REGULATOR_TYPE_BUCK,
	REGULATOR_TYPE_DVS,
};

/* enum regulator_out_state - used for ldo/buck output state setting */
enum regulator_out_state {
	REGULATOR_OFF = 0,
	REGULATOR_ON,
};

/**
 * struct regulator_value_desc - this structure holds an information about
 * each regulator voltage limits. There is no "step" voltage value - so driver
 * should take care of this. This is rather platform specific so can be
 * taken from the device-tree.
 *
 * @type   - one of: DESC_TYPE_LDO, DESC_TYPE_BUCK or  DESC_TYPE_DVS
 * @number - hardware internal regulator number
 * @min_uV - minimum voltage (micro Volts)
 * @max_uV - maximum voltage (micro Volts)
 * @name   - name of regulator - usually will be taken from device tree and
 *           can describe regulator output connected hardware, e.g. "eMMC"
*/
struct regulator_value_desc {
	int type;
	int number;
	int min_uV;
	int max_uV;
	char name[DESC_NAME_LEN];
};

/**
 * struct regulator_mode_desc - this structure holds an information about
 * each regulator operation modes. Probably in most cases - an array.
 * This will be probably a driver static data since it's device specific.
 *
 * @mode           - a driver specific mode number - used for regulator command
 * @register_value - a driver specific value for this mode
 * @name           - the name of mode - used for regulator command
 */
struct regulator_mode_desc {
	int mode;
	int register_value;
	char *name;
};

/* PMIC regulator device operations */
struct dm_regulator_ops {
	/**
	 * get_cnt - get the number of a given regulator 'type' outputs
	 *
	 * @dev    - regulator device
	 * @type   - regulator type, one of enum regulator_type
	 * Gets:
	 * @cnt    - regulator count
	 * Returns: 0 on success or negative value of errno.
	 */
	int (*get_cnt)(struct udevice *dev, int type, int *cnt);

	/**
	 * Regulator output value descriptor is specific for each output.
	 * It's usually driver static data, but output value limits are
	 * often defined in the device-tree, so those are platform dependent
	 * attributes.
	 *
	 * get_value_desc - get value descriptor of the given regulator output
	 * @dev           - regulator device
	 * @type          - regulator type, one of enum regulator_type
	 * @number        - regulator number
	 * Gets:
	 * @desc          - pointer to the descriptor
	 * Returns: 0 on success or negative value of errno.
	 */
	int (*get_value_desc)(struct udevice *dev, int type, int number,
			      struct regulator_value_desc **desc);

	/**
	 * Regulator output mode descriptors are device specific.
	 * It's usually driver static data.
	 * Each regulator output can support different mode types,
	 * so usually will return an array with a number 'mode_desc_cnt'
	 * of mode descriptors.
	 *
	 * get_mode_desc_array - get mode descriptor array of the given
	 *                       regulator output number
	 * @dev                - regulator device
	 * @type               - regulator type, one of 'enum regulator_type'
	 * @number             - regulator number
	 * Gets:
	 * @mode_desc_cnt      - descriptor array entry count
	 * @desc               - pointer to the descriptor array
	 * Returns: 0 on success or negative value of errno.
	 */
	int (*get_mode_desc_array)(struct udevice *dev, int type,
				   int number, int *mode_desc_cnt,
				   struct regulator_mode_desc **desc);

	/**
	 * The regulator output value function calls operates on a micro Volts,
	 * however the regulator commands operates on a mili Volt unit.
	 *
	 * get/set_value - get/set output value of the given output number
	 * @dev          - regulator device
	 * @type         - regulator type, one of 'enum regulator_type'
	 * @number       - regulator number
	 * Sets/Gets:
	 * @value        - set or get output value
	 * Returns: 0 on success or negative value of errno.
	 */
	int (*get_value)(struct udevice *dev, int type, int number, int *value);
	int (*set_value)(struct udevice *dev, int type, int number, int value);

	/**
	 * The most basic feature of the regulator output is it's on/off state.
	 *
	 * get/set_state - get/set on/off state of the given output number
	 * @dev          - regulator device
	 * @type         - regulator type, one of 'enum regulator_type'
	 * @number       - regulator number
	 * Sets/Gets:
	 * @state        - set or get one of 'enum regulator_out_state'
	 * Returns: 0 on success or negative value of errno.
	 */
	int (*get_state)(struct udevice *dev, int type, int number, int *state);
	int (*set_state)(struct udevice *dev, int type, int number, int state);

	/**
	 * The 'get/set_mode()' function calls should operate on a driver
	 * specific mode definitions, which should be found in:
	 * field 'mode' of struct regulator_mode_desc.
	 *
	 * get/set_mode - get/set operation mode of the given output number
	 * @dev          - regulator device
	 * @type         - regulator type, one of 'enum regulator_type'
	 * @number       - regulator number
	 * Sets/Gets:
	 * @mode         - set or get output mode
	 * Returns: 0 on success or negative value of errno.
	 */
	int (*get_mode)(struct udevice *dev, int type, int number, int *mode);
	int (*set_mode)(struct udevice *dev, int type, int number, int mode);
};

/**
 * regulator_get_cnt: returns the number of driver registered regulators.
 * This is used for pmic regulator command purposes.
 *
 * @dev     - pointer to the regulator device
 * @type    - regulator type: device specific
 * Gets:
 * @cnt     - regulator count
 * Returns: 0 on success or negative value of errno.
 */
int regulator_get_cnt(struct udevice *dev, int type, int *cnt);

/**
 * regulator_get_value_desc: returns a pointer to the regulator value
 * descriptor of a given device regulator output number.
 *
 * @dev      - pointer to the regulator device
 * @type     - regulator descriptor type
 * @number   - descriptor number (regulator output number)
 * Gets:
 * @desc     - pointer to the descriptor
 * Returns: 0 on success or negative value of errno.
 */
int regulator_get_value_desc(struct udevice *dev, int type, int number,
			     struct regulator_value_desc **desc);

/**
 * regulator_get_mode_desc: returns a pointer to the array of regulator mode
 * descriptors of a given device regulator type and number.
 *
 * @dev        - pointer to the regulator device
 * @type       - regulator type: device specific
 * @number     - output number: device specific
 * Gets:
 * @mode_cnt   - descriptor array entry count
 * @desc       - pointer to the descriptor array
 * Returns: 0 on success or negative value of errno.
 */
int regulator_get_mode_desc(struct udevice *dev, int type, int number,
			    int *mode_cnt, struct regulator_mode_desc **desc);

/**
 * regulator_get_value: returns voltage value of a given device
 * regulator type  number.
 *
 * @dev        - pointer to the regulator device
 * @type       - regulator type: device specific
 * @number     - output number: device specific
 * Gets:
 * @val        - returned voltage - unit: uV (micro Volts)
 * Returns: 0 on success or negative value of errno.
 */
int regulator_get_value(struct udevice *dev, int type, int number, int *val);

/**
 * regulator_set_value: set the voltage value of a given device regulator type
 * number to the given one passed by the argument: @val
 *
 * @dev    - pointer to the regulator device
 * @type   - regulator type: device specific
 * @number - output number: device specific
 * @val    - voltage value to set - unit: uV (micro Volts)
 * Returns - 0 on success or -errno val if fails
 */
int regulator_set_value(struct udevice *dev, int type, int number, int val);

/**
 * regulator_get_state: returns output state of a given device
 * regulator type number.
 *
 * @dev        - pointer to the regulator device
 * @type       - regulator type, device specific
 * @number     - regulator number, device specific
 * Gets:
 * @state      - returned regulator number output state:
 *               - REGULATOR_OFF (0) - disable
 *               - REGULATOR_ON  (1) - enable
 * Returns:    - 0 on success or -errno val if fails
 */
int regulator_get_state(struct udevice *dev, int type, int number, int *state);

/**
 * regulator_set_state: set output state of a given device regulator type
 * number to the given one passed by the argument: @state
 *
 * @dev            - pointer to the regulator device
 * @type           - regulator type: device specific
 * @number         - output number: device specific
 * @state          - output state to set
 *                   - enum REGULATOR_OFF == 0 - disable
 *                   - enum REGULATOR_ON  == 1 - enable
 * Returns - 0 on success or -errno val if fails
 */
int regulator_set_state(struct udevice *dev, int type, int number, int state);

/**
 * regulator_get_mode: get mode number of a given device regulator type number
 *
 * @dev    - pointer to the regulator device
 * @type   - output number: device specific
 * @number - output number: device specific
 * Gets:
 * @mode   - returned mode number
 * Returns - 0 on success or -errno val if fails
 * Note:
 * The regulator driver should return one of defined, mode number rather
 * than the raw register value. The struct type 'regulator_mode_desc' has
 * a mode field for this purpose and register_value for a raw register value.
 */
int regulator_get_mode(struct udevice *dev, int type, int number, int *mode);

/**
 * regulator_set_mode: set given regulator type and number mode to the given
 * one passed by the argument: @mode
 *
 * @dev    - pointer to the regulator device
 * @type   - output number: device specific
 * @number - output number: device specific
 * @mode   - mode type (struct regulator_mode_desc.mode)
 * Returns - 0 on success or -errno value if fails
 * Note:
 * The regulator driver should take one of defined, mode number rather
 * than a raw register value. The struct type 'regulator_mode_desc' has
 * a mode field for this purpose and register_value for a raw register value.
 */
int regulator_set_mode(struct udevice *dev, int type, int number, int mode);

/**
 * regulator_..._get: returns the pointer to the pmic regulator device
 * by specified arguments:
 *
 * NAME:
 *  @name    - device name (useful for specific names)
 * or SPI/I2C interface:
 *  @bus     - device I2C/SPI bus number
 *  @addr_cs - device specific address for I2C or chip select for SPI,
 *             on the given bus number
 * Gets:
 * @regulator - pointer to the pmic device
 * Returns: 0 on success or negative value of errno.
 *
 * The returned 'regulator' device can be used with:
 * - regulator_get/set_*
 * and if pmic_platdata is given, then also with:
 * - pmic_if_* calls
 * The last one is used for the pmic command purposes.
 */
int regulator_get(char *name, struct udevice **regulator);
int regulator_i2c_get(int bus, int addr, struct udevice **regulator);
int regulator_spi_get(int bus, int cs, struct udevice **regulator);

#endif /* _INCLUDE_REGULATOR_H_ */
