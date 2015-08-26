/*
 * Copyright (C) 2015 Samsung Electronics
 * Przemyslaw Marczak <p.marczak@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <errno.h>
#include <asm/arch/adc.h>
#include <asm/arch/power.h>

extern void sdelay(unsigned long loops);

struct exynos_adc_v2 *exynos_adc_probe(void)
{
	struct exynos_adc_v2 *adc;

	adc = (struct exynos_adc_v2 *)samsung_get_base_adc();
	if (!adc) {
		error("Invalid ADC base address!");
		return NULL;
	}

	/* Check HW version */
	if (readl(&adc->version) != ADC_V2_VERSION) {
		error("This driver supports only ADC v2!");
		return NULL;
	}

	/* ADC Reset */
	writel(ADC_V2_CON1_SOFT_RESET, &adc->con1);

	return adc;
}

static void exynos_adc_start_conversion(struct exynos_adc_v2 *adc, int channel)
{
	unsigned int cfg;

	/* Disable INT - will read status only */
	writel(0x0, &adc->int_en);

	/* CON2 - set conversion parameters */
	cfg = ADC_V2_CON2_C_TIME(3); /* Conversion times: (1 << 3) = 8 */
	cfg |= ADC_V2_CON2_OSEL(OSEL_BINARY);
	cfg |= ADC_V2_CON2_CHAN_SEL(channel);
	cfg |= ADC_V2_CON2_ESEL(ESEL_ADC_EVAL_TIME_20CLK);
	cfg |= ADC_V2_CON2_HIGHF(HIGHF_CONV_RATE_600KSPS);
	writel(cfg, &adc->con2);

	/* Start conversion */
	cfg = readl(&adc->con1);
	writel(cfg | ADC_V2_CON1_STC_EN, &adc->con1);
}

int exynos_adc_read_channel(int channel)
{
	struct exynos_adc_v2 *adc;
	int timeout_us = ADC_V2_CONV_TIMEOUT_US;

	adc = exynos_adc_probe();
	if (!adc) {
		error("Can't init ADC!");
		return -ENODEV;
	}

	if (channel > ADC_V2_MAX_CHANNEL) {
		error("ADC: max channel is: %d.", ADC_V2_MAX_CHANNEL);
		return -ENODEV;
	}

	exynos_adc_start_conversion(adc, channel);

	while (ADC_V2_GET_STATUS_FLAG(readl(&adc->status)) != FLAG_CONV_END) {
		sdelay(4);
		if (!timeout_us--) {
			error("ADC conversion timeout!");
			return -ETIME;
		}
	}

	return readl(&adc->dat) & ADC_V2_DAT_MASK;
}
