/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 * Copyright 2016 reMarkable AS
 *
 * Configuration settings for the reMarkable
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ZERO_GRAVITAS_H
#define __ZERO_GRAVITAS_H

#include "mx6_common.h"

#ifdef CONFIG_SPL
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_BOARD_LOAD_IMAGE
#include "imx6_spl.h"
#endif

#define CONFIG_SUPPORT_EMMC_BOOT /* eMMC specific */

#define CONFIG_CMD_POWEROFF

#define CONFIG_MXC_EPDC
#define CONFIG_WAVEFORM_BUF_SIZE	SZ_4M
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SOURCE
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_LCD
#define CONFIG_CMD_BMP
#define LCD_BPP				LCD_COLOR16

/* Size of malloc() pool, needs space for EPDC working buffer */
#define CONFIG_SYS_MALLOC_LEN		(SZ_32M)

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_LATE_INIT

#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART1_IPS_BASE_ADDR

/* MMC Configs */
#define CONFIG_SYS_FSL_ESDHC_ADDR	USDHC2_BASE_ADDR

/* Enable reading of serial tag from OCOTP */
#define CONFIG_SERIAL_TAG

/* I2C Configs */
#define CONFIG_SYS_I2C
#define CONFIG_SYS_I2C_MXC
#define CONFIG_SYS_I2C_MXC_I2C1		/* enable I2C bus 1 */
#define CONFIG_SYS_I2C_MXC_I2C2		/* enable I2C bus 2 */
#define CONFIG_SYS_I2C_MXC_I2C3		/* enable I2C bus 3 */
#define CONFIG_SYS_I2C_SPEED		  100000

/* PMIC */
#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_PFUZE100
#define CONFIG_POWER_PFUZE100_I2C_ADDR	0x08

#define CONFIG_EXTRA_ENV_SETTINGS \
	"vcom=-1250000\0" \
	"image=zImage\0" \
	"console=ttymxc0\0" \
	"initrd=0x89000000\0" \
	"fdt_file=zero-gravitas.dtb\0" \
	"fdt_addr=0x88000000\0" \
	"ip_dyn=yes\0" \
	"mmcdev=1\0" \
	"mmcpart=1\0" \
	"splashimage=0x80000000\0" \
	"splashpos=m,m\0" \
	"mmcroot=/dev/mmcblk1p2 rootwait rw\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} " \
			"root=${mmcroot} max17135:vcom=${vcom};\0" \
	"loadimage=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdt_file}\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"run mmcargs; " \
		"if run loadfdt; then " \
			"bootz ${loadaddr} - ${fdt_addr}; " \
		"else " \
			"echo WARN: Cannot load the DT; " \
		"fi;\0" \
	"memboot=echo Booting from memory...; " \
		"setenv bootargs console=${console},${baudrate}" \
			"max17135:vcom=${vcom} " \
		"g_mass_storage.stall=0 g_mass_storage.removable=1 " \
		"g_mass_storage.idVendor=0x066F g_mass_storage.idProduct=0x37FF "\
		"g_mass_storage.iSerialNumber=\"\" rdinit=/sbin/init; "\
		"bootz ${loadaddr} ${initrd} ${fdt_addr};\0" \

#define CONFIG_BOOTCOMMAND \
	"run memboot; " \
	"mmc dev ${mmcdev}; " \
	"if mmc rescan; then " \
		"if run loadimage; then " \
			"run mmcboot; " \
		"fi; " \
	"fi;" \
	"echo WARN: unable to boot from either RAM or eMMC; "

/* Miscellaneous configurable options */
#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_512M)

#define CONFIG_STACKSIZE		SZ_128K

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR
#define PHYS_SDRAM_SIZE			SZ_512M

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Environment organization */
#define CONFIG_ENV_SIZE			SZ_8K

#define CONFIG_ENV_IS_IN_FAT
#define FAT_ENV_INTERFACE "mmc"
#define FAT_ENV_DEVICE_AND_PART "1:1"
#define CONFIG_FAT_WRITE
#define FAT_ENV_FILE "uboot.env"

#ifdef CONFIG_CMD_SF
#define CONFIG_MXC_SPI
#define CONFIG_SF_DEFAULT_BUS		0
#define CONFIG_SF_DEFAULT_CS		0
#define CONFIG_SF_DEFAULT_SPEED		20000000
#define CONFIG_SF_DEFAULT_MODE		SPI_MODE_0
#endif

/* USB Configs */
#ifdef CONFIG_CMD_USB
#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_MX6
#define CONFIG_USB_STORAGE
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET
#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_MXC_USB_PORTSC		(PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS		0
#define CONFIG_USB_MAX_CONTROLLER_COUNT	2
#endif

#define CONFIG_SYS_FSL_USDHC_NUM	3
#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_SYS_MMC_ENV_DEV		1	/* SDHC2*/
#endif

#define CONFIG_IMX_THERMAL

#endif				/* __ZERO_GRAVITAS_H */
