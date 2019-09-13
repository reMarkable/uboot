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
#include "charger_init.h"
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

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_SIZE;

	return 0;
}

static iomux_v3_cfg_t const wdog_pads[] = {
	MX7D_PAD_ENET1_COL__WDOG1_WDOG_ANY | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void power_perfs(void)
{
    printf("\n----------------------------------------------\n");
	printk("Powering up peripherals\n");
    printf("----------------------------------------------\n");

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

static int init_charger(void)
{
    int ret;
    printf("----------------------------------------------\n");
    printf("Initiating charging parameters\n");
    ret = max77818_init_device();
    if (ret != 0)
        return ret;

    ret = max77818_enable_safeout1();
    if (ret != 0) {
        printf("failed to enable SAFEOUT1 regulator: %d\n", ret);
        return ret;
    }

    printf("----------------------------------------------\n");
    printf("Setting fast charge current: 2.8A\n");
    ret = max77818_set_fast_charge_current_2800_ma(NULL);
    if (ret != 0)
        return ret;

    printf("----------------------------------------------\n");
    printf("Setting pogo input current limit: 2.8A\n");
    ret = max77818_set_pogo_input_current_limit(NULL);
    if (ret != 0)
        return ret;

    printf("----------------------------------------------\n");
    printf("Setting USB-C input current limit: 1.2A\n");
    ret = max77818_set_usbc_input_current_limit(NULL);
    if (ret != 0)
        return ret;

    printf("----------------------------------------------\n");
    printf("Setting charge termination voltage: 3.4V\n");
    ret = max77818_set_charge_termination_voltage(NULL);
    if (ret != 0)
        return ret;

    return 0;
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

    /* Try to store console log so far to mmc 0:1 */
//    printf("Initializing dummy buffer to be written to file ..\n");

//    struct membuff buf;
//    membuff_new(&buf, 100);
//    membuff_put(&buf, "This is a test !!!", 18);

//    printf("Trying to write to mmc 0:1 /uboot_console_log ..\n");
//    if(membuff_avail(&gd->console_out)) {
//        fat_fswrite_mem("uboot_console_log", &gd->console_out, 10);
//    }

    return 0;
}
