/*
 *  Copyright (C) 2014 Samsung Electronics
 *  Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _INCLUDE_REGULATOR_H_
#define _INCLUDE_REGULATOR_H_

#define DESC_NAME_LEN	20

/* enum regulator_desc_type - used for lbo/buck descriptor calls */
enum regulator_desc_type {
	DESC_TYPE_LDO,
	DESC_TYPE_BUCK,
};

/**
 * struct regulator_desc - this structure holds an information about each
 * ldo/buck voltage limits. There is no "step" voltage value - so driver
 * should take care of this. This is rather platform specific so can be
 * taken from the device-tree.
 *
 * @type   - one of: DESC_TYPE_LDO or DESC_TYPE_BUCK
 * @number - hardware ldo/buck number
 * @min_uV - minimum voltage (micro Volts)
 * @max_uV - maximum voltage (micro Volts)
 * @name   - name of LDO - usually will be taken from device tree and can
 *           describe ldo/buck really connected hardware
*/
struct regulator_desc {
	int type;
	int number;
	int min_uV;
	int max_uV;
	char name[DESC_NAME_LEN];
};

/**
 * struct regulator_mode_desc - this structure holds an information about each
 * ldo/buck operation modes. Probably in most cases - an array of modes for
 * each ldo/buck. This will be probably a driver static data since modes
 * are device specific more than platform specific.
 *
 * @type - one of: DESC_TYPE_LDO or DESC_TYPE_BUCK
 * @val  - a driver specific value for this mode
 * @name - the name of mode - for command purposes
 */
struct regulator_mode_desc {
	int mode;
	int reg_val;
	char *name;
};

/* PMIC regulator device operations */
struct dm_regulator_ops {
	/* Number of LDOs and BUCKs */
	int (*get_ldo_cnt)(struct udevice *dev);
	int (*get_buck_cnt)(struct udevice *dev);

	/**
	 * LDO and BUCK attribute descriptors can be usually const static data,
	 * but ldo/buck value limits are often defined in the device-tree,
	 * so those are platform dependent attributes.
	 */
	struct regulator_desc *(*get_val_desc)(struct udevice *dev, int d_type,
					       int d_num);

	/**
	 * LDO/BUCK mode descriptors are device specific, so in this case
	 * we have also the same static data for each driver device.
	 * But each LDO/BUCK regulator can support different modes,
	 * so this is the reason why returns mode descriptors array for each
	 * regulator number.
	*/
	struct regulator_mode_desc *(*get_mode_desc_array)(struct udevice *dev,
							   int desc_type,
							   int desc_num,
							   int *desc_mode_cnt);

	/**
	 * The 'ldo/reg_val()' function calls should operate on a micro Volts
	 * values, however the regulator commands operates in mili Volts.
	 *
	 * The 'ldo/buck_mode()' function calls as a taken/returned mode value
	 * should always use a 'mode' field value from structure type:
	 * 'regulator_mode_desc'. So for this purpose each driver should defines
	 * its own mode types for friendly use with regulator commands.
	 */

	/* LDO operations */
	int (*ldo_val)(struct udevice *dev, int operation, int ldo,
		       int *val);
	int (*ldo_mode)(struct udevice *dev, int operation, int ldo,
			int *mode);

	/* Buck converters operations */
	int (*buck_val)(struct udevice *dev, int operation, int buck,
			int *val);
	int (*buck_mode)(struct udevice *dev, int operation, int buck,
			 int *mode);
};

/**
 * pmic_ldo_cnt - returns the number of driver registered linear regulators
 * and this is used for pmic regulator commands purposes.
 *
 * @dev - pointer to regulator device
 * Returns: a null or positive number of LDO or negative value of errno.
 */
int pmic_ldo_cnt(struct udevice *dev);

/**
 * pmic_buck_cnt - returns the number of driver registered step-down converters
 * and this is used for pmic regulator commands purposes.
 *
 * @dev - pointer to regulator device
 * Returns: a null or positive number of BUCK or negative value of errno.
 */
int pmic_buck_cnt(struct udevice *dev);

/**
 * pmic_ldo_desc - returns a pointer to the regulator value descriptor
 * of a given device LDO number.
 *
 * @dev   - pointer to regulator device
 * @ldo   - descriptor number: device specific output number
 * Returns: pointer to descriptor if found or NULL on error.
 */
struct regulator_desc *pmic_ldo_desc(struct udevice *dev, int ldo);

/**
 * pmic_buck_desc -  returns a pointer to the regulator value descriptor
 * of a given device BUCK number.
 *
 * @dev    - pointer to regulator device
 * @buck   - descriptor number: device specific output number
 * Returns: pointer to descriptor if found or NULL on error.
 */
struct regulator_desc *pmic_buck_desc(struct udevice *dev, int buck);

/**
 * pmic_ldo_mode_desc - returns a pointer to the array of regulator mode
 * descriptors of a given device regulator type and number.
 *
 * @dev        - pointer to regulator device
 * @ldo        - output number: device specific
 * @mode_cnt   - pointer to the returned descriptor array entries
 * Returns: pointer to descriptor if found or NULL on error.
 */
struct regulator_mode_desc *pmic_ldo_mode_desc(struct udevice *dev, int ldo,
					       int *mode_cnt);

/**
 * pmic_buck_mode_desc - returns a pointer to the array of regulator mode
 * descriptors of a given device regulator type and number.
 *
 * @dev        - pointer to regulator device
 * @ldo        - output number: device specific
 * @mode_cnt   - pointer to the returned descriptor array entries
 * Returns: pointer to descriptor if found or NULL on error.
 */
struct regulator_mode_desc *pmic_buck_mode_desc(struct udevice *dev, int buck,
					       int *mode_cnt);

/**
 * pmic_get_ldo_val: returns LDO voltage value of a given device regulator
 * number.
 *
 * @dev        - pointer to regulator device
 * @ldo        - output number: device specific
 * Returns:    - ldo number voltage - unit: uV (micro Volts) or -errno if fails
 */
int pmic_get_ldo_val(struct udevice *dev, int ldo);

/**
 * pmic_set_ldo_val: set LDO voltage value of a given device regulator
 * number to the given value
 *
 * @dev    - pointer to regulator device
 * @ldo    - output number: device specific
 * @val    - voltage value to set - unit: uV (micro Volts)
 * Returns - 0 on success or -errno val if fails
 */
int pmic_set_ldo_val(struct udevice *dev, int ldo, int val);

/**
 * pmic_get_ldo_mode: get LDO mode type of a given device regulator
 * number
 *
 * @dev    - pointer to regulator device
 * @ldo    - output number: device specific
 * Returns - mode number on success or -errno val if fails
 * Note:
 * The regulator driver should return one of defined, continuous mode number,
 * rather than a raw register value. The struct type 'regulator_mode_desc' has
 * a mode field for this purpose and reg_val for a raw register value.
 */
int pmic_get_ldo_mode(struct udevice *dev, int ldo);

/**
 * pmic_set_ldo_mode: set given regulator LDO mode to the given mode type
 *
 * @dev    - pointer to regulator device
 * @ldo    - output number: device specific
 * @mode   - mode type (struct regulator_mode_desc.mode)
 *
 * Returns - 0 on success or -errno value if fails
 * Note:
 * The regulator driver should take one of defined, continuous mode number,
 * rather than a raw register value. The struct type 'regulator_mode_desc' has
 * a mode field for this purpose and reg_val for a raw register value.
 */
int pmic_set_ldo_mode(struct udevice *dev, int ldo, int mode);

/**
 * pmic_get_buck_val: returns BUCK voltage value of a given device regulator
 * number.
 *
 * @dev    - pointer to regulator device
 * @ldo    - output number: device specific
 * Returns - BUCK number voltage - unit: uV (micro Volts) or -errno if fails
 */
int pmic_get_buck_val(struct udevice *dev, int buck);

/**
 * pmic_set_buck_val: set BUCK voltage value of a given device regulator
 * number to the given value
 *
 * @dev    - pointer to regulator device
 * @buck    - output number: device specific
 * @val    - voltage value to set - unit: uV (micro Volts)
 * Returns - 0 on success or -errno val if fails
 */
int pmic_set_buck_val(struct udevice *dev, int buck, int val);

/**
 * pmic_get_buck_mode: get BUCK mode type of a given device regulator
 * number
 *
 * @dev    - pointer to regulator device
 * @buck    - output number: device specific
 * Returns - mode number on success or -errno val if fails
 * Note:
 * The regulator driver should return one of defined, continuous mode number,
 * rather than a raw register value. The struct type 'regulator_mode_desc' has
 * the 'mode' field for this purpose and the 'reg_val' for a raw register value.
 */
int pmic_get_buck_mode(struct udevice *dev, int buck);

/**
 * pmic_set_buck_mode: set given regulator buck mode to the given mode type
 *
 * @dev    - pointer to regulator device
 * @buck   - output number: device specific
 * @mode   - mode type (struct regulator_mode_desc.mode)
 *
 * Returns - 0 on success or -errno value if fails
 * Note:
 * The regulator driver should take one of defined, continuous mode number,
 * rather than a raw register value. The struct type 'regulator_mode_desc' has
 * the 'mode' field for this purpose and the 'reg_val' for a raw register value.
 */
int pmic_set_buck_mode(struct udevice *dev, int buck, int mode);

#endif /* _INCLUDE_REGULATOR_H_ */
