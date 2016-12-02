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

DECLARE_GLOBAL_DATA_PTR;

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

#define ETH_PHY_POWER	IMX_GPIO_NR(4, 21)

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

/* For the BQ24133 charger */
#define BQ24133_CHRGR_OK	IMX_GPIO_NR(4, 1)



int dram_init(void)
{
	gd->ram_size = get_ram_size((void *)PHYS_SDRAM, PHYS_SDRAM_SIZE);

	return 0;
}

static iomux_v3_cfg_t const charger_pads[] = {
	/* CHRGR_OK */
	MX6_PAD_KEY_ROW4__GPIO_4_1 | MUX_PAD_CTRL(NO_PAD_CTRL),
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

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
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
			imx_iomux_v3_setup_multiple_pads(
				usdhc1_pads, ARRAY_SIZE(usdhc1_pads));
			gpio_direction_input(USDHC1_CD_GPIO);
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
			break;
		case 1:
			imx_iomux_v3_setup_multiple_pads(
				usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
			/*gpio_direction_input(USDHC2_CD_GPIO);*/
			usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
			break;
		case 2:
			imx_iomux_v3_setup_multiple_pads(
				usdhc3_pads, ARRAY_SIZE(usdhc3_pads));
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
		imx_iomux_v3_setup_multiple_pads(usdhc1_pads,
						 ARRAY_SIZE(usdhc1_pads));
		gpio_direction_input(USDHC1_CD_GPIO);
		usdhc_cfg[0].esdhc_base = USDHC1_BASE_ADDR;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		break;
	case 1:
		imx_iomux_v3_setup_multiple_pads(usdhc2_pads,
						 ARRAY_SIZE(usdhc2_pads));
		/*gpio_direction_input(USDHC2_CD_GPIO);*/
		usdhc_cfg[0].esdhc_base = USDHC2_BASE_ADDR;
		usdhc_cfg[0].max_bus_width = 4;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		break;
	case 2:
		imx_iomux_v3_setup_multiple_pads(usdhc3_pads,
						 ARRAY_SIZE(usdhc3_pads));
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

static int check_battery(void)
{
	int voltage, full_charge, charge, flags, percent;
	int ret;
	uint8_t message[2];

	I2C_SET_BUS(I2C_PMIC);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_VOLTAGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read voltage from fuel gauge\n");
		return -1;
	}
	voltage = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_CHARGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read charge from fuel gauge\n");
		return -1;
	}
	charge = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_FULL_CHARGE, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read full charge from fuel gauge\n");
		return -1;
	}
	full_charge = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_PERCENT, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read percent charge from fuel gauge\n");
		return -1;
	}
	percent = get_unaligned_le16(message);

	ret = i2c_read(BQ27441_I2C_ADDR, BQ27441_REG_FLAGS, 1, message, sizeof(message));
	if (ret) {
		printf("Failed to read full charge from fuel gauge\n");
		return -1;
	}
	flags = get_unaligned_le16(message);

	if (flags & BQ27441_FLAG_DISCHARGE) {
		printf("Battery discharging\n");
	}
	if (flags & BQ27441_FLAG_UNDERTEMP) {
		printf("Battery undertemp condition detected\n");
	}
	if (flags & BQ27441_FLAG_OVERTEMP) {
		printf("Battery overtemp condition detected\n");
	}
	if (flags & BQ27441_FLAG_FASTCHARGE) {
		printf("Battery fastcharge available\n");
	}
	if (flags & BQ27441_FLAG_FULLCHARGE) {
		printf("Battery full charge\n");
	}
	if (flags & BQ27441_FLAG_LOWCHARGE) {
		printf("Battery low charge\n");
	}
	if (flags & BQ27441_FLAG_CRITCHARGE) {
		printf("Battery critically low charge\n");
	}

	printf("Battery full charge: %d mAh\n", full_charge);
	printf("Battery charge: %d mAh\n", charge);
	printf("Battery voltage: %d mV\n", voltage);
	printf("Battery charge: %d%%\n", percent);
	return 0;
}

static int check_charger_status(void)
{
	return gpio_get_value(BQ24133_CHRGR_OK);
}


int power_init_board(void)
{
	struct pmic *p;
	unsigned int reg;
	u32 id;
	int ret;
	unsigned char offset, i, switch_num;

	/* Set up charger */
	imx_iomux_v3_setup_multiple_pads(charger_pads, ARRAY_SIZE(charger_pads));
	gpio_direction_input(BQ24133_CHRGR_OK);
	if (check_charger_status()) {
		printf("Charging\n");
	} else {
		printf("Not charging\n");
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

	/* Set SW1AB stanby volage to 0.975V */
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

	check_battery();

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
	imx_iomux_v3_setup_multiple_pads(usb_otg_pads,
					 ARRAY_SIZE(usb_otg_pads));
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
	imx_iomux_v3_setup_multiple_pads(key_pads, ARRAY_SIZE(key_pads));

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

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	if (check_keypress()) {
		printf("Magic key press detected, launching USB download mode\n");
		hab_rvt_failsafe(); /* This never returns, hopefully */
	}


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
	puts("Board: MX6SLEVK\n");

	return 0;
}

#ifdef CONFIG_SPL_BUILD
#include <spl.h>
#include <libfdt.h>

const struct mx6dq_iomux_ddr_regs mx6_ddr_ioregs = {
	.dram_sdclk_0 =  0x00020030,
	.dram_sdclk_1 =  0x00020030,
	.dram_cas =  0x00020030,
	.dram_ras =  0x00020030,
	.dram_reset =  0x00020030,
	.dram_sdcke0 =  0x00003000,
	.dram_sdcke1 =  0x00003000,
	.dram_sdba2 =  0x00000000,
	.dram_sdodt0 =  0x00003030,
	.dram_sdodt1 =  0x00003030,
	.dram_sdqs0 =  0x00000030,
	.dram_sdqs1 =  0x00000030,
	.dram_sdqs2 =  0x00000030,
	.dram_sdqs3 =  0x00000030,
	.dram_sdqs4 =  0x00000030,
	.dram_sdqs5 =  0x00000030,
	.dram_sdqs6 =  0x00000030,
	.dram_sdqs7 =  0x00000030,
	.dram_dqm0 =  0x00020030,
	.dram_dqm1 =  0x00020030,
	.dram_dqm2 =  0x00020030,
	.dram_dqm3 =  0x00020030,
	.dram_dqm4 =  0x00020030,
	.dram_dqm5 =  0x00020030,
	.dram_dqm6 =  0x00020030,
	.dram_dqm7 =  0x00020030,
};

const struct mx6sl_iomux_grp_regs mx6_grp_ioregs = {
	.grp_b0ds = 0x00000030,
	.grp_b1ds = 0x00000030,
	.grp_b2ds = 0x00000030,
	.grp_b3ds = 0x00000030,
	.grp_addds = 0x00000030,
	.grp_ctlds = 0x00000030,
	.grp_ddrmode_ctl = 0x00020000,
	.grp_ddrpke = 0x00000000,
	.grp_ddrmode = 0x00020000,
	.grp_ddr_type = 0x00080000,
};

const struct mx6_mmdc_calibration mx6_mmcd_calib = {
	.p0_mpdgctrl0 =  0x20000000,
	.p0_mpdgctrl1 =  0x00000000,
	.p0_mprddlctl =  0x4241444a,
	.p0_mpwrdlctl =  0x3030312b,
	.mpzqlp2ctl = 0x1b4700c7,
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

static void spl_dram_init(void)
{
	struct mx6_ddr_sysinfo sysinfo = {
		/* width of data bus:0=16,1=32,2=64 */
		.dsize = 2,
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
	mx6sl_dram_iocfg(32, &mx6_ddr_ioregs, &mx6_grp_ioregs);
	mx6_dram_cfg(&sysinfo, &mx6_mmcd_calib, &mem_ddr);
}

void board_init_f(ulong dummy)
{
	/* setup AIPS and disable watchdog */
	arch_cpu_init();

	ccgr_init();

	/* iomux and setup of i2c */
	board_early_init_f();

	/* setup GP timer */
	timer_init();

	/* UART clocks enabled and gd valid - init serial console */
	preloader_console_init();

	/* DDR initialization */
	spl_dram_init();

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	/* load/boot image from boot device */
	board_init_r(NULL, 0);
}
#endif
