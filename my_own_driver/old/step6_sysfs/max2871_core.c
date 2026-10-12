// SPDX-License-Identifier: GPL-2.0
/*
 * MAX2871 23.5-6000 MHz PLL/VCO - SPI driver core: probe/remove, sysfs, ID tables.
 *
 * sysfs attributes in /sys/bus/spi/devices/spiB.C/:
 *   frequency     rw  RFOUTA frequency in Hz (23500000..6000000000)
 *   enable        rw  RFOUTA on (1) / off (0)
 *   output_power  rw  RFOUTA power code APWR 0..3 = -4, -1, +2, +5 dBm
 *   locked        ro  lock-detect pin LD (1 = PLL locked); -ENODEV without ld-gpios
 * ("power" cannot be used: every device already has a power/ directory.)
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/kstrtox.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>

#include "max2871_registers.h"

#define MAX2871_DEFAULT_FREQ_HZ	50000000ULL

/* ---------------------------------------------------------------- sysfs */

static ssize_t frequency_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	u64 freq;

	mutex_lock(&st->lock);
	freq = st->freq_hz;
	mutex_unlock(&st->lock);

	return sysfs_emit(buf, "%llu\n", freq);
}

static ssize_t frequency_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t len)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	u64 freq;
	int ret;

	ret = kstrtou64(buf, 10, &freq);
	if (ret)
		return ret;

	mutex_lock(&st->lock);
	ret = max2871_set_frequency(st, freq);
	mutex_unlock(&st->lock);

	return ret ? ret : len;
}
static DEVICE_ATTR_RW(frequency);

static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	u32 on;

	mutex_lock(&st->lock);
	on = FIELD_GET(MAX2871_REG4_RFA_EN, st->regs[4]);
	mutex_unlock(&st->lock);

	return sysfs_emit(buf, "%u\n", on);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	bool on;
	int ret;

	ret = kstrtobool(buf, &on);
	if (ret)
		return ret;

	mutex_lock(&st->lock);
	ret = on ? max2871_rf_enable(st) : max2871_rf_disable(st);
	mutex_unlock(&st->lock);

	return ret ? ret : len;
}
static DEVICE_ATTR_RW(enable);

static ssize_t output_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	u32 apwr;

	mutex_lock(&st->lock);
	apwr = FIELD_GET(MAX2871_REG4_APWR, st->regs[4]);
	mutex_unlock(&st->lock);

	return sysfs_emit(buf, "%u\n", apwr);
}

static ssize_t output_power_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	unsigned int apwr;
	int ret;

	ret = kstrtouint(buf, 10, &apwr);
	if (ret)
		return ret;

	mutex_lock(&st->lock);
	ret = max2871_set_power(st, apwr);
	mutex_unlock(&st->lock);

	return ret ? ret : len;
}
static DEVICE_ATTR_RW(output_power);

static ssize_t locked_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max2871_state *st = dev_get_drvdata(dev);
	int ld;

	if (!st->ld)
		return -ENODEV;

	ld = gpiod_get_value_cansleep(st->ld);
	if (ld < 0)
		return ld;

	return sysfs_emit(buf, "%d\n", ld);
}
static DEVICE_ATTR_RO(locked);

static struct attribute *max2871_attrs[] = {
	&dev_attr_frequency.attr,
	&dev_attr_enable.attr,
	&dev_attr_output_power.attr,
	&dev_attr_locked.attr,
	NULL
};
ATTRIBUTE_GROUPS(max2871);

/* ---------------------------------------------------------------- probe/remove */

static int max2871_spi_probe(struct spi_device *spi)
{
	struct max2871_state *st;
	int ld_value = 0;
	int ret;

	dev_info(&spi->dev, "Probing MAX2871 driver\n");

	st = devm_kzalloc(&spi->dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->spi = spi;

	ret = devm_mutex_init(&spi->dev, &st->lock);
	if (ret)
		return ret;

	/* Get the reference clock from DT ("clocks" property) */
	st->clkin = devm_clk_get_enabled(&spi->dev, NULL);
	if (IS_ERR(st->clkin))
		return dev_err_probe(&spi->dev, PTR_ERR(st->clkin),
				     "Failed to get reference clock\n");

	st->fref_hz = clk_get_rate(st->clkin);
	dev_info(&spi->dev, "Reference clock: %lu Hz\n", st->fref_hz);

	/* Fill Reg0..Reg5 with the static defaults */
	max2871_init_regs(st);

	/* Choose DBR/R/BS/LDS from the reference clock, compute fPFD */
	ret = max2871_calc_pfd(st);
	if (ret)
		return ret;

	/* CE high: chip enabled */
	st->ce = devm_gpiod_get(&spi->dev, "ce", GPIOD_OUT_HIGH);
	if (IS_ERR(st->ce))
		return dev_err_probe(&spi->dev, PTR_ERR(st->ce), "Failed to get CE GPIO\n");

	/* RFOUT_EN low: RF output off until the PLL is locked */
	st->rfout_en = devm_gpiod_get_optional(&spi->dev, "rfout-en", GPIOD_OUT_LOW);
	if (IS_ERR(st->rfout_en))
		return dev_err_probe(&spi->dev, PTR_ERR(st->rfout_en),
				     "Failed to get RFOUT_EN GPIO\n");

	/* LD: lock-detect input */
	st->ld = devm_gpiod_get_optional(&spi->dev, "ld", GPIOD_IN);
	if (IS_ERR(st->ld))
		return dev_err_probe(&spi->dev, PTR_ERR(st->ld), "Failed to get LD GPIO\n");

	/* First write: program the default output frequency */
	ret = max2871_set_frequency(st, MAX2871_DEFAULT_FREQ_HZ);
	if (ret)
		return dev_err_probe(&spi->dev, ret, "Failed to set frequency\n");

	msleep(20); /* datasheet p.13: program twice, >= 20 ms apart */

	ret = max2871_write_all(st);
	if (ret)
		return dev_err_probe(&spi->dev, ret, "Failed to write registers\n");

	dev_info(&spi->dev, "Successfully wrote all registers\n");

	/* Wait up to ~10-20 ms for lock */
	for (int i = 0; i < 10; i++) {
		ld_value = gpiod_get_value_cansleep(st->ld);
		if (ld_value)
			break;
		usleep_range(1000, 2000);
	}

	if (ld_value > 0) {
		ret = max2871_rf_enable(st);
		if (ret)
			return dev_err_probe(&spi->dev, ret, "Failed to enable RF output\n");
		dev_info(&spi->dev, "PLL locked, RF output enabled\n");
	} else {
		dev_warn(&spi->dev, "PLL not locked, RF output kept off\n");
	}

	spi_set_drvdata(spi, st);
	return 0;
}

static void max2871_spi_remove(struct spi_device *spi)
{
	struct max2871_state *st = spi_get_drvdata(spi);
	int ret;

	mutex_lock(&st->lock);
	ret = max2871_rf_disable(st);
	mutex_unlock(&st->lock);

	dev_info(&spi->dev, "RF output disabled (%d)\n", ret);
}

static const struct spi_device_id max2871_id_table[] = {
	{ "max2871" },
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
		.dev_groups = max2871_groups,
	},
	.id_table = max2871_id_table,
};
module_spi_driver(max2871_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HaiLV25");
MODULE_DESCRIPTION("MAX2871 23.5-6000 MHz PLL/VCO driver");
