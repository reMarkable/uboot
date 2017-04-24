/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2016 reMarkable AS
 *
 * Author: Fabio Estevam <fabio.estevam@freescale.com>
 * Author: Martin Sandsmark <martin.sandsmark@remarkable.no>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-ddr.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/imx-common/mxc_i2c.h>
#include <asm/imx-common/spi.h>
#include <asm/io.h>
#include <linux/sizes.h>
#include <common.h>
#include <fsl_esdhc.h>
#include <i2c.h>
#include <mmc.h>
#include <netdev.h>
#include <power/pmic.h>
#include <power/pfuze100_pmic.h>
#include <usb.h>
#include <usb/ehci-ci.h>

#include <lcd.h>
#include <mxc_epdc_fb.h>
#include <splash.h>

DECLARE_GLOBAL_DATA_PTR;

#define CHRGSTAT_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_47K_UP | PAD_CTL_HYS)

#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PUS_22K_UP |			\
	PAD_CTL_SPEED_LOW | PAD_CTL_DSE_80ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |             \
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED   |             \
	PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

#define I2C_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
		      PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |	\
		      PAD_CTL_DSE_40ohm | PAD_CTL_HYS |		\
		      PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define OTGID_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
			PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_LOW |\
			PAD_CTL_DSE_80ohm | PAD_CTL_HYS |	\
			PAD_CTL_SRE_FAST)

#define EPDC_PAD_CTRL    (PAD_CTL_PKE | PAD_CTL_SPEED_MED |	\
	PAD_CTL_DSE_40ohm | PAD_CTL_HYS)

/* GPIO keys */
#define GPIO_KEY_LEFT	IMX_GPIO_NR(3, 24)
#define GPIO_KEY_HOME	IMX_GPIO_NR(3, 26)
#define GPIO_KEY_RIGHT	IMX_GPIO_NR(3, 28)

/*
 * For handling the keypad
 */
#define KPP_KPCR 0x20B8000
#define KPP_KPSR 0x20B8002
#define KPP_KDDR 0x20B8004
#define KPP_KPDR 0x20B8006
#define KBD_STAT_KPKD	(0x1 << 0) /* Key Press Interrupt Status bit (w1c) */
#define KBD_STAT_KPKR	(0x1 << 1) /* Key Release Interrupt Status bit (w1c) */
#define KBD_STAT_KDSC	(0x1 << 2) /* Key Depress Synch Chain Status bit (w1c)*/
#define KBD_STAT_KRSS	(0x1 << 3) /* Key Release Synch Status bit (w1c)*/
#define KBD_STAT_KDIE	(0x1 << 8) /* Key Depress Interrupt Enable Status bit */
#define KBD_STAT_KRIE	(0x1 << 9) /* Key Release Interrupt Enable */
#define KBD_STAT_KPPEN	(0x1 << 10) /* Keypad Clock Enable */

/* For the bq27441 fuel gauge */
#define BQ27441_I2C_ADDR	0x55

#define BQ27441_REG_VOLTAGE	0x04
#define BQ27441_REG_CURRENT	0x10
#define BQ27441_REG_CHARGE	0x0c
#define BQ27441_REG_FULL_CHARGE	0x0e
#define BQ27441_REG_FLAGS	0x06
#define BQ27441_REG_PERCENT	0x1c

#define BQ27441_FLAG_DISCHARGE	BIT(0)
#define BQ27441_FLAG_UNDERTEMP	BIT(14)
#define BQ27441_FLAG_OVERTEMP	BIT(15)
#define BQ27441_FLAG_FASTCHARGE	BIT(8)
#define BQ27441_FLAG_FULLCHARGE	BIT(9)
#define BQ27441_FLAG_LOWCHARGE	BIT(2)
#define BQ27441_FLAG_CRITCHARGE	BIT(1)

#define SNVS_REG_LPCR		0x20CC038
#define SNVS_MASK_POWEROFF	(BIT(5) | BIT(6) | BIT(0))

/* For the BQ24133 charger */
#define BQ24133_CHRGR_OK	IMX_GPIO_NR(4, 1)
#define USB_POWER_UP		IMX_GPIO_NR(4, 6)


#define BATTERY_LEVEL_LOW	5
#define BATTERY_LEVEL_CRITICAL	2


int dram_init(void)
{
	gd->ram_size = get_ram_size((void *)PHYS_SDRAM, PHYS_SDRAM_SIZE);

	return 0;
}

static iomux_v3_cfg_t const charger_pads[] = {
	/* CHRGR_OK */
	MX6_PAD_KEY_ROW4__GPIO_4_1 | MUX_PAD_CTRL(CHRGSTAT_PAD_CTRL),
};

static iomux_v3_cfg_t const usb_power_up_pads[] = {
	/* USB_POWER_UP */
	MX6_PAD_KEY_COL7__GPIO_4_6 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const uart1_pads[] = {
	MX6_PAD_UART1_TXD__UART1_TXD | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_UART1_RXD__UART1_RXD | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc1_pads[] = {
	/* 4 bit SD */
	MX6_PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DAT0__USDHC1_DAT0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DAT1__USDHC1_DAT1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DAT2__USDHC1_DAT2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DAT3__USDHC1_DAT3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),

	/*CD pin*/
	MX6_PAD_KEY_ROW7__GPIO_4_7 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	MX6_PAD_SD2_RST__USDHC2_RST | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT0__USDHC2_DAT0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT1__USDHC2_DAT1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT2__USDHC2_DAT2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT3__USDHC2_DAT3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT4__USDHC2_DAT4 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT5__USDHC2_DAT5 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT6__USDHC2_DAT6 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT7__USDHC2_DAT7 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc3_pads[] = {
	MX6_PAD_SD3_CLK__USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD3_CMD__USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD3_DAT0__USDHC3_DAT0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD3_DAT1__USDHC3_DAT1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD3_DAT2__USDHC3_DAT2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD3_DAT3__USDHC3_DAT3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const key_pads[] = {
	MX6_PAD_KEY_ROW0__KEY_ROW0 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL0__KEY_COL0 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL1__KEY_COL1 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL2__KEY_COL2 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const gpio_key_pads[] = {
	MX6_PAD_KEY_COL0__GPIO_3_24 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL1__GPIO_3_26 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL2__GPIO_3_28 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const epdc_enable_pads[] = {
	MX6_PAD_EPDC_D0__EPDC_SDDO_0	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D1__EPDC_SDDO_1	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D2__EPDC_SDDO_2	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D3__EPDC_SDDO_3	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D4__EPDC_SDDO_4	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D5__EPDC_SDDO_5	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D6__EPDC_SDDO_6	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D7__EPDC_SDDO_7	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D8__EPDC_SDDO_8	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D9__EPDC_SDDO_9	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D10__EPDC_SDDO_10	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D11__EPDC_SDDO_11	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D12__EPDC_SDDO_12	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D13__EPDC_SDDO_13	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D14__EPDC_SDDO_14	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_D15__EPDC_SDDO_15	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_GDCLK__EPDC_GDCLK	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_GDSP__EPDC_GDSP	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_GDOE__EPDC_GDOE	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_GDRL__EPDC_GDRL	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDCLK__EPDC_SDCLK	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDOE__EPDC_SDOE	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDLE__EPDC_SDLE	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDSHR__EPDC_SDSHR	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_BDR0__EPDC_BDR_0	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDCE0__EPDC_SDCE_0	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDCE1__EPDC_SDCE_1	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
	MX6_PAD_EPDC_SDCE2__EPDC_SDCE_2	| MUX_PAD_CTRL(EPDC_PAD_CTRL),
};

static iomux_v3_cfg_t const epdc_disable_pads[] = {
	MX6_PAD_EPDC_D0__GPIO_1_7,
	MX6_PAD_EPDC_D1__GPIO_1_8,
	MX6_PAD_EPDC_D2__GPIO_1_9,
	MX6_PAD_EPDC_D3__GPIO_1_10,
	MX6_PAD_EPDC_D4__GPIO_1_11,
	MX6_PAD_EPDC_D5__GPIO_1_12,
	MX6_PAD_EPDC_D6__GPIO_1_13,
	MX6_PAD_EPDC_D7__GPIO_1_14,
	MX6_PAD_EPDC_D8__GPIO_1_15,
	MX6_PAD_EPDC_D9__GPIO_1_16,
	MX6_PAD_EPDC_D10__GPIO_1_17,
	MX6_PAD_EPDC_D11__GPIO_1_18,
	MX6_PAD_EPDC_D12__GPIO_1_19,
	MX6_PAD_EPDC_D13__GPIO_1_20,
	MX6_PAD_EPDC_D14__GPIO_1_21,
	MX6_PAD_EPDC_D15__GPIO_1_22,
	MX6_PAD_EPDC_GDCLK__GPIO_1_31,
	MX6_PAD_EPDC_GDSP__GPIO_2_2,
	MX6_PAD_EPDC_GDOE__GPIO_2_0,
	MX6_PAD_EPDC_GDRL__GPIO_2_1,
	MX6_PAD_EPDC_SDCLK__GPIO_1_23,
	MX6_PAD_EPDC_SDOE__GPIO_1_25,
	MX6_PAD_EPDC_SDLE__GPIO_1_24,
	MX6_PAD_EPDC_SDSHR__GPIO_1_26,
	MX6_PAD_EPDC_BDR0__GPIO_2_5,
	MX6_PAD_EPDC_SDCE0__GPIO_1_27,
	MX6_PAD_EPDC_SDCE1__GPIO_1_28,
	MX6_PAD_EPDC_SDCE2__GPIO_1_29,
};

static void setup_iomux_uart(void)
{
	SETUP_IOMUX_PADS(uart1_pads);
}

#define USDHC1_CD_GPIO	IMX_GPIO_NR(4, 7)
/*#define USDHC2_CD_GPIO	IMX_GPIO_NR(5, 0)*/
/*#define USDHC3_CD_GPIO	IMX_GPIO_NR(3, 22)*/

static struct fsl_esdhc_cfg usdhc_cfg[3] = {
	{USDHC1_BASE_ADDR, 0, 4},
	{USDHC2_BASE_ADDR, 0, 8},
	{USDHC3_BASE_ADDR, 0, 4},
};

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC1_BASE_ADDR:
		ret = !gpio_get_value(USDHC1_CD_GPIO);
		break;
	case USDHC2_BASE_ADDR:
		ret = 1;
		/*ret = !gpio_get_value(USDHC2_CD_GPIO);*/
		break;
	case USDHC3_BASE_ADDR:
		ret = 0;
		/*ret = !gpio_get_value(USDHC3_CD_GPIO);*/
		break;
	}

	return ret;
}

int board_mmc_init(bd_t *bis)
{
#ifndef CONFIG_SPL_BUILD
	int i, ret;

	/*
	 * According to the board_mmc_init() the following map is done:
	 * (U-Boot device node)    (Physical Port)
	 * mmc0                    USDHC1
	 * mmc1                    USDHC2
	 * mmc2                    USDHC3
	 */
	for (i = 0; i < CONFIG_SYS_FSL_USDHC_NUM; i++) {
		switch (i) {
		case 0:
			SETUP_IOMUX_PADS(usdhc1_pads);
			gpio_direction_input(USDHC1_CD_GPIO);
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
			break;
		case 1:
			SETUP_IOMUX_PADS(usdhc2_pads);
			/*gpio_direction_input(USDHC2_CD_GPIO);*/
			usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
			break;
		case 2:
			SETUP_IOMUX_PADS(usdhc3_pads);
			/*gpio_direction_input(USDHC3_CD_GPIO);*/
			usdhc_cfg[2].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
				"(%d) than supported by the board\n", i + 1);
			return -EINVAL;
			}

			ret = fsl_esdhc_initialize(bis, &usdhc_cfg[i]);
			if (ret) {
				printf("Warning: failed to initialize "
					"mmc dev %d\n", i);
				return ret;
			}
	}

	return 0;
#else
	struct src *src_regs = (struct src *)SRC_BASE_ADDR;
	u32 val;
	u32 port;

	val = readl(&src_regs->sbmr1);

	/* Boot from USDHC */
	port = (val >> 11) & 0x3;
	switch (port) {
	case 0:
		SETUP_IOMUX_PADS(usdhc1_pads);
		gpio_direction_input(USDHC1_CD_GPIO);
		usdhc_cfg[0].esdhc_base = USDHC1_BASE_ADDR;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		break;
	case 1:
		SETUP_IOMUX_PADS(usdhc2_pads);
		/*gpio_direction_input(USDHC2_CD_GPIO);*/
		usdhc_cfg[0].esdhc_base = USDHC2_BASE_ADDR;
		usdhc_cfg[0].max_bus_width = 4;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		break;
	case 2:
		SETUP_IOMUX_PADS(usdhc3_pads);
		/*gpio_direction_input(USDHC3_CD_GPIO);*/
		usdhc_cfg[0].esdhc_base = USDHC3_BASE_ADDR;
		usdhc_cfg[0].max_bus_width = 4;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		break;
	}

	gd->arch.sdhc_clk = usdhc_cfg[0].sdhc_clk;
	return fsl_esdhc_initialize(bis, &usdhc_cfg[0]);
#endif
}

#define PC	MUX_PAD_CTRL(I2C_PAD_CTRL)
/* I2C1 for PMIC */
struct i2c_pads_info i2c_pad_info1 = {
	.sda = {
		.i2c_mode = MX6_PAD_I2C1_SDA__I2C1_SDA | PC,
		.gpio_mode = MX6_PAD_I2C1_SDA__GPIO_3_13 | PC,
		.gp = IMX_GPIO_NR(3, 13),
	},
	.scl = {
		.i2c_mode = MX6_PAD_I2C1_SCL__I2C1_SCL | PC,
		.gpio_mode = MX6_PAD_I2C1_SCL__GPIO_3_12 | PC,
		.gp = IMX_GPIO_NR(3, 12),
	},
};
#undef PC

static void pfuze100_dump(struct pmic *p)
{
	unsigned int reg;

	pmic_reg_read(p, PFUZE100_DEVICEID, &reg);

	/* Interrupt registers, reason for poweroff etc. */
	pmic_reg_read(p, PFUZE100_INTSTAT0, &reg);
	printf("PMIC: INTSTAT0: %x\n", reg);
	pmic_reg_read(p, PFUZE100_INTSTAT1, &reg);
	printf("PMIC: INTSTAT1: %x\n", reg);
	pmic_reg_read(p, PFUZE100_INTSTAT3, &reg);
	printf("PMIC: INTSTAT3: %x\n", reg);
	pmic_reg_read(p, PFUZE100_INTSTAT4, &reg);
	printf("PMIC: INTSTAT4: %x\n", reg);

	/* Switch to extended register page for OTP SW4 voltage */
	reg = PFUZE100_PAGE_EXT1;
	pmic_reg_write(p, PFUZE100_PAGESELECT, reg);

	/* Read set SW4 voltage from OTP register */
	pmic_reg_read(p, PFUZE100_SW4OTPVOLT, &reg);
	printf("PMIC: SW4 OTP volt before: %x\n", reg);

	/* Check fuse settings */
	pmic_reg_read(p, PFUZE100_OTP_FUZE_POR1, &reg);
	printf("PMIC: POR1: %x\n", reg);
	pmic_reg_read(p, PFUZE100_OTP_FUZE_POR_XOR, &reg);
	printf("PMIC: POR XOR: %x\n", reg);

	/* Switch back to normal pages */
	reg = PFUZE100_PAGE_FUNC;
	pmic_reg_write(p, PFUZE100_PAGESELECT, reg);

	/* Read from normal register */
	pmic_reg_read(p, PFUZE100_SW4VOL, &reg);
	printf("First SW4 voltage from reg: %x\n", reg);
}

static void snvs_poweroff(void)
{
	writel(SNVS_MASK_POWEROFF, SNVS_REG_LPCR);
	while (1) {
		udelay(500000);
		printf("Should have halted!\n");
	}
}

int do_poweroff(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	snvs_poweroff();

	return 0;
}


/*
 * The percentage calculated by the fuel gauge isn't always very correct,
 * so calculate it manually as well to be sure.
 */
static int get_battery_charge(void)
{
	int ret, percent, charge_now, charge_full, charge_percent;
	uint8_t message[2];

	I2C_SET_BUS(I2C_PMIC);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_PERCENT, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read percent charge from fuel gauge\n");
		return -1;
	}

	percent = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_CHARGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read percent charge from fuel gauge\n");
		return -1;
	}
	charge_now = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_FULL_CHARGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read full charge from fuel gauge\n");
		return -1;
	}
	charge_full = get_unaligned_le16(message);

	charge_percent = 100 * charge_now / charge_full;


	if (percent < charge_percent) {
		return percent;
	} else {
		return charge_percent;
	}
}

static void battery_dump(void)
{
	int voltage, full_charge, charge;
	int ret;
	uint8_t message[2];

	I2C_SET_BUS(I2C_PMIC);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_VOLTAGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read voltage from fuel gauge\n");
		return;
	}
	voltage = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_CHARGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read charge from fuel gauge\n");
		return;
	}
	charge = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_FULL_CHARGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read full charge from fuel gauge\n");
		return;
	}
	full_charge = get_unaligned_le16(message);

	printf("Battery full charge: %d mAh\n", full_charge);
	printf("Battery charge: %d mAh\n", charge);
	printf("Battery voltage: %d mV\n", voltage);
}

static int check_battery(void)
{
	int ret, flags;
	uint8_t message[2];

	I2C_SET_BUS(I2C_PMIC);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_FLAGS, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read flags from fuel gauge\n");
		return -1;
	}
	flags = get_unaligned_le16(message);

	if (flags & BQ27441_FLAG_DISCHARGE) {
		printf("Battery discharging\n");
	}

	if (flags & BQ27441_FLAG_UNDERTEMP) {
		printf("Battery undertemp condition detected, powering off\n");
		return -1;
	}

	if (flags & BQ27441_FLAG_OVERTEMP) {
		printf("Battery overtemp condition detected\n");
		return -1;
	}

	if (flags & BQ27441_FLAG_FASTCHARGE) {
		printf("Battery fastcharge available\n");
	}

	if (flags & BQ27441_FLAG_CRITCHARGE) {
		printf("Battery critically low, powering off\n");
		return -1;
	} else if (flags & BQ27441_FLAG_LOWCHARGE) {
		printf("Battery low charge\n");
	} else if (flags & BQ27441_FLAG_FULLCHARGE) {
		printf("Battery full charge\n");
	}

	return 0;
}

static int check_charger_status(void)
{
	return gpio_get_value(BQ24133_CHRGR_OK);
}

int cmd_check_battery(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	check_battery();
	battery_dump();
	if (check_charger_status()) {
		printf("Charging\n");
	} else {
		printf("Not charging\n");
	}
	printf("Battery charge: %d%%\n", get_battery_charge());

	return 0;
}

U_BOOT_CMD(checkbattery, CONFIG_SYS_MAXARGS, 1, cmd_check_battery, "Check battery status", "")

static void wait_for_battery_charge(int minimum)
{
	int battery_charge;

	while ((battery_charge = get_battery_charge()) < minimum) {
		if (!check_charger_status()) {
			printf("Battery low, not charging, powering off\n");
			snvs_poweroff();
		}

		printf("Battery critical, charging, current charge: %d%%, target: %d%%\n", battery_charge, minimum);
		/* Sleep for one second */
		udelay(1000000);
	}
}

int power_init_board(void)
{
	struct pmic *p;
	unsigned int reg;
	u32 id;
	int ret;
	unsigned char offset, i, switch_num;

	/* Disable the circuitry that automatically
	 * powers up the board when USB is plugged in */
	SETUP_IOMUX_PADS(usb_power_up_pads);
	gpio_request(USB_POWER_UP, "usb_power_up");
	gpio_direction_output(USB_POWER_UP, 1);
	/*udelay(500000);*/

	/* Set up charger */
	SETUP_IOMUX_PADS(charger_pads);
	gpio_request(BQ24133_CHRGR_OK, "bq24133_chrgr_ok");
	gpio_direction_input(BQ24133_CHRGR_OK);
	if (check_charger_status()) {
		printf("Charging\n");
	} else {
		printf("Not charging\n");
	}

	wait_for_battery_charge(BATTERY_LEVEL_CRITICAL);
	/* Critical battery */
/*	while ((battery_charge = get_battery_charge()) < BATTERY_LEVEL_CRITICAL) {
		if (!check_charger_status()) {
			printf("Battery critical, not charging, powering off\n");
			snvs_poweroff();
		}

		printf("Battery critical, charging, current charge: %d%%\n", battery_charge);
		udelay(1000000);
	}*/

	if (check_battery() < 0) {
		printf("Error when checking battery state, powering off\n");
		snvs_poweroff();
	}

	ret = power_pfuze100_init(I2C_PMIC);
	if (ret)
		return ret;

	/* Setup PF0100 */
	p = pmic_get("PFUZE100");
	ret = pmic_probe(p);
	if (ret) {
		printf("PMIC: Unable to find PFUZE100!\n");
		return ret;
	}

	/* Get ID, verify it is a supported version */
	pmic_reg_read(p, PFUZE100_DEVICEID, &id);
	id = id & 0xf;
	printf("PMIC: PFUZE100 ID=0x%02x\n", id);
	if (id == 0) {
		switch_num = 6;
		offset = PFUZE100_SW1CMODE;
	} else if (id == 1) {
		switch_num = 4;
		offset = PFUZE100_SW2MODE;
	} else {
		printf("PMIC: PFUZE100 ID not supported, id=%d\n", id);
		return -EINVAL;
	}

	/* Set SW1AB standby voltage to 0.975V */
	pmic_reg_read(p, PFUZE100_SW1ABSTBY, &reg);
	reg &= ~SW1x_STBY_MASK;
	reg |= SW1x_0_975V;
	pmic_reg_write(p, PFUZE100_SW1ABSTBY, reg);

	/* Set SW1AB/VDDARM step ramp up time from 16us to 4us/25mV */
	pmic_reg_read(p, PFUZE100_SW1ABCONF, &reg);
	reg &= ~SW1xCONF_DVSSPEED_MASK;
	reg |= SW1xCONF_DVSSPEED_4US;
	pmic_reg_write(p, PFUZE100_SW1ABCONF, reg);

	/* Set SW1C standby voltage to 0.975V */
	pmic_reg_read(p, PFUZE100_SW1CSTBY, &reg);
	reg &= ~SW1x_STBY_MASK;
	reg |= SW1x_0_975V;
	pmic_reg_write(p, PFUZE100_SW1CSTBY, reg);

	/* Set SW1C/VDDSOC step ramp up time from 16us to 4us/25mV */
	pmic_reg_read(p, PFUZE100_SW1CCONF, &reg);
	reg &= ~SW1xCONF_DVSSPEED_MASK;
	reg |= SW1xCONF_DVSSPEED_4US;
	pmic_reg_write(p, PFUZE100_SW1CCONF, reg);

	/* Set 3V3_SW4 voltage to 3.3V */
	pmic_reg_read(p, PFUZE100_SW4VOL, &reg);
	reg &= ~SW4_VOL_MASK;
	reg |= SW4_3_300V;
	pmic_reg_write(p, PFUZE100_SW4VOL, reg);

	/* Set 3V3_VGEN6 voltage to 3.3V */
	pmic_reg_read(p, PFUZE100_VGEN6VOL, &reg);
	reg &= ~LDO_VOL_MASK;
	reg |= LDOB_3_30V;
	pmic_reg_write(p, PFUZE100_VGEN6VOL, reg);

	/* Set modes */
	ret = pmic_reg_write(p, PFUZE100_SW1ABMODE, APS_PFM);
	if (ret < 0) {
		printf("Set SW1AB mode error!\n");
		return ret;
	}

	for (i = 0; i < switch_num - 1; i++) {
		ret = pmic_reg_write(p, offset + i * SWITCH_SIZE, APS_PFM);
		if (ret < 0) {
			printf("Set switch 0x%x mode error!\n",
			       offset + i * SWITCH_SIZE);
			return ret;
		}
	}

	/* Dump some values from registers for debugging */
	pfuze100_dump(p);


	return ret;
}

#ifdef CONFIG_USB_EHCI_MX6
#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)

static iomux_v3_cfg_t const usb_otg_pads[] = {
	/* OTG1 */
	MX6_PAD_KEY_COL4__USB_USBOTG1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EPDC_PWRCOM__ANATOP_USBOTG1_ID | MUX_PAD_CTRL(OTGID_PAD_CTRL),
	/* OTG2 */
	MX6_PAD_KEY_COL5__USB_USBOTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)
};

static void setup_usb(void)
{
	SETUP_IOMUX_PADS(usb_otg_pads);
}

int board_usb_phy_mode(int port)
{
	if (port == 1)
		return USB_INIT_HOST;
	else
		return usb_phy_mode(port);
}

int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 1)
		return -EINVAL;

	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);

	/* Set Power polarity */
	setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);

	return 0;
}
#endif

int board_early_init_f(void)
{
	enable_uart_clk(1);
	setup_iomux_uart();
#ifdef CONFIG_MXC_SPI
	setup_spi();
#endif
	return 0;
}

int board_late_init(void)
{
	wait_for_battery_charge(BATTERY_LEVEL_LOW);

	/*int battery_charge;
	while ((battery_charge = get_battery_charge()) < BATTERY_LEVEL_LOW) {
		if (!check_charger_status()) {
			printf("Battery low, not charging, powering off\n");
			snvs_poweroff();
		}

		printf("Battery low, charging, current charge: %d%%\n", battery_charge);
		udelay(1000000);
	}*/

	return 0;
}

static struct splash_location splash_locations[] = {
	{
		.name = "mmc_fs",
		.storage = SPLASH_STORAGE_MMC,
		.flags = SPLASH_STORAGE_FS,
		.devpart = "1:1",
	},
};

int splash_screen_prepare(void)
{
	return splash_source_load(splash_locations, ARRAY_SIZE(splash_locations));
}

vidinfo_t panel_info = {
	.vl_col = 1872,
	.vl_row = 1404,
	.vl_left_margin = 32,
	.vl_right_margin = 326,
	.vl_upper_margin = 4,
	.vl_lower_margin = 12,
	.vl_hsync = 44,
	.vl_vsync = 1,
	.vl_bpix = 3,
	.cmap = 0,
};

struct epdc_timing_params panel_timings = {
	.vscan_holdoff = 4,
	.sdoed_width = 10,
	.sdoed_delay = 20,
	.sdoez_width = 10,
	.sdoez_delay = 20,
	.gdclk_hp_offs = 1042,
	.gdsp_offs = 768,
	.gdoe_offs = 0,
	.gdclk_offs = 91,
	.num_ce = 1,
};

static void epdc_enable_pins(void)
{
	/* epdc iomux settings */
	SETUP_IOMUX_PADS(epdc_enable_pads);
}

static void epdc_disable_pins(void)
{
	/* Configure MUX settings for EPDC pins to GPIO  and drive to 0 */
	SETUP_IOMUX_PADS(epdc_disable_pads);
}

static void setup_epdc(void)
{
	unsigned int reg;
	struct mxc_ccm_reg *ccm_regs = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/*** EPDC Maxim PMIC settings ***/
	/* EPDC_PWRSTAT - GPIO2[13] for PWR_GOOD status */
	imx_iomux_v3_setup_pad(MX6_PAD_EPDC_PWRSTAT__GPIO_2_13 |
				MUX_PAD_CTRL(EPDC_PAD_CTRL));
	gpio_direction_input(IMX_GPIO_NR(2, 13));

	/* EPDC_VCOM0 - GPIO2[3] for VCOM control */
	imx_iomux_v3_setup_pad(MX6_PAD_EPDC_VCOM0__GPIO_2_3 |
				MUX_PAD_CTRL(EPDC_PAD_CTRL));
	gpio_direction_output(IMX_GPIO_NR(2, 3), 0);

	/* EPDC_PWRWAKEUP - GPIO2[14] for EPD PMIC WAKEUP */
	imx_iomux_v3_setup_pad(MX6_PAD_EPDC_PWRWAKEUP__GPIO_2_14 |
				MUX_PAD_CTRL(EPDC_PAD_CTRL));
	gpio_direction_output(IMX_GPIO_NR(2, 14), 0);

	/*** Set pixel clock rates for EPDC ***/

	/* EPDC AXI clk from PFD_400M, set to 396/2 = 198MHz */
	reg = readl(&ccm_regs->chsccdr);
	reg &= ~0x3F000;
	reg |= (0x4 << 15) | (1 << 12);
	writel(reg, &ccm_regs->chsccdr);

	/* EPDC AXI clk enable */
	reg = readl(&ccm_regs->CCGR3);
	reg |= (3 << 4);
	writel(reg, &ccm_regs->CCGR3);

	/* EPDC PIX clk from PFD_480M (PLL3), set to 480/3 = 160MHz */
	reg = readl(&ccm_regs->cscdr2);
	reg &= ~0x03F000;
	reg |= (5 << 15) | (2 << 12);
	writel(reg, &ccm_regs->cscdr2);

	/* EPDC PIX clk post divider, set to 1 */
	reg = readl(&ccm_regs->cbcmr);
	reg &= ~0x03800000;
	/*reg |= (0x0 << 23);*/
	writel(reg, &ccm_regs->cbcmr);

	/* EPDC PIX clk enable */
	reg = readl(&ccm_regs->CCGR3);
	reg |= 0x0C00;
	writel(reg, &ccm_regs->CCGR3);

	panel_info.epdc_data.wv_modes.mode_init = 0;
	panel_info.epdc_data.wv_modes.mode_du = 1;
	panel_info.epdc_data.wv_modes.mode_gc4 = 3;
	panel_info.epdc_data.wv_modes.mode_gc8 = 2;
	panel_info.epdc_data.wv_modes.mode_gc16 = 2;
	panel_info.epdc_data.wv_modes.mode_gc32 = 2;

	panel_info.epdc_data.epdc_timings = panel_timings;
}

void epdc_power_on(void)
{
	unsigned int reg;
	int tries = 10;

	printf("Powering on EPDC\n");

	/* Enable epdc signal pin */
	epdc_enable_pins();

	/* Set PMIC Wakeup to high - enable Display power */
	gpio_set_value(IMX_GPIO_NR(2, 14), 1);

	/* Wait for PWRGOOD == 1 */
	while (tries--) {
		reg = gpio_get_value(IMX_GPIO_NR(2, 13));
		if (reg) {
			break;
		}

		/*reg = readl(&gpio_regs->gpio_psr);
		if (!(reg & (1 << 13)))
			break;*/

		udelay(1000);
	}
	if (!tries) {
		printf("Failed to bring up display power\n");
	}
	printf("EPDC powered up, enabling VCOM\n");

	/* Enable VCOM */
	gpio_set_value(IMX_GPIO_NR(2, 3), 1);
	printf("VCOM up\n");

	udelay(500);
}

void epdc_power_off(void)
{
	printf("Powering off EPDC\n");
	/* Set PMIC Wakeup to low - disable Display power */
	gpio_set_value(IMX_GPIO_NR(2, 14), 0);

	/* Disable VCOM */
	gpio_set_value(IMX_GPIO_NR(2, 3), 0);

	epdc_disable_pins();

	/* Set EPD_PWR_CTL0 to low - disable EINK_VDD (3.15) */
	/*gpio_set_value(IMX_GPIO_NR(2, 7), 0);*/
}

/*
 * Sets up GPIO keys and checks for magic key combo
 */
static int check_gpio_keypress(void)
{
	int left, home, right;

	/* Set up pins */
	SETUP_IOMUX_PADS(gpio_key_pads);

	gpio_request(GPIO_KEY_LEFT, "key_left");
	gpio_direction_input(GPIO_KEY_LEFT);
	gpio_request(GPIO_KEY_HOME, "key_home");
	gpio_direction_input(GPIO_KEY_HOME);
	gpio_request(GPIO_KEY_RIGHT, "key_right");
	gpio_direction_input(GPIO_KEY_RIGHT);

	left = gpio_get_value(GPIO_KEY_LEFT);
	home = gpio_get_value(GPIO_KEY_HOME);
	right = gpio_get_value(GPIO_KEY_RIGHT);

	gpio_free(GPIO_KEY_LEFT);
	gpio_free(GPIO_KEY_HOME);
	gpio_free(GPIO_KEY_RIGHT);

	/* It is supposed to be just the home button, but sometimes the values
	 * are inverted */
	return (left == right && left != home);
}

/*
 * Sets up the keypad and then checks for a magic key combo
 */
static int check_keypress(void)
{
	unsigned short reg_val;
	int col;

	/* Array of pressed keys */
	int pressed[3] = { 0 };

	/* Masks for enabled rows/cols */
	unsigned short rows_en_mask = 0x1;
	unsigned short cols_en_mask = 0x7;

	/* Set up pins */
	SETUP_IOMUX_PADS(key_pads);

	/* Initialize keypad */
	/*
	 * Include enabled rows in interrupt generation (KPCR[7:0])
	 * Configure keypad columns as open-drain (KPCR[15:8])
	 */
	reg_val = readw(KPP_KPCR);
	reg_val |= rows_en_mask & 0xff;		/* rows */
	reg_val |= (cols_en_mask & 0xff) << 8;	/* cols */
	writew(reg_val, KPP_KPCR);

	/* Write 0's to KPDR[15:8] (Colums) */
	reg_val = readw(KPP_KPDR);
	reg_val &= 0x00ff;
	writew(reg_val, KPP_KPDR);

	/* Configure columns as output, rows as input (KDDR[15:0]) */
	writew(0xff00, KPP_KDDR);

	/*
	 * Clear Key Depress and Key Release status bit.
	 * Clear both synchronizer chain.
	 */
	reg_val = readw(KPP_KPSR);
	reg_val |= KBD_STAT_KPKR | KBD_STAT_KPKD |
		   KBD_STAT_KDSC | KBD_STAT_KRSS;
	writew(reg_val, KPP_KPSR);

	/* Enable KDI and disable KRI (avoid false release events). */
	reg_val |= KBD_STAT_KDIE;
	reg_val &= ~KBD_STAT_KRIE;
	writew(reg_val, KPP_KPSR);

	for (col = 0; col < 3; col++) {
		/*
		 * Discharge keypad capacitance:
		 * 2. write 1s on column data.
		 * 3. configure columns as totem-pole to discharge capacitance.
		 * 4. configure columns as open-drain.
		 */
		reg_val = readw(KPP_KPDR);
		reg_val |= 0xff00;
		writew(reg_val, KPP_KPDR);

		reg_val = readw(KPP_KPCR);
		reg_val &= ~((cols_en_mask & 0xff) << 8);
		writew(reg_val, KPP_KPCR);

		udelay(2);

		reg_val = readw(KPP_KPCR);
		reg_val |= (cols_en_mask & 0xff) << 8;
		writew(reg_val, KPP_KPCR);

		/*
		 * 5. Write a single column to 0, others to 1.
		 * 6. Sample row inputs and save data.
		 * 7. Repeat steps 2 - 6 for remaining columns.
		 */
		reg_val = readw(KPP_KPDR);
		reg_val &= ~(1 << (8 + col));
		writew(reg_val, KPP_KPDR);

		reg_val = readw(KPP_KPDR);
		pressed[col] = (~reg_val) & 1;
		printf("key: %d, pressed: %d\n", col, pressed[col]);
	}

	/* Check for magic key sequence */
	if (pressed[0] == 0 && pressed[1] == 1 && pressed[2] == 0) {
		return 1;
	} else {
		return 0;
	}
}

/* For jumping to USB serial download mode */
typedef void hab_rvt_failsafe_t(void);
#define HAB_RVT_FAILSAFE (*(uint32_t *) 0x000000BC)
#define hab_rvt_failsafe ((hab_rvt_failsafe_t *) HAB_RVT_FAILSAFE)

int cmd_hab_failsafe(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	hab_rvt_failsafe();
	return 0;
}

U_BOOT_CMD(habfailsafe, CONFIG_SYS_MAXARGS, 1, cmd_hab_failsafe, "jump to ROM failsafe code", "")

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	if (check_keypress() || check_gpio_keypress()) {
		printf("Magic key press detected, launching USB download mode\n");
		hab_rvt_failsafe(); /* This never returns, hopefully */
	}

	setup_epdc();

#ifdef CONFIG_SYS_I2C_MXC
	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info1);
#endif

#ifdef CONFIG_USB_EHCI_MX6
	setup_usb();
#endif

	return 0;
}

int checkboard(void)
{
	puts("Board: Zero Gravitas\n");

	return 0;
}

#ifdef CONFIG_SPL_BUILD
#include <spl.h>
#include <libfdt.h>

const struct mx6sl_iomux_ddr_regs mx6_ddr_ioregs = {
	.dram_sdclk_0 =  0x00000030,

	.dram_cas =  0x00000030,
	.dram_ras =  0x00000030,

	.dram_reset =  0x00000030,
	.dram_sdba2 =  0x00000030,

	.dram_sdqs0 =  0x00000030,
	.dram_sdqs1 =  0x00000030,

	.dram_sdqs2 =  0x00000030,
	.dram_sdqs3 =  0x00000030,

	.dram_dqm0 =  0x00020030,
	.dram_dqm1 =  0x00020030,

	.dram_dqm2 =  0x00020030,
	.dram_dqm3 =  0x00020030,
};

const struct mx6sl_iomux_grp_regs mx6_grp_ioregs = {
	.grp_ddr_type = 0x000C0000,
	.grp_ddrpke = 0x00000000,

	.grp_addds = 0x00000030,

	.grp_ctlds = 0x00000030,

	.grp_ddrmode_ctl = 0x00020000,

	.grp_ddrmode = 0x00020000,

	.grp_b0ds = 0x00000030,
	.grp_b1ds = 0x00000030,

	.grp_b2ds = 0x00000030,
	.grp_b3ds = 0x00000030,
};

const struct mx6_mmdc_calibration mx6_mmcd_calib = {
	.p0_mpwldectrl0 = 0x002D0028,
	.p0_mpwldectrl1 = 0x00280028,
	.p0_mpdgctrl0 = 0x420C0204,
	.p0_mpdgctrl1 = 0x01700168,
	.p0_mprddlctl = 0x3E3E4446,
	.p0_mpwrdlctl = 0x38343830,
};

/* MT41K128M16JT-125 */
static struct mx6_ddr3_cfg mem_ddr = {
	.mem_speed = 1600,
	.density = 2,
	.width = 16,
	.banks = 8,
	.rowaddr = 14,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};

static struct mx6_ddr_sysinfo ddr_info = {
	/* width of data bus:0=16,1=32,2=64 */
	.dsize = 1,
	/* config for full 4GB range so that get_mem_size() works */
	.cs_density = 32, /* 32Gb per CS */
	/* single chip select */
	.ncs = 1,
	.cs1_mirror = 0,
	.rtt_wr = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Wr = RZQ/4 */
	.rtt_nom = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Nom = RZQ/4 */
	.walat = 1,	/* Write additional latency */
	.ralat = 5,	/* Read additional latency */
	.mif3_mode = 3,	/* Command prediction working mode */
	.bi_on = 1,	/* Bank interleaving enabled */
	.sde_to_rst = 0x10,	/* 14 cycles, 200us (JEDEC default) */
	.rst_to_cke = 0x23,	/* 33 cycles, 500us (JEDEC default) */
	.ddr_type = DDR_TYPE_DDR3,
};

static void ccgr_init(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	writel(0xFFFFFFFF, &ccm->CCGR0);
	writel(0xFFFFFFFF, &ccm->CCGR1);
	writel(0xFFFFFFFF, &ccm->CCGR2);
	writel(0xFFFFFFFF, &ccm->CCGR3);
	writel(0xFFFFFFFF, &ccm->CCGR4);
	writel(0xFFFFFFFF, &ccm->CCGR5);
	writel(0xFFFFFFFF, &ccm->CCGR6);

	writel(0x00260324, &ccm->cbcmr);
}

void board_boot_order(u32 *spl_boot_list)
{
	/* From SoC boot config pins */
	spl_boot_list[0] = spl_boot_device();

	/* Default location */
	spl_boot_list[1] = BOOT_DEVICE_MMC1;

	/* Fall back to FSL USB serial download mode */
	spl_boot_list[3] = BOOT_DEVICE_BOARD;
}

int spl_board_load_image(void)
{
	debug("Jumping to HAB failsafe USB download mode...\n");
	hab_rvt_failsafe();

	return 0;
}


void board_init_f(ulong dummy)
{
	/* setup AIPS and disable watchdog */
	arch_cpu_init();

	ccgr_init();

	enable_uart_clk(1);
	setup_iomux_uart();

	/* setup GP timer */
	timer_init();

	/* DDR initialization */
	mx6sl_dram_iocfg(32, &mx6_ddr_ioregs, &mx6_grp_ioregs);
	mx6_dram_cfg(&ddr_info, &mx6_mmcd_calib, &mem_ddr);

	/* load/boot image from boot device */
	board_init_r(NULL, 0);
}
#endif
