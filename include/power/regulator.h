/*
 *  Copyright (C) 2014-2015 Samsung Electronics
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _INCLUDE_REGULATOR_H_
#define _INCLUDE_REGULATOR_H_

/* enum regulator_type - used for regulator_*() variant calls */
enum regulator_type {
	REGULATOR_TYPE_LDO = 0,
	REGULATOR_TYPE_BUCK,
	REGULATOR_TYPE_DVS,
	REGULATOR_TYPE_FIXED,
	REGULATOR_TYPE_OTHER,
};

/**
 * struct dm_regulator_mode - this structure holds an information about
 * each regulator operation mode. Probably in most cases - an array.
 * This will be probably a driver-static data, since it is device-specific.
 *
 * @id             - a driver-specific mode id
 * @register_value - a driver-specific value for its mode id
 * @name           - the name of mode - used for regulator command
 * Note:
 * The field 'id', should be always a positive number, since the negative values
 * are reserved for the errno numbers when returns the mode id.
 */
struct dm_regulator_mode {
	int id; /* Set only as >= 0 (negative value is reserved for errno) */
	int register_value;
	const char *name;
};

/**
 * struct dm_regulator_info - this structure holds an information about
 * each regulator constraints and supported operation modes. There is no "step"
 * voltage value - so driver should take care of this.
 *
 * @type       - one of 'enum regulator_type'
 * @mode       - pointer to the regulator mode (array if more than one)
 * @mode_count - number of '.mode' entries
 * @min_uV*    - minimum voltage (micro Volts)
 * @max_uV*    - maximum voltage (micro Volts)
 * @min_uA*    - minimum amperage (micro Amps)
 * @max_uA*    - maximum amperage (micro Amps)
 * @always_on* - bool type, true or false
 * @boot_on*   - bool type, true or false
 * @name*      - fdt regulator name - should be taken from the device tree 
 *
 * Note: for attributes signed with '*'
 * These platform-specific constraints can be taken by regulator api function,
 * which is 'regulator_ofdata_to_platdata()'. Please read the description, which
 * can be found near the declaration of the mentioned function.
*/
struct dm_regulator_info {
	enum regulator_type type;
	struct dm_regulator_mode *mode;
	int mode_count;
	int min_uV;
	int max_uV;
	int min_uA;
	int max_uA;
	bool always_on;
	bool boot_on;
	const char *name;
};

/* PMIC regulator device operations */
struct dm_regulator_ops {
	/**
	 * The regulator output value function calls operates on a micro Volts.
	 *
	 * get/set_value - get/set output value of the given output number
	 * @dev          - regulator device
	 * Sets:
	 * @uV           - set the output value [micro Volts]
	 * Returns: output value [uV] on success or negative errno if fail.
	 */
	int (*get_value)(struct udevice *dev);
	int (*set_value)(struct udevice *dev, int uV);

	/**
	 * The regulator output current function calls operates on a micro Amps.
	 *
	 * get/set_current - get/set output current of the given output number
	 * @dev            - regulator device
	 * Sets:
	 * @uA           - set the output current [micro Amps]
	 * Returns: output value [uA] on success or negative errno if fail.
	 */
	int (*get_current)(struct udevice *dev);
	int (*set_current)(struct udevice *dev, int uA);

	/**
	 * The most basic feature of the regulator output is its enable state.
	 *
	 * get/set_enable - get/set enable state of the given output number
	 * @dev           - regulator device
	 * Sets:
	 * @enable         - set true - enable or false - disable
	 * Returns: true/false for get; or 0 / -errno for set.
	 */
	bool (*get_enable)(struct udevice *dev);
	int (*set_enable)(struct udevice *dev, bool enable);

	/**
	 * The 'get/set_mode()' function calls should operate on a driver
	 * specific mode definitions, which should be found in:
	 * field 'mode' of struct mode_desc.
	 *
	 * get/set_mode - get/set operation mode of the given output number
	 * @dev         - regulator device
	 * Sets
	 * @mode_id     - set output mode id (struct dm_regulator_mode->id)
	 * Returns: id/0 for get/set on success or negative errno if fail.
	 * Note:
	 * The field 'id' of struct type 'dm_regulator_mode', should be always
	 * positive number, since the negative is reserved for the error.
	 */
	int (*get_mode)(struct udevice *dev);
	int (*set_mode)(struct udevice *dev, int mode_id);
};

/**
 * regulator_info: returns a pointer to the devices regulator info structure
 *
 * @dev    - pointer to the regulator device
 * @infop  - pointer to the returned regulator info
 * Returns - 0 on success or negative value of errno.
 */
int regulator_info(struct udevice *dev, struct dm_regulator_info **infop);

/**
 * regulator_mode: returns a pointer to the array of regulator mode info
 *
 * @dev        - pointer to the regulator device
 * @modep      - pointer to the returned mode info array
 * Returns     - count of modep entries on success or negative errno if fail.
 */
int regulator_mode(struct udevice *dev, struct dm_regulator_mode **modep);

/**
 * regulator_get_value: get microvoltage voltage value of a given regulator
 *
 * @dev    - pointer to the regulator device
 * Returns - positive output value [uV] on success or negative errno if fail.
 */
int regulator_get_value(struct udevice *dev);

/**
 * regulator_set_value: set the microvoltage value of a given regulator.
 *
 * @dev    - pointer to the regulator device
 * @uV     - the output value to set [micro Volts]
 * Returns - 0 on success or -errno val if fails
 */
int regulator_set_value(struct udevice *dev, int uV);

/**
 * regulator_get_current: get microampere value of a given regulator
 *
 * @dev    - pointer to the regulator device
 * Returns - positive output current [uA] on success or negative errno if fail.
 */
int regulator_get_current(struct udevice *dev);

/**
 * regulator_set_current: set the microampere value of a given regulator.
 *
 * @dev    - pointer to the regulator device
 * @uA     - set the output current [micro Amps]
 * Returns - 0 on success or -errno val if fails
 */
int regulator_set_current(struct udevice *dev, int uA);

/**
 * regulator_get_enable: get regulator device enable state.
 *
 * @dev    - pointer to the regulator device
 * Returns - true/false of enable state
 */
bool regulator_get_enable(struct udevice *dev);

/**
 * regulator_set_enable: set regulator enable state
 *
 * @dev    - pointer to the regulator device
 * @enable - set true or false
 * Returns - 0 on success or -errno val if fails
 */
int regulator_set_enable(struct udevice *dev, bool enable);

/**
 * regulator_get_mode: get mode of a given device regulator
 *
 * @dev    - pointer to the regulator device
 * Returns - positive  mode number on success or -errno val if fails
 * Note:
 * The regulator driver should return one of defined, mode number rather, than
 * the raw register value. The struct type 'mode_desc' provides a field 'mode'
 * for this purpose and register_value for a raw register value.
 */
int regulator_get_mode(struct udevice *dev);

/**
 * regulator_set_mode: set given regulator mode
 *
 * @dev    - pointer to the regulator device
 * @mode   - mode type (field 'mode' of struct mode_desc)
 * Returns - 0 on success or -errno value if fails
 * Note:
 * The regulator driver should take one of defined, mode number rather
 * than a raw register value. The struct type 'regulator_mode_desc' has
 * a mode field for this purpose and register_value for a raw register value.
 */
int regulator_set_mode(struct udevice *dev, int mode);

/**
 * regulator_ofdata_to_platdata: get the regulator constraints from its fdt node
 * and put it into the regulator 'dm_regulator_info' (dev->uclass_priv).
 *
 * An example of required regulator fdt node constraints:
 * ldo1 {
 *      regulator-compatible = "LDO1"; (not used here, but required for bind)
 *      regulator-name = "VDD_MMC_1.8V"; (mandatory)
 *      regulator-min-microvolt = <1000000>; (mandatory)
 *      regulator-max-microvolt = <1000000>; (mandatory)
 *      regulator-min-microamp = <1000>; (optional)
 *      regulator-max-microamp = <1000>; (optional)
 *      regulator-always-on; (optional)
 *      regulator-boot-on; (optional)
 * };
 *
 * @dev    - pointer to the regulator device
 * Returns - 0 on success or -errno value if fails
 * Note2:
 * This function can be called at stage in which 'dev->uclass_priv' is non-NULL.
 * It is possible at two driver call stages: '.ofdata_to_platdata' or '.probe'.
 */
int regulator_ofdata_to_platdata(struct udevice *dev);

/**
 * regulator_get: returns the pointer to the pmic regulator device based on
 * regulator fdt node data
 *
 * @fdt_name - property 'regulator-name' value of regulator fdt node
 * @devp     - returned pointer to the regulator device
 * Returns: 0 on success or negative value of errno.
 *
 * The returned 'regulator' device can be used with:
 * - regulator_get/set_*
 */
int regulator_get(char *fdt_name, struct udevice **devp);

#endif /* _INCLUDE_REGULATOR_H_ */
