/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MAX2871_REGISTERS_H
#define MAX2871_REGISTERS_H

#include <linux/bits.h>
#include <linux/cache.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/types.h>

#define MAX2871_NUM_REGS	6

/* Register address, bits [2:0] of every register */
#define MAX2871_ADDR_MASK	GENMASK(2, 0)

/* Register 0 bit masks - Table 3 */
#define MAX2871_REG0_INT	BIT(31)
#define MAX2871_REG0_N		GENMASK(30, 15)
#define MAX2871_REG0_FRAC	GENMASK(14, 3)

/* Register 1 bit masks - Table 4 */
#define MAX2871_REG1_CPL	GENMASK(30, 29)
#define MAX2871_REG1_CPT	GENMASK(28, 27)
#define MAX2871_REG1_P		GENMASK(26, 15)
#define MAX2871_REG1_M		GENMASK(14, 3)

/* Register 2 bit masks - Table 5 */
#define MAX2871_REG2_LDS	BIT(31)
#define MAX2871_REG2_SDN	GENMASK(30, 29)
#define MAX2871_REG2_MUX	GENMASK(28, 26)
#define MAX2871_REG2_DBR	BIT(25)
#define MAX2871_REG2_RDIV2	BIT(24)
#define MAX2871_REG2_R		GENMASK(23, 14)
#define MAX2871_REG2_REG4DB	BIT(13)
#define MAX2871_REG2_CP		GENMASK(12, 9)
#define MAX2871_REG2_LDF	BIT(8)
#define MAX2871_REG2_LDP	BIT(7)
#define MAX2871_REG2_PDP	BIT(6)
#define MAX2871_REG2_SHDN	BIT(5)
#define MAX2871_REG2_TRI	BIT(4)
#define MAX2871_REG2_RST	BIT(3)

/* Register 3 bit masks - Table 6 */
#define MAX2871_REG3_VCO	GENMASK(31, 26)
#define MAX2871_REG3_VAS_SHDN	BIT(25)
#define MAX2871_REG3_VAS_TEMP	BIT(24)
#define MAX2871_REG3_CSM	BIT(18)
#define MAX2871_REG3_MUTEDEL	BIT(17)
#define MAX2871_REG3_CDM	GENMASK(16, 15)
#define MAX2871_REG3_CDIV	GENMASK(14, 3)

/* Register 4 bit masks - Table 7 */
#define MAX2871_REG4_RESERVED	GENMASK(31, 29)
#define MAX2871_REG4_SDLDO	BIT(28)
#define MAX2871_REG4_SDDIV	BIT(27)
#define MAX2871_REG4_SDREF	BIT(26)
#define MAX2871_REG4_BS_MSB	GENMASK(25, 24)
#define MAX2871_REG4_FB		BIT(23)
#define MAX2871_REG4_DIVA	GENMASK(22, 20)
#define MAX2871_REG4_BS_LSB	GENMASK(19, 12)
#define MAX2871_REG4_SDVCO	BIT(11)
#define MAX2871_REG4_MTLD	BIT(10)
#define MAX2871_REG4_BDIV	BIT(9)
#define MAX2871_REG4_RFB_EN	BIT(8)
#define MAX2871_REG4_BPWR	GENMASK(7, 6)
#define MAX2871_REG4_RFA_EN	BIT(5)
#define MAX2871_REG4_APWR	GENMASK(4, 3)

/* Register 5 bit masks - Table 8 */
#define MAX2871_REG5_VAS_DLY	GENMASK(30, 29)
#define MAX2871_REG5_SDPLL	BIT(25)
#define MAX2871_REG5_F01	BIT(24)
#define MAX2871_REG5_LD		GENMASK(23, 22)
#define MAX2871_REG5_MUX3	BIT(18)
#define MAX2871_REG5_ADCS	BIT(6)
#define MAX2871_REG5_ADCM	GENMASK(5, 3)

/*
 * Driver state. @lock serializes regs[], buf and the SPI writes between the
 * IIO callbacks and the RF-off devm action; the max2871_* helpers expect it held
 * (probe runs before the IIO device is registered, so it does not take it).
 */
struct max2871_state {
	struct spi_device *spi;
	struct mutex lock;	/* protects regs[], buf and SPI writes */
	u32 regs[MAX2871_NUM_REGS];
	__be32 buf ____cacheline_aligned;

	/* GPIO descriptor for the CE pin */
	struct gpio_desc *ce;

	/*
	 * GPIO descriptor for the RFOUT_EN pin.
	 * This pin is optional and can be configured by the hardware.
	 */
	struct gpio_desc *rfout_en;

	/* GPIO descriptor for the LD pin */
	struct gpio_desc *ld;

	/* Reference clock (REF_IN) and its frequency, Hz */
	struct clk *clkin;
	unsigned long fref_hz;

	/* Phase-detector frequency, Hz (max2871_calc_pfd) */
	u32 fpfd_hz;

	/* Current RFOUTA frequency, Hz */
	u64 freq_hz;
};

/* max2871_registers.c */
void max2871_init_regs(struct max2871_state *st);
int max2871_write_reg(struct max2871_state *st, unsigned int idx);
int max2871_write_all(struct max2871_state *st);
int max2871_rf_enable(struct max2871_state *st);
int max2871_rf_disable(struct max2871_state *st);
int max2871_set_power(struct max2871_state *st, unsigned int apwr);
int max2871_calc_pfd(struct max2871_state *st);
int max2871_calc_diva(u64 freq_hz);
int max2871_calc_nfm(u64 freq_hz, u32 fpfd_hz, u32 *n, u32 *f, u32 *m);
int max2871_set_frequency(struct max2871_state *st, u64 freq_hz);

#endif /* MAX2871_REGISTERS_H */
