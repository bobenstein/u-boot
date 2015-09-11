/*
 * Copyright (C) 2015 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <errno.h>
#include <dm.h>
#include <adc.h>
#include <asm/arch/adc.h>

struct exynos_adc_priv {
	struct exynos_adc_v2 *regs;
};

extern void sdelay(unsigned long loops);

int exynos_adc_data(struct udevice *dev, unsigned int *data)
{
	struct exynos_adc_priv *priv = dev_get_priv(dev);
	struct exynos_adc_v2 *regs = priv->regs;
	unsigned int cfg, timeout_us = ADC_V2_CONV_TIMEOUT_US;

	/* Start conversion */
	cfg = readl(&regs->con1);
	writel(cfg | ADC_V2_CON1_STC_EN, &regs->con1);

	while (ADC_V2_GET_STATUS_FLAG(readl(&regs->status)) != FLAG_CONV_END) {
		sdelay(4);
		if (!timeout_us--) {
			error("ADC conversion timeout!");
			return -ETIME;
		}
	}

	*data = readl(&regs->dat) & ADC_V2_DAT_MASK;

	return 0;
}

int exynos_adc_init(struct udevice *dev, int channel)
{
	struct exynos_adc_priv *priv = dev_get_priv(dev);
	struct exynos_adc_v2 *regs = priv->regs;
	unsigned int cfg;

	/* Check HW version */
	if (readl(&regs->version) != ADC_V2_VERSION) {
		error("This driver supports only ADC v2!");
		return -ENXIO;
	}

	/* ADC Reset */
	writel(ADC_V2_CON1_SOFT_RESET, &regs->con1);

	/* Disable INT - will read status only */
	writel(0x0, &regs->int_en);

	/* CON2 - set conversion parameters */
	cfg = ADC_V2_CON2_C_TIME(3); /* Conversion times: (1 << 3) = 8 */
	cfg |= ADC_V2_CON2_OSEL(OSEL_BINARY);
	cfg |= ADC_V2_CON2_CHAN_SEL(channel);
	cfg |= ADC_V2_CON2_ESEL(ESEL_ADC_EVAL_TIME_20CLK);
	cfg |= ADC_V2_CON2_HIGHF(HIGHF_CONV_RATE_600KSPS);
	writel(cfg, &regs->con2);

	return 0;
}

int exynos_adc_ofdata_to_platdata(struct udevice *dev)
{
	struct adc_uclass_platdata *uc_pdata = dev_get_uclass_platdata(dev);
	struct exynos_adc_priv *priv = dev_get_priv(dev);

	priv->regs = (struct exynos_adc_v2 *)dev_get_addr(dev);

	uc_pdata->data_mask = ADC_V2_DAT_MASK;
	/* Channel count starts from 0 */
	uc_pdata->channels_num = ADC_V2_MAX_CHANNEL + 1;

	return 0;
}

static const struct adc_ops exynos_adc_ops = {
	.adc_init = exynos_adc_init,
	.adc_data = exynos_adc_data,
};

static const struct udevice_id exynos_adc_ids[] = {
	{ .compatible = "samsung,exynos-adc-v2" },
	{ }
};

U_BOOT_DRIVER(exynos_adc) = {
	.name		= "exynos-adc",
	.id		= UCLASS_ADC,
	.of_match	= exynos_adc_ids,
	.ops		= &exynos_adc_ops,
	.ofdata_to_platdata = exynos_adc_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct exynos_adc_priv),
};
