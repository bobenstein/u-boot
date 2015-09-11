/*
 * Copyright (C) 2015 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <dm.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <adc.h>

#define ADC_UCLASS_PLATDATA_SIZE	sizeof(struct adc_uclass_platdata)

int adc_init(struct udevice *dev, int channel)
{
	const struct adc_ops *ops = dev_get_driver_ops(dev);

	if (ops->adc_init)
		return ops->adc_init(dev, channel);

	return -ENOSYS;
}

int adc_data(struct udevice *dev, unsigned int *data)
{
	const struct adc_ops *ops = dev_get_driver_ops(dev);

	*data = 0;

	if (ops->adc_data)
		return ops->adc_data(dev, data);

	return -ENOSYS;
}

int adc_data_mask(struct udevice *dev)
{
	struct adc_uclass_platdata *uc_pdata = dev_get_uclass_platdata(dev);

	if (uc_pdata)
		return uc_pdata->data_mask;

	return -ENOSYS;
}

int adc_channel_single_shot(const char *name, int channel, unsigned int *data)
{
	struct udevice *dev;
	int ret;

	*data = 0;

	ret = uclass_get_device_by_name(UCLASS_ADC, name, &dev);
	if (ret)
		return ret;

	ret = adc_init(dev, channel);
	if (ret)
		return ret;

	ret = adc_data(dev, data);
	if (ret)
		return ret;

	return 0;
}

UCLASS_DRIVER(adc) = {
	.id	= UCLASS_ADC,
	.name	= "adc",
	.per_device_platdata_auto_alloc_size = ADC_UCLASS_PLATDATA_SIZE,
};
