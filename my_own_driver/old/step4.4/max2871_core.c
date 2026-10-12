// SPDX-License-Identifier: GPL-2.0
/*
 * MAX2871 23.5-6000 MHz PLL/VCO - SPI driver core: probe/remove, ID tables.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include "max2871_registers.h"

/*
 * Temporary self-test for max2871_calc_nfm() (step 4.2).
 * Expected values computed on the host for fPFD = 38.4 MHz.
 * Remove once max2871_set_frequency() is in place.
 */
#define MAX2871_TEST_FPFD	38400000

struct max2871_nfm_test {
	u64 freq_hz;
	int d;		/* expected DIVA code, or negative errno */
	u32 n, f, m;
};

static const struct max2871_nfm_test max2871_nfm_tests[] = {
	{   50000000ULL,  6,  83,    1,    3 },
	{  433000000ULL,  3,  90,    5,   24 },
	{ 3000000000ULL,  0,  78,    1,    8 },
	{ 2412000000ULL,  1, 125,    5,    8 },
	{ 1234567890ULL,  2, 128, 2460, 4095 },	/* M > 4095: approximation */
	{ 3840000000ULL,  0, 100,    0,    2 },	/* rem = 0: integer-N */
	{   23500000ULL,  7,  78,    1,    3 },	/* lowest output frequency */
	{ 7000000000ULL, -EINVAL, 0, 0, 0 },		/* out of range */
};

static void max2871_selftest_nfm(struct device *dev)
{
	unsigned int pass = 0;

	for (int i = 0; i < ARRAY_SIZE(max2871_nfm_tests); i++) {
		const struct max2871_nfm_test *t = &max2871_nfm_tests[i];
		u32 n = 0, f = 0, m = 0;
		int d = max2871_calc_nfm(t->freq_hz, MAX2871_TEST_FPFD, &n, &f, &m);
		bool ok = (d == t->d) && (d < 0 || (n == t->n && f == t->f && m == t->m));

		dev_info(dev, "%s %llu Hz: d=%d N=%u F=%u M=%u\n",
			 ok ? "PASS" : "FAIL", t->freq_hz, d, n, f, m);
		if (ok)
			pass++;
	}

	dev_info(dev, "calc_nfm self-test: %u/%zu passed\n",
		 pass, ARRAY_SIZE(max2871_nfm_tests));
}

static int max2871_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	int ld_value = 0;
	struct max2871_state *max2871;

	dev_info(&spi->dev, "Probing MAX2871 driver\n");

	max2871 = devm_kzalloc(&spi->dev, sizeof(*max2871), GFP_KERNEL);
	if (!max2871)
		return -ENOMEM;

	max2871->spi = spi;
	/* Get the reference clock from DT ("clocks" property) */
	max2871->clkin = devm_clk_get_enabled(&spi->dev, NULL);
	if (IS_ERR(max2871->clkin))
		return dev_err_probe(&spi->dev, PTR_ERR(max2871->ce), "Failed to get reference clock\n");

	max2871->fref_hz = clk_get_rate(max2871->clkin);
	dev_info(&spi->dev, "Reference clock: %lu Hz\n", max2871->fref_hz);


	/* Fill Reg0..Reg5 with the default configuration */
	max2871_init_regs(max2871);

	/* Choose DBR/R from the reference clock, compute fPFD (step 4.4.4) */
	ret = max2871_calc_pfd(max2871);
	if (ret)
		return ret;

	/* Temporary: check the N/F/M calculation (step 4.2) */
	max2871_selftest_nfm(&spi->dev);

	/* Handle the config of the CE pin */
	max2871->ce = devm_gpiod_get(&spi->dev, "ce", GPIOD_OUT_HIGH);
	if (IS_ERR(max2871->ce))
		return dev_err_probe(&spi->dev, PTR_ERR(max2871->ce), "Failed to get CE GPIO\n");

	/* Handle the config of the RFOUT_EN pin */
	max2871->rfout_en = devm_gpiod_get_optional(&spi->dev, "rfout-en", GPIOD_OUT_LOW);
	if (IS_ERR(max2871->rfout_en))
		return dev_err_probe(&spi->dev, PTR_ERR(max2871->rfout_en),
				     "Failed to get RFOUT_EN GPIO\n");

	/* Handle the config of the LD pin */
	max2871->ld = devm_gpiod_get_optional(&spi->dev, "ld", GPIOD_IN);
	if (IS_ERR(max2871->ld))
		return dev_err_probe(&spi->dev, PTR_ERR(max2871->ld), "Failed to get LD GPIO\n");

	/* First write: program the default output frequency (50 MHz) */
	ret = max2871_set_frequency(max2871, 50000000ULL);
	if (ret)
		return dev_err_probe(&spi->dev, ret, "Failed to set frequency\n");

	msleep(20); /* datasheet p.13: program twice, >= 20 ms apart */

	ret = max2871_write_all(max2871);
	if (ret) {
		dev_err(&spi->dev, "Failed to write registers: %d\n", ret);
		return ret;
	}

	dev_info(&spi->dev, "Successfully wrote all registers\n");

	/* Read the value of the LD pin */
	for (int i = 0; i < 10; i++) {
		ld_value = gpiod_get_value_cansleep(max2871->ld);
		if (ld_value)
			break;
		usleep_range(1000, 2000);
	}

	/*Handle the RF process*/
	if(ld_value)
	{
		/*Turn RF On*/
		ret = max2871_rf_enable(max2871);

		if(ret)
		{
			return ret;
		}
		dev_info(&spi->dev, "PLL locked, RF output enabled\n");
	}
	else
	{
		dev_warn(&spi->dev, "PLL not locked, RF output kept off\n");
	}

	spi_set_drvdata(spi, max2871);
	return 0;
}

static void max2871_spi_remove(struct spi_device *spi)
{
	int ret;
	struct max2871_state *st = spi_get_drvdata(spi);
	ret = max2871_rf_disable(st);
	dev_info(&spi->dev, "RF output disabled (%d)\n", ret);

	dev_info(&spi->dev, "Removing MAX2871 driver\n");
}

static const struct spi_device_id max2871_id_table[] = {
	{ "max2871", },
	{}
};
MODULE_DEVICE_TABLE(spi, max2871_id_table);

static const struct of_device_id max2871_of_match[] = {
	{ .compatible = "maxim,max2871" },
	{}
};
MODULE_DEVICE_TABLE(of, max2871_of_match);

static struct spi_driver max2871_driver = {
	.probe = max2871_spi_probe,
	.remove = max2871_spi_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = max2871_of_match,
	},
	.id_table = max2871_id_table,
};


module_spi_driver(max2871_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HaiLV25");
MODULE_DESCRIPTION("MAX2871 driver");
