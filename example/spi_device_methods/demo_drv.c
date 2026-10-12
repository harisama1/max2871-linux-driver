// SPDX-License-Identifier: GPL-2.0
/* Driver dùng chung cho cả 3 cách: chỉ in thông tin spi_device trong probe. */
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/spi/spi.h>

static int demo_probe(struct spi_device *spi)
{
	dev_info(&spi->dev, "probe: cs=%u max_speed=%u mode=0x%x bpw=%u\n",
		 spi_get_chipselect(spi, 0), spi->max_speed_hz,
		 spi->mode, spi->bits_per_word);
	return 0;
}

static void demo_remove(struct spi_device *spi)
{
	dev_info(&spi->dev, "remove\n");
}

/* Cách 1 (DT) khớp qua bảng này: so với "compatible" của node */
static const struct of_device_id demo_of_match[] = {
	{ .compatible = "temix,max2871" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_of_match);

/* Cách 2 và 3 khớp qua bảng này: so với spi_board_info.modalias */
static const struct spi_device_id demo_spi_id[] = {
	{ "max2871" },
	{ }
};
MODULE_DEVICE_TABLE(spi, demo_spi_id);

static struct spi_driver demo_driver = {
	.driver = {
		.name = "max2871-demo",
		.of_match_table = demo_of_match,
	},
	.probe = demo_probe,
	.remove = demo_remove,
	.id_table = demo_spi_id,
};
module_spi_driver(demo_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Demo: driver SPI dùng chung cho 3 cách tạo spi_device");
