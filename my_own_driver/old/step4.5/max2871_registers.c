// SPDX-License-Identifier: GPL-2.0
/*
 * MAX2871 register cache: default values and SPI write helpers.
 */

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/gcd.h>
#include <linux/math.h>
#include <linux/math64.h>
#include <linux/minmax.h>

#include "max2871_registers.h"

/* Initialize register0 with default values (N, FRAC, INT: max2871_set_frequency) */
static void max2871_init_reg0(struct max2871_state *st)
{
	u32p_replace_bits(&st->regs[0], 0, MAX2871_ADDR_MASK);
}

/* Initialize register1 with default values (M, CPL: max2871_set_frequency) */
static void max2871_init_reg1(struct max2871_state *st)
{
	u32p_replace_bits(&st->regs[1], 1, MAX2871_ADDR_MASK);
	u32p_replace_bits(&st->regs[1], 0, MAX2871_REG1_CPT);
	u32p_replace_bits(&st->regs[1], 1, MAX2871_REG1_P);
}

/*
 * Initialize register2 with default values
 * (LDS, DBR, RDIV2, R: max2871_calc_pfd; LDF: max2871_set_frequency)
 */
static void max2871_init_reg2(struct max2871_state *st)
{
	u32p_replace_bits(&st->regs[2], 2, MAX2871_ADDR_MASK);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_SDN);
	u32p_replace_bits(&st->regs[2], 4, MAX2871_REG2_MUX);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_REG4DB);
	u32p_replace_bits(&st->regs[2], 15, MAX2871_REG2_CP);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_LDP);
	u32p_replace_bits(&st->regs[2], 1, MAX2871_REG2_PDP);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_SHDN);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_TRI);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_RST);
}

/* Initialize register3 with default values */
static void max2871_init_reg3(struct max2871_state *st)
{
	u32p_replace_bits(&st->regs[3], 3, MAX2871_ADDR_MASK);
	u32p_replace_bits(&st->regs[3], 0, MAX2871_REG3_VCO);
	u32p_replace_bits(&st->regs[3], 0, MAX2871_REG3_VAS_SHDN);
	u32p_replace_bits(&st->regs[3], 1, MAX2871_REG3_VAS_TEMP);
	u32p_replace_bits(&st->regs[3], 0, MAX2871_REG3_CSM);
	u32p_replace_bits(&st->regs[3], 1, MAX2871_REG3_MUTEDEL);
	u32p_replace_bits(&st->regs[3], 0, MAX2871_REG3_CDM);
	u32p_replace_bits(&st->regs[3], 38, MAX2871_REG3_CDIV);
}

/* Initialize register4 with default values (BS: calc_pfd; DIVA: set_frequency) */
static void max2871_init_reg4(struct max2871_state *st)
{
	u32p_replace_bits(&st->regs[4], 4, MAX2871_ADDR_MASK);
	u32p_replace_bits(&st->regs[4], 3, MAX2871_REG4_RESERVED);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_SDLDO);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_SDDIV);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_SDREF);
	u32p_replace_bits(&st->regs[4], 1, MAX2871_REG4_FB);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_SDVCO);
	u32p_replace_bits(&st->regs[4], 1, MAX2871_REG4_MTLD);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_BDIV);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_RFB_EN);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_BPWR);
	u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_RFA_EN);
	u32p_replace_bits(&st->regs[4], 3, MAX2871_REG4_APWR);
}

/* Initialize register5 with default values */
static void max2871_init_reg5(struct max2871_state *st)
{
	u32p_replace_bits(&st->regs[5], 5, MAX2871_ADDR_MASK);
	u32p_replace_bits(&st->regs[5], 3, MAX2871_REG5_VAS_DLY);
	u32p_replace_bits(&st->regs[5], 0, MAX2871_REG5_SDPLL);
	u32p_replace_bits(&st->regs[5], 0, MAX2871_REG5_F01);
	u32p_replace_bits(&st->regs[5], 1, MAX2871_REG5_LD);
	u32p_replace_bits(&st->regs[5], 1, MAX2871_REG5_MUX3);
	u32p_replace_bits(&st->regs[5], 0, MAX2871_REG5_ADCS);
	u32p_replace_bits(&st->regs[5], 0, MAX2871_REG5_ADCM);
}

/*
 * Fill the register cache with the static defaults. Frequency-dependent fields
 * are left at 0 and filled in by max2871_calc_pfd() and max2871_set_frequency().
 */
void max2871_init_regs(struct max2871_state *st)
{
	max2871_init_reg0(st);
	max2871_init_reg1(st);
	max2871_init_reg2(st);
	max2871_init_reg3(st);
	max2871_init_reg4(st);
	max2871_init_reg5(st);
}

/* Send one cached register to the chip, MSB first */
int max2871_write_reg(struct max2871_state *st, unsigned int idx)
{
	st->buf = cpu_to_be32(st->regs[idx]);

	return spi_write(st->spi, &st->buf, sizeof(st->buf));
}

/* Send all cached registers, Reg5 first and Reg0 last (datasheet p.13) */
int max2871_write_all(struct max2871_state *st)
{
	int ret;

	for (int i = MAX2871_NUM_REGS - 1; i >= 0; i--) {
		ret = max2871_write_reg(st, i);
		if (ret) {
			dev_err(&st->spi->dev, "Failed to write register %d: %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}

/* Enable the RF OUT*/
int max2871_rf_enable(struct max2871_state *st)
{
    int ret;

    u32p_replace_bits(&st->regs[4], 1, MAX2871_REG4_RFA_EN);
    
    ret = max2871_write_reg(st, 4);
    if(ret != 0)
    {
        return ret;
    }

    gpiod_set_value_cansleep(st->rfout_en, 1);

    return 0;
}

int max2871_rf_disable(struct max2871_state *st)
{
    gpiod_set_value_cansleep(st->rfout_en, 0);

    u32p_replace_bits(&st->regs[4], 0, MAX2871_REG4_RFA_EN);

    return max2871_write_reg(st, 4);
}

/*
 * Choose DBR/R/RDIV2 for the reference clock and store fPFD (datasheet p.14):
 *   fPFD = fREF * (1 + DBR) / (R * (1 + RDIV2)),  fPFD <= 125 MHz
 * fREF is 10-210 MHz, and at most 105 MHz with the doubler (DBR = 1).
 * Also sets the fPFD-dependent fields BS (Reg4) and LDS (Reg2).
 */
int max2871_calc_pfd(struct max2871_state *st)
{
	unsigned long fref = st->fref_hz;
	u32 dbr, r, bs;

	if (fref < 10000000UL || fref > 210000000UL) {
		dev_err(&st->spi->dev, "fREF %lu Hz out of range (10-210 MHz)\n", fref);
		return -EINVAL;
	}

	if (fref <= 62500000UL) {
		/* Doubler on: fPFD = 2 * fREF <= 125 MHz */
		dbr = 1;
		r = 1;
	} else {
		/* Doubler off: divide down to <= 125 MHz */
		dbr = 0;
		r = DIV_ROUND_UP(fref, 125000000UL);
	}

	st->fpfd_hz = fref * (1 + dbr) / r;

	u32p_replace_bits(&st->regs[2], dbr, MAX2871_REG2_DBR);
	u32p_replace_bits(&st->regs[2], 0, MAX2871_REG2_RDIV2);
	u32p_replace_bits(&st->regs[2], r, MAX2871_REG2_R);

	/* Band-select clock must be 50 kHz: BS = round(fPFD / 50 kHz), 1..1023 (p.17) */
	bs = DIV_ROUND_CLOSEST(st->fpfd_hz, 50000U);
	bs = clamp(bs, 1U, 1023U);
	u32p_replace_bits(&st->regs[4], bs >> 8, MAX2871_REG4_BS_MSB);
	u32p_replace_bits(&st->regs[4], bs & 0xff, MAX2871_REG4_BS_LSB);

	/* Lock-detect speed: 1 when fPFD > 32 MHz (p.20) */
	u32p_replace_bits(&st->regs[2], st->fpfd_hz > 32000000U, MAX2871_REG2_LDS);

	dev_info(&st->spi->dev, "fREF=%lu Hz DBR=%u R=%u -> fPFD=%u Hz BS=%u\n",
		 fref, dbr, r, st->fpfd_hz, bs);
	return 0;
}

int max2871_calc_diva(u64 freq_hz)
{
    if(freq_hz < 23500000ULL || freq_hz > 6000000000ULL)
    {
        return -EINVAL;
    }

    for(int d = 0; d <= 7; d++)
    {
        if((freq_hz << d) >= 3000000000ULL) return d;
    }

    return -EINVAL;
}

/*
 * Compute the PLL dividers for an output frequency (FB = 1, DIVA outside loop):
 *   fVCO = freq << d,   N + F/M = fVCO / fPFD   (datasheet p.14)
 * Returns the DIVA code d (>= 0) on success, a negative errno on failure.
 */
int max2871_calc_nfm(u64 freq_hz, u32 fpfd_hz, u32 *n, u32 *f, u32 *m)
{
	u64 fvco;
	u32 rem, g;
	int d;

	d = max2871_calc_diva(freq_hz);
	if (d < 0)
		return d;

	fvco = freq_hz << d;

	/* N = integer part, rem = fractional part * fPFD */
	*n = div_u64_rem(fvco, fpfd_hz, &rem);

	if (rem == 0) {
		/* Integer-N: F = 0, M only needs a valid value (2..4095) */
		*f = 0;
		*m = 2;
	} else {
		/* Exact fraction rem / fPFD, reduced to lowest terms */
		g = gcd(rem, fpfd_hz);
		*f = rem / g;
		*m = fpfd_hz / g;

		/* M is a 12-bit field: approximate with the finest modulus */
		if (*m > 4095) {
			*m = 4095;
			*f = div_u64((u64)rem * 4095 + fpfd_hz / 2, fpfd_hz);
			if (*f == *m) {
				(*n)++;
				*f = 0;
			}
		}
	}

	/* N limits, datasheet p.14-15 */
	if (*f == 0) {
		if (*n < 16 || *n > 65535)
			return -EINVAL;
	} else {
		if (*n < 19 || *n > 4091)
			return -EINVAL;
	}

	return d;
}

/*
 * Program the output frequency on RFOUTA: compute N/F/M/DIVA, update the
 * register cache and send all registers to the chip.
 */
int max2871_set_frequency(struct max2871_state *st, u64 freq_hz)
{
	u32 n, f, m;
	int d, ret;
	bool int_n;

	d = max2871_calc_nfm(freq_hz, st->fpfd_hz, &n, &f, &m);
	if (d < 0)
		return d;

	int_n = (f == 0);

	/* Reg0: N, F, integer-N mode */
	u32p_replace_bits(&st->regs[0], n, MAX2871_REG0_N);
	u32p_replace_bits(&st->regs[0], f, MAX2871_REG0_FRAC);
	u32p_replace_bits(&st->regs[0], int_n, MAX2871_REG0_INT);

	/* Reg1: modulus, charge-pump linearity (0 in int-N, 1 in frac-N) */
	u32p_replace_bits(&st->regs[1], m, MAX2871_REG1_M);
	u32p_replace_bits(&st->regs[1], int_n ? 0 : 1, MAX2871_REG1_CPL);

	/* Reg2: lock-detect function (0 = frac-N, 1 = int-N) */
	u32p_replace_bits(&st->regs[2], int_n, MAX2871_REG2_LDF);

	/* Reg4: RF output divider */
	u32p_replace_bits(&st->regs[4], d, MAX2871_REG4_DIVA);

	ret = max2871_write_all(st);
	if (ret)
		return ret;

	st->freq_hz = freq_hz;
	dev_info(&st->spi->dev, "Frequency %llu Hz: N=%u F=%u M=%u DIVA=%d\n",
		 freq_hz, n, f, m, d);
	dev_info(&st->spi->dev,
		 "Reg0=0x%08x Reg1=0x%08x Reg2=0x%08x Reg3=0x%08x Reg4=0x%08x Reg5=0x%08x\n",
		 st->regs[0], st->regs[1], st->regs[2], st->regs[3], st->regs[4], st->regs[5]);

	return 0;
}
