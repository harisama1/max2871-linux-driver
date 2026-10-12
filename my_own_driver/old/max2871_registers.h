#ifndef MAX2871_REGISTERS_H
#define MAX2871_REGISTERS_H
#include <linux/types.h>
#include <linux/bits.h>                     

#define MAX2871_NUM_REGS	6

// Regiter 0 bit mask
#define MAX2871_ADDR_MASK       GENMASK(2, 0)   
#define MAX2871_REG0_INT        BIT(31)         
#define MAX2871_REG0_N          GENMASK(30, 15) 
#define MAX2871_REG0_FRAC       GENMASK(14, 3)   

// Register 1 bit mask
#define MAX2871_REG1_CPL        GENMASK(30, 29)
#define MAX2871_REG1_CPT        GENMASK(28, 27)
#define MAX2871_REG1_P          GENMASK(26, 15)
#define MAX2871_REG1_M          GENMASK(14, 3)

// Register 2 bit mask
#define MAX2871_REG2_LDS        BIT(31)
#define MAX2871_REG2_SDN        GENMASK(30, 29)
#define MAX2871_REG2_MUX        GENMASK(28, 26)
#define MAX2871_REG2_DBR        BIT(25)
#define MAX2871_REG2_RDIV2      BIT(24)
#define MAX2871_REG2_R          GENMASK(23, 14)
#define MAX2871_REG2_REG4DB     BIT(13)
#define MAX2871_REG2_CP         GENMASK(12, 9)
#define MAX2871_REG2_LDF        BIT(8)
#define MAX2871_REG2_LDP        BIT(7)
#define MAX2871_REG2_PDP        BIT(6)
#define MAX2871_REG2_SHDN       BIT(5)
#define MAX2871_REG2_TRI        BIT(4)
#define MAX2871_REG2_RST        BIT(3)

// Register 3 bit mask
#define MAX2871_REG3_VCO        GENMASK(31, 26)
#define MAX2871_REG3_VAS_SHDN   BIT(25)
#define MAX2871_REG3_VAS_TEMP   BIT(24)
#define MAX2871_REG3_CSM        BIT(18)
#define MAX2871_REG3_MUTEDEL    BIT(17)
#define MAX2871_REG3_CDM        GENMASK(16, 15)
#define MAX2871_REG3_CDIV       GENMASK(14, 3)

// Register 4 bit mask
#define MAX2871_REG4_RESERVED   GENMASK(31, 29)
#define MAX2871_REG4_SDLDO      BIT(28)
#define MAX2871_REG4_SDDIV      BIT(27)
#define MAX2871_REG4_SDREF      BIT(26)
#define MAX2871_REG4_BS_MSB     GENMASK(25, 24)
#define MAX2871_REG4_FB         BIT(23)
#define MAX2871_REG4_DIVA       GENMASK(22, 20)
#define MAX2871_REG4_BS_LSB     GENMASK(19, 12)
#define MAX2871_REG4_SDVCO      BIT(11)
#define MAX2871_REG4_MTLD       BIT(10)
#define MAX2871_REG4_BDIV       BIT(9)
#define MAX2871_REG4_RFB_EN     BIT(8)
#define MAX2871_REG4_BPWR       GENMASK(7, 6)
#define MAX2871_REG4_RFA_EN     BIT(5)
#define MAX2871_REG4_APWR       GENMASK(4, 3)

// Register 5 bit mask
#define MAX2871_REG5_VAS_DLY    GENMASK(30, 29) 
#define MAX2871_REG5_SDPLL      BIT(25)
#define MAX2871_REG5_F01        BIT(24)
#define MAX2871_REG5_LD         GENMASK(23, 22)
#define MAX2871_REG5_MUX3       BIT(18)
#define MAX2871_REG5_ADCS       BIT(6)
#define MAX2871_REG5_ADCM       GENMASK(5, 3)

void max2871RegsInit(void);
uint32_t max2871GetRegister(uint8_t r);

void max2871Set_INT(uint32_t j);
void max2871Set_N(uint32_t j);
void max2871Set_FRAC(uint32_t j);
void max2871Set_CPOC(uint32_t j);
void max2871Set_CPL(uint32_t j);
void max2871Set_CPT(uint32_t j);
void max2871Set_P(uint32_t j);
void max2871Set_M(uint32_t j);
void max2871Set_LDS(uint32_t j);
void max2871Set_SDN(uint32_t j);
void max2871Set_MUX(uint32_t j);
void max2871Set_DBR(uint32_t j);
void max2871Set_RDIV2(uint32_t j);
void max2871Set_R(uint32_t j);
void max2871Set_REG4DB(uint32_t j);
void max2871Set_CP(uint32_t j);
void max2871Set_LDF(uint32_t j);
void max2871Set_LDP(uint32_t j);
void max2871Set_PDP(uint32_t j);
void max2871Set_SHDN(uint32_t j);
void max2871Set_TRI(uint32_t j);
void max2871Set_RST(uint32_t j);
void max2871Set_VCO(uint32_t j);
void max2871Set_VAS_SHDN(uint32_t j);
void max2871Set_VAS_TEMP(uint32_t j);
void max2871Set_CSM(uint32_t j);
void max2871Set_MUTEDEL(uint32_t j);
void max2871Set_CDM(uint32_t j);
void max2871Set_CDIV(uint32_t j);
void max2871Set_SDLDO(uint32_t j);
void max2871Set_SDDIV(uint32_t j);
void max2871Set_SDREF(uint32_t j);
void max2871Set_BS(uint32_t j);
void max2871Set_FB(uint32_t j);
void max2871Set_DIVA(uint32_t j);
void max2871Set_SDVCO(uint32_t j);
void max2871Set_MTLD(uint32_t j);
void max2871Set_BDIV(uint32_t j);
void max2871Set_RFB_EN(uint32_t j);
void max2871Set_BPWR(uint32_t j);
void max2871Set_RFA_EN(uint32_t j);
void max2871Set_APWR(uint32_t j);
void max2871Set_SDPLL(uint32_t j);
void max2871Set_F01(uint32_t j);
void max2871Set_LD(uint32_t j);
void max2871Set_Reserved(void);
void max2871Set_ADCS(uint32_t j);
void max2871Set_ADCM(uint32_t j);

#endif 
