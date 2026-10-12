// SPDX-License-Identifier: GPL-2.0
/*
 * Cách 3: tạo spi_device từ code lúc chạy bằng spi_new_device().
 * Dùng CS1 (CE1, GPIO7) để không tranh CS0 với DT; cần disable spidev1.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

static struct spi_device *demo_spi;

static int __init demo_newdev_init(void)
{
	struct spi_board_info info = {
		.modalias	= "max2871",
		.max_speed_hz	= 10000000,
		.chip_select	= 1,
		.mode		= SPI_MODE_0,
	};
	struct spi_controller *ctlr;
	struct platform_device *pdev;
	struct device_node *np;

	/* Tìm controller SPI0 qua alias "spi0" trong DT của Pi */
	np = of_find_node_by_path("spi0");
	if (!np)
		return -ENODEV;
	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return -ENODEV;

	/* spi-bcm2835 lưu con trỏ controller bằng platform_set_drvdata() */
	ctlr = platform_get_drvdata(pdev);
	if (!ctlr) {
		put_device(&pdev->dev);
		return -EPROBE_DEFER;	/* spi-bcm2835 chưa probe */
	}

	/* Core: spi_alloc_device() -> điền từ info -> spi_add_device() */
	demo_spi = spi_new_device(ctlr, &info);
	put_device(&pdev->dev);

	return demo_spi ? 0 : -ENODEV;
}

static void __exit demo_newdev_exit(void)
{
	spi_unregister_device(demo_spi);	/* gỡ thiết bị, core giải phóng */
}

module_init(demo_newdev_init);
module_exit(demo_newdev_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Demo: tạo spi_device bằng spi_new_device()");
