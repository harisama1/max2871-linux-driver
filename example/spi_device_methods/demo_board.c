// SPDX-License-Identifier: GPL-2.0
/*
 * Cách 2: board file (kiểu cũ, trước Device Tree).
 * Chỉ minh hoạ: spi_register_board_info() không được export cho module,
 * đoạn này phải nằm trong code board biên dịch liền vào kernel
 * (ví dụ arch/arm/mach-xxx/board-yyy.c), gọi trước khi controller đăng ký.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>

static struct spi_board_info demo_board_spi[] __initdata = {
	{
		.modalias	= "max2871",	/* khớp spi_device_id của driver */
		.max_speed_hz	= 10000000,
		.bus_num	= 0,		/* SPI0 */
		.chip_select	= 0,		/* CE0 */
		.mode		= SPI_MODE_0,
	},
};

static int __init demo_board_init(void)
{
	/* Core lưu bản mô tả; khi SPI0 đăng ký, core tự gọi spi_new_device() */
	return spi_register_board_info(demo_board_spi,
				       ARRAY_SIZE(demo_board_spi));
}
arch_initcall(demo_board_init);
