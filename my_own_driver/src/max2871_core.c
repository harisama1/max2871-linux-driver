// SPDX-License-Identifier: GPL-2.0
/*
 * MAX2871 23.5-6000 MHz PLL/VCO - SPI driver core: probe, IIO interface, ID tables.
 *
 * IIO attributes in /sys/bus/iio/devices/iio:deviceX/ (one output channel, RFOUTA):
 *   out_altvoltage0_frequency        rw  RFOUTA frequency in Hz (23500000..6000000000)
 *   out_altvoltage0_powerdown        rw  1 = RFOUTA off, 0 = on
 *   out_altvoltage0_power            rw  output power code APWR 0..3 = -4, -1, +2, +5 dBm
 *   out_altvoltage0_locked           ro  lock-detect pin LD (1 = PLL locked)
 *   out_altvoltage0_refin_frequency  ro  reference clock in Hz
 * debugfs: /sys/kernel/debug/iio/iio:deviceX/direct_reg_access (Reg0..Reg5 cache)
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/kstrtox.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>

#include "max2871_registers.h"

#define MAX2871_DEFAULT_FREQ_HZ	50000000ULL

/* ---------------------------------------------------------------- IIO */

enum max2871_iio_attr {
	MAX2871_FREQ,
	MAX2871_PWRDOWN,
	MAX2871_POWER,
	MAX2871_LOCKED,
	MAX2871_FREQ_REFIN,
};

static ssize_t max2871_read(struct iio_dev *indio_dev, uintptr_t private,
			    const struct iio_chan_spec *chan, char *buf)
{
	struct max2871_state *st = iio_priv(indio_dev);
	u64 val;
	int ld;

	switch ((u32)private) {
	case MAX2871_FREQ:
		mutex_lock(&st->lock);
		val = st->freq_hz;
		mutex_unlock(&st->lock);
		break;
	case MAX2871_PWRDOWN:
		mutex_lock(&st->lock);
		val = !FIELD_GET(MAX2871_REG4_RFA_EN, st->regs[4]);
		mutex_unlock(&st->lock);
		break;
	case MAX2871_POWER:
		mutex_lock(&st->lock);
		val = FIELD_GET(MAX2871_REG4_APWR, st->regs[4]);
		mutex_unlock(&st->lock);
		break;
	case MAX2871_LOCKED:
		if (!st->ld)
			return -ENODEV;
		ld = gpiod_get_value_cansleep(st->ld);
		if (ld < 0)
			return ld;
		val = ld;
		break;
	case MAX2871_FREQ_REFIN:
		val = st->fref_hz;
		break;
	default:
		return -EINVAL;
	}

	return sysfs_emit(buf, "%llu\n", val);
}

static ssize_t max2871_write(struct iio_dev *indio_dev, uintptr_t private,
			     const struct iio_chan_spec *chan, const char *buf, size_t len)
{
	struct max2871_state *st = iio_priv(indio_dev);
	unsigned int apwr;
	bool pwrdown;
	u64 freq;
	int ret;

	switch ((u32)private) {
	case MAX2871_FREQ:
		ret = kstrtou64(buf, 10, &freq);
		if (ret)
			return ret;
		mutex_lock(&st->lock);
		ret = max2871_set_frequency(st, freq);
		mutex_unlock(&st->lock);
		break;
	case MAX2871_PWRDOWN:
		ret = kstrtobool(buf, &pwrdown);
		if (ret)
			return ret;
		mutex_lock(&st->lock);
		ret = pwrdown ? max2871_rf_disable(st) : max2871_rf_enable(st);
		mutex_unlock(&st->lock);
		break;
	case MAX2871_POWER:
		ret = kstrtouint(buf, 10, &apwr);
		if (ret)
			return ret;
		mutex_lock(&st->lock);
		ret = max2871_set_power(st, apwr);
		mutex_unlock(&st->lock);
		break;
	default:
		return -EINVAL;
	}

	return ret ? ret : len;
}

#define MAX2871_EXT_INFO(_name, _ident, _write) {	\
	.name = _name,					\
	.read = max2871_read,				\
	.write = _write,				\
	.private = _ident,				\
	.shared = IIO_SEPARATE,				\
}

static const struct iio_chan_spec_ext_info max2871_ext_info[] = {
	/* u64 Hz does not fit IIO_CHAN_INFO_FREQUENCY, same as adf4350 */
	MAX2871_EXT_INFO("frequency", MAX2871_FREQ, max2871_write),
	MAX2871_EXT_INFO("powerdown", MAX2871_PWRDOWN, max2871_write),
	MAX2871_EXT_INFO("power", MAX2871_POWER, max2871_write),
	MAX2871_EXT_INFO("locked", MAX2871_LOCKED, NULL),
	MAX2871_EXT_INFO("refin_frequency", MAX2871_FREQ_REFIN, NULL),
	{ }
};

static const struct iio_chan_spec max2871_chan = {
	.type = IIO_ALTVOLTAGE,
	.indexed = 1,
	.output = 1,
	.channel = 0,
	.ext_info = max2871_ext_info,
};

/* debugfs: read the register cache, or write one register (address bits are forced) */
static int max2871_reg_access(struct iio_dev *indio_dev, unsigned int reg,
			      unsigned int writeval, unsigned int *readval)
{
	struct max2871_state *st = iio_priv(indio_dev);
	int ret = 0;

	if (reg >= MAX2871_NUM_REGS)
		return -EINVAL;

	mutex_lock(&st->lock);
	if (readval) {
		*readval = st->regs[reg];
	} else {
		st->regs[reg] = (writeval & ~MAX2871_ADDR_MASK) | reg;
		ret = max2871_write_reg(st, reg);
	}
	mutex_unlock(&st->lock);

	return ret;
}

static const struct iio_info max2871_info = {
	.debugfs_reg_access = max2871_reg_access,
};

/* ---------------------------------------------------------------- probe */

/*
 * devm action, registered before the IIO device: devres runs in reverse order,
 * so the IIO device is unregistered first and RF is turned off afterwards.
 */
static void max2871_rf_off_action(void *data)
{
	struct max2871_state *st = data;
	int ret;

	mutex_lock(&st->lock);
	ret = max2871_rf_disable(st);
	mutex_unlock(&st->lock);

	dev_info(&st->spi->dev, "RF output disabled (%d)\n", ret);
}

static int max2871_spi_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct max2871_state *st;
	int ld_value = 0;
	int ret;

	dev_info(&spi->dev, "Probing MAX2871 driver\n");

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (!indio_dev)
		return -ENOMEM;

	st = iio_priv(indio_dev);
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

	ret = devm_add_action_or_reset(&spi->dev, max2871_rf_off_action, st);
	if (ret)
		return ret;

	indio_dev->name = "max2871";
	indio_dev->info = &max2871_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = &max2871_chan;
	indio_dev->num_channels = 1;

	return devm_iio_device_register(&spi->dev, indio_dev);
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
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = max2871_of_match,
	},
	.id_table = max2871_id_table,
};
module_spi_driver(max2871_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HaiLV25");
MODULE_DESCRIPTION("MAX2871 23.5-6000 MHz PLL/VCO driver");
