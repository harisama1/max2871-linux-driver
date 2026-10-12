// SPDX-License-Identifier: GPL-2.0

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/spi/spi.h>
#include <linux/bitfield.h>
#include <linux/gpio/consumer.h>

#include "max2871_registers.h"


struct max2871_state {
    struct spi_device *spi;
    u32 regs[MAX2871_NUM_REGS];
    __be32 buf ____cacheline_aligned;

    //GPIO descriptor for the CE pin
    struct gpio_desc *ce;

    /*
        - GPIO descriptor for the RFOUT_EN pin.
        - This pin is optional and can be configure by the hardware.
    */
    struct gpio_desc *rfout_en;

    //GPIO descriptor for the LD pin
    struct gpio_desc *ld;
};

static int max2871_write_reg(struct max2871_state *st, unsigned int idx)
{
    int status = 0;
    st->buf = cpu_to_be32(st->regs[idx]);
    
    status = spi_write(st->spi,&st->buf, sizeof(st->buf));

    return status;
}

static int max2871_write_all(struct max2871_state *st)
{
    for (int i = MAX2871_NUM_REGS - 1; i >= 0; i--)
    {
        int ret = max2871_write_reg(st, i);
        if (ret != 0)
        {
            dev_err(&st->spi->dev, "Failed to write register %d: %d\n", i, ret);
            return ret;
        }
    }
    
    return 0;
}

static int max2871_spi_probe(struct spi_device *spi)
{
    int ret = 0;
    int ld_value = 0;
    struct max2871_state *max2871;

    dev_info(&spi->dev, "Probing MAX2871 driver\n");

    max2871 = devm_kzalloc(&spi->dev, sizeof(*max2871), GFP_KERNEL);
    if (!max2871){
        return -ENOMEM;
    }

    max2871->spi = spi;

    // Initialize register0 with default values
    u32p_replace_bits(&max2871->regs[0], 0, MAX2871_ADDR_MASK);
    u32p_replace_bits(&max2871->regs[0], 0, MAX2871_REG0_INT);
    u32p_replace_bits(&max2871->regs[0], 83, MAX2871_REG0_N);
    u32p_replace_bits(&max2871->regs[0], 1365, MAX2871_REG0_FRAC);
    dev_info(&spi->dev, "Reg0 = 0x%08x\n", max2871->regs[0]);

    // Initialize register1 with default values
    u32p_replace_bits(&max2871->regs[1], 1, MAX2871_ADDR_MASK);
    u32p_replace_bits(&max2871->regs[1], 0, MAX2871_REG1_CPL);
    u32p_replace_bits(&max2871->regs[1], 0, MAX2871_REG1_CPT);
    u32p_replace_bits(&max2871->regs[1], 1, MAX2871_REG1_P);
    u32p_replace_bits(&max2871->regs[1], 4095, MAX2871_REG1_M);
    dev_info(&spi->dev, "Reg1 = 0x%08x\n", max2871->regs[1]);

    // Initialize register2 with default values
    u32p_replace_bits(&max2871->regs[2], 2, MAX2871_ADDR_MASK);
	u32p_replace_bits(&max2871->regs[2], 1, MAX2871_REG2_LDS);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_SDN);
	u32p_replace_bits(&max2871->regs[2], 4, MAX2871_REG2_MUX);
	u32p_replace_bits(&max2871->regs[2], 1, MAX2871_REG2_DBR);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_RDIV2);
	u32p_replace_bits(&max2871->regs[2], 1, MAX2871_REG2_R);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_REG4DB);
	u32p_replace_bits(&max2871->regs[2], 15, MAX2871_REG2_CP);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_LDF);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_LDP);
	u32p_replace_bits(&max2871->regs[2], 1, MAX2871_REG2_PDP);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_SHDN);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_TRI);
	u32p_replace_bits(&max2871->regs[2], 0, MAX2871_REG2_RST);
	dev_info(&spi->dev, "Reg2 = 0x%08x\n", max2871->regs[2]);

    // Initialize register3 with default values
    u32p_replace_bits(&max2871->regs[3], 3, MAX2871_ADDR_MASK);
	u32p_replace_bits(&max2871->regs[3], 0, MAX2871_REG3_VCO);
	u32p_replace_bits(&max2871->regs[3], 0, MAX2871_REG3_VAS_SHDN);
	u32p_replace_bits(&max2871->regs[3], 1, MAX2871_REG3_VAS_TEMP);
	u32p_replace_bits(&max2871->regs[3], 0, MAX2871_REG3_CSM);
	u32p_replace_bits(&max2871->regs[3], 1, MAX2871_REG3_MUTEDEL);
	u32p_replace_bits(&max2871->regs[3], 0, MAX2871_REG3_CDM);
	u32p_replace_bits(&max2871->regs[3], 38, MAX2871_REG3_CDIV);
	dev_info(&spi->dev, "Reg3 = 0x%08x\n", max2871->regs[3]);

    // Initialize register4 with default values
    u32p_replace_bits(&max2871->regs[4], 4, MAX2871_ADDR_MASK);
	u32p_replace_bits(&max2871->regs[4], 3, MAX2871_REG4_RESERVED);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_SDLDO);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_SDDIV);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_SDREF);
	u32p_replace_bits(&max2871->regs[4], 3, MAX2871_REG4_BS_MSB);
	u32p_replace_bits(&max2871->regs[4], 1, MAX2871_REG4_FB);
	u32p_replace_bits(&max2871->regs[4], 6, MAX2871_REG4_DIVA);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_BS_LSB);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_SDVCO);
	u32p_replace_bits(&max2871->regs[4], 1, MAX2871_REG4_MTLD);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_BDIV);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_RFB_EN);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_BPWR);
	u32p_replace_bits(&max2871->regs[4], 0, MAX2871_REG4_RFA_EN);
	u32p_replace_bits(&max2871->regs[4], 3, MAX2871_REG4_APWR);
	dev_info(&spi->dev, "Reg4 = 0x%08x\n", max2871->regs[4]);

    // Initialize register5 with default values
    u32p_replace_bits(&max2871->regs[5], 5, MAX2871_ADDR_MASK);
	u32p_replace_bits(&max2871->regs[5], 3, MAX2871_REG5_VAS_DLY);
	u32p_replace_bits(&max2871->regs[5], 0, MAX2871_REG5_SDPLL);
	u32p_replace_bits(&max2871->regs[5], 0, MAX2871_REG5_F01);
	u32p_replace_bits(&max2871->regs[5], 1, MAX2871_REG5_LD);
	u32p_replace_bits(&max2871->regs[5], 1, MAX2871_REG5_MUX3);
	u32p_replace_bits(&max2871->regs[5], 0, MAX2871_REG5_ADCS);
	u32p_replace_bits(&max2871->regs[5], 0, MAX2871_REG5_ADCM);
	dev_info(&spi->dev, "Reg5 = 0x%08x\n", max2871->regs[5]);

    // Handle the config of the CE pin
    max2871->ce = devm_gpiod_get(&spi->dev, "ce", GPIOD_OUT_HIGH);

    if(IS_ERR(max2871->ce))
    {
        return dev_err_probe(&spi->dev, PTR_ERR(max2871->ce), "Failed to get CE GPIO\n");
    }

    // Handle the config of the RFOUT_EN pin
    max2871->rfout_en = devm_gpiod_get_optional(&spi->dev, "rfout-en", GPIOD_OUT_LOW);

    if(IS_ERR(max2871->rfout_en))
    {
        return dev_err_probe(&spi->dev, PTR_ERR(max2871->rfout_en), "Failed to get RFOUT_EN GPIO\n");
    }

    //Handle the config of the LD pin
    max2871->ld = devm_gpiod_get_optional(&spi->dev, "ld", GPIOD_IN);

    if(IS_ERR(max2871->ld))
    {
        return dev_err_probe(&spi->dev, PTR_ERR(max2871->ld), "Failed to get LD GPIO\n");
    }

    // Write-to-all-Register process
    ret = max2871_write_all(max2871);

    if (ret != 0)
    {
        return ret;
    }

    msleep(20); // datasheet p.13: program twice, >= 20 ms apart

    ret = max2871_write_all(max2871);

    if (ret != 0)
    {
        dev_err(&spi->dev, "Failed to write registers: %d\n", ret);
        return ret;
    }

    dev_info(&spi->dev, "Successfully wrote all registers\n");

    //Read the value of the LD pin
    for(int i = 0; i < 10; i++)
    {
        ld_value = gpiod_get_value_cansleep(max2871->ld);
        if(ld_value)
        {
            break;
        }
        else
        {
            msleep(1);
        }
    }

    spi_set_drvdata(spi, max2871);
    return 0;  
}

static void max2871_spi_remove(struct spi_device *spi)
{
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

