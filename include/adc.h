/*
 * Copyright (C) 2015 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _ADC_H_
#define _ADC_H_

/**
 * struct adc_uclass_platdata - ADC power supply and reference Voltage info
 *
 * @data_mask     - conversion output data mask
 * @channels_num  - number of analog channels input
 * @vdd_supply    - positive reference voltage supply
 * @vss_supply    - negative reference voltage supply
 */
struct adc_uclass_platdata {
	unsigned int data_mask;
	unsigned int channels_num;
	struct udevice *vdd_supply;
	struct udevice *vss_supply;
};

/**
 * struct adc_ops - ADC device operations
 */
struct adc_ops {
	/**
	 * conversion_init() - init device's default conversion parameters
	 *
	 * @dev:     ADC device to init
	 * @channel: analog channel number
	 * @return:  0 if OK, -ve on error
	 */
	int (*adc_init)(struct udevice *dev, int channel);

	/**
	 * conversion_data() - get output data of given channel conversion
	 *
	 * @dev:          ADC device to trigger
	 * @channel_data: pointer to returned channel's data
	 * @return:       0 if OK, -ve on error
	 */
	int (*adc_data)(struct udevice *dev, unsigned int *channel_data);
};

/**
 * adc_init() - init device's default conversion parameters for the given
 * analog input channel.
 *
 * @dev:     ADC device to init
 * @channel: analog channel number
 * @return:  0 if OK, -ve on error
 */
int adc_init(struct udevice *dev, int channel);

/**
 * adc_data() - get conversion data for the channel inited by adc_init().
 *
 * @dev:    ADC device to trigger
 * @data:   pointer to returned channel's data
 * @return: 0 if OK, -ve on error
 */
int adc_data(struct udevice *dev, unsigned int *data);

/**
 * adc_data_mask() - get data mask (ADC resolution mask) for given ADC device.
 * This can be used if adc uclass platform data is filled.
 *
 * @dev:    ADC device to check
 * @return: 0 if OK, -ve on error
 */
int adc_data_mask(struct udevice *dev);

/**
 * adc_channel_single_shot() - get output data of convertion for the ADC
 * device's channel. This function search for the device with the given name,
 * init the given channel and returns the convertion data.
 *
 * @name:    device's name to search
 * @channel: device's input channel to init
 * @data:    pointer to convertion output data
 */
int adc_channel_single_shot(const char *name, int channel, unsigned int *data);

#endif
