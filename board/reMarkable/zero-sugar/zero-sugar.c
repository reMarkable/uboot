/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 * Copyright (C) 2019 reMarkable AS
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Lars Miljeteig <lars.ivar.miljeteig@remarkable.com>
 *
 */

#include "uart_init.h"
#include "epd_display_init.h"
#include "epd_pmic_init.h"
#include "digitizer_init.h"
#include "max77818.h"
#include "max77818_charger.h"
#include "serial_download_trap.h"

#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx7-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/io.h>
#include <linux/sizes.h>
#include <common.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <miiphy.h>
#include <netdev.h>
#include <power/pmic.h>
#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/crm_regs.h>
#include <command.h>
#include <dm/uclass.h>

#include <asm/mach-imx/video.h>

#include <fat.h>
#include <console.h>
#include <membuff.h>

#include <environment.h>
#include <asm/setup.h>
#include <asm/bootm.h>

#define SNVS_BASE_ADDR 0x30370000
#define SNVS_REG_LPCR SNVS_BASE_ADDR + 0x38
#define SNVS_MASK_POWEROFF (BIT(5) | BIT(6))

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_SIZE;

	return 0;
}

static iomux_v3_cfg_t const wdog_pads[] = {
	MX7D_PAD_ENET1_COL__WDOG1_WDOG_ANY | MUX_PAD_CTRL(NO_PAD_CTRL),
};

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

static void power_perfs(void)
{
	printk("Powering up peripherals\n");

	/* EPD */
	zs_do_config_epd_powerctrl_pins();
	zs_do_epd_power_on(NULL, 0, 0, NULL);
	udelay(10000);

	/* enable display and run init sequence */
	epd_display_init();

	// Shutdown LCDIF
	lcdif_power_down();

	/* DIGITIZER */
	zs_do_config_digitizer_powerctrl_pins();
}

static void init_charger(void)
{
	int ret;

	printf("Enabling SAFEOUT1\n");
	ret = max77818_enable_safeout1();
	if (ret != 0) {
		printf("%s: Failed to enable SAFEOUT1 regulator: %d\n",
		       __func__, ret);
	}

	printf("Setting minimal charger configuration\n");
	ret = max77818_set_minimal_charger_config();
	if (ret) {
		printf("%s: Failed to set charger config: %d\n",
		       __func__, ret);
	}
}

static void save_serial(void)
{
	struct tag_serialnr serialnr = {0};
	char snstr[22] = {0};
	u64 sn;
	int ret;

	if (env_get("serial#") != NULL)
		return;

	get_board_serial(&serialnr);

	if (serialnr.low == 0 && !serialnr.high == 0)
		return;

	sn = (u64)serialnr.low + ((u64)serialnr.high << 32);

	ret = snprintf(snstr, sizeof(snstr) - 1, "%llu", sn);
	if (ret <= 0) {
		printf("%s: Failed to write serial number to string\n", __func__);
		return;
	}

	ret = env_set("serial#", snstr);
	if (ret) {
		printf("%s: Failed to write serial number to environment\n", __func__);
		return;
	}

	if (env_save())
		printf("%s: Failed to save environment\n", __func__);
}

int board_early_init_f(void)
{
	setup_iomux_uart();
	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	return 0;
}

int board_late_init(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));
	set_wdog_reset(wdog);
	/*
	 * Do not assert internal WDOG_RESET_B_DEB(controlled by bit 4),
	 * since we use PMIC_PWRON to reset the board.
	 */
	clrsetbits_le16(&wdog->wcr, 0, 0x10);

	init_charger();
	probe_serial_download_trap();

	/*
	 * Enable icache and dcache: we need this to be able to show splash
	 * screen.
	 */
	icache_enable();
	dcache_enable();

	power_perfs();

	save_serial();

	return 0;
}
