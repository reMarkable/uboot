/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright 2019 reMarkable AS
 *
 * Configuration settings for the reMarkable fusion board.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ZEROSUGAR_CONFIG_H
#define __ZEROSUGAR_CONFIG_H

#include "mx7_common.h"

#define CONFIG_SYS_DCACHE_OFF

#define CONFIG_DBG_MONITOR
#define PHYS_SDRAM_SIZE			SZ_1G

#define CONFIG_MXC_UART_BASE            UART1_IPS_BASE_ADDR

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(32 * SZ_1M)

/* Network */
#ifdef CONFIG_DM_ETH
#define CONFIG_FEC_MXC
#define CONFIG_MII
#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_ENET_DEV		0

#define CONFIG_PHY_BROADCOM
/* ENET1 */
#if (CONFIG_FEC_ENET_DEV == 0)
#define IMX_FEC_BASE			ENET_IPS_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR          0x0
#define CONFIG_ETHPRIME                 "eth0"
#elif (CONFIG_FEC_ENET_DEV == 1)
#define IMX_FEC_BASE			ENET2_IPS_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR          0x1
#define CONFIG_ETHPRIME                 "eth1"
#endif

#define CONFIG_FEC_MXC_MDIO_BASE	ENET_IPS_BASE_ADDR
#endif

/* MMC Config*/
#define CONFIG_SYS_FSL_ESDHC_ADDR       0

#undef CONFIG_BOOTM_NETBSD
#undef CONFIG_BOOTM_PLAN9
#undef CONFIG_BOOTM_RTEMS

/* I2C configs */
#define CONFIG_SYS_I2C_MXC
#define CONFIG_SYS_I2C_SPEED		100000

#define CONFIG_SUPPORT_EMMC_BOOT	/* eMMC specific */
#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#ifdef CONFIG_IMX_BOOTAUX

#ifdef CONFIG_FSL_QSPI
#define UPDATE_M4_ENV \
	"m4image=m4_qspi.bin\0" \
	"loadm4image=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${m4image}\0" \
	"update_m4_from_sd=" \
		"if sf probe 1:0; then " \
			"if run loadm4image; then " \
				"setexpr fw_sz ${filesize} + 0xffff; " \
				"setexpr fw_sz ${fw_sz} / 0x10000; "	\
				"setexpr fw_sz ${fw_sz} * 0x10000; "	\
				"sf erase 0x100000 ${fw_sz}; " \
				"sf write ${loadaddr} 0x100000 ${filesize}; " \
			"fi; " \
		"fi\0" \
	"m4boot=sf probe 1:0; bootaux "__stringify(CONFIG_SYS_AUXCORE_BOOTDATA)"\0"
#else
#define UPDATE_M4_ENV \
	"m4image=m4_qspi.bin\0" \
	"loadm4image=fatload mmc ${mmcdev}:${mmcpart} "__stringify(CONFIG_SYS_AUXCORE_BOOTDATA)" ${m4image}\0" \
	"m4boot=run loadm4image; bootaux "__stringify(CONFIG_SYS_AUXCORE_BOOTDATA)"\0"
#endif
#else
#define UPDATE_M4_ENV ""
#endif

#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG
#define CONFIG_FASTBOOT_USB_DEV 0

#define CONFIG_MFG_ENV_SETTINGS \
	"mfgtool_args=setenv bootargs console=${console},${baudrate} rootwait rw root=/dev/mmcblk2p2\0" \
	"bootcmd_mfg=" \
	"run mfgtool_args;" \
	"setenv loadaddr 0x82000000;" \
	"setenv fdt_file /boot/zero-sugar.dtb;" \
	"setenv fdt_addr 0x88000000;" \
	"setenv mmcdev 0;" \
	"setenv mmcpart 2;" \
	"run loadimage;" \
	"run loadfdt;" \
	"bootz 0x82000000 - 0x88000000;" \
	"initrd_addr=0x83800000\0" \
	"initrd_high=0xffffffff\0" \
	"emmc_dev=1\0"\
	"sd_dev=0\0" \
	"\0"\

#define CONFIG_DFU_ENV_SETTINGS \
	"dfu_alt_info=image raw 0 0x800000;"\
		"u-boot raw 0 0x4000;"\
		"bootimg part 0 1;"\
		"rootfs part 0 2\0" \

#define CONFIG_EXTRA_ENV_SETTINGS \
	"image=/boot/zImage\0" \
	"console=ttymxc0\0" \
	"fdt_file=/boot/zero-sugar.dtb\0" \
	"fdt_addr=0x83000000\0" \
	"ip_dyn=yes\0" \
	"panel=EPD\0" \
	"mmcdev=0\0" \
	"active_partition=2\0" \
	"fallback_partition=3\0 " \
	"bootlimit=1\0 " \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} " \
		"root=/dev/mmcblk2p${active_partition} rootwait rootfstype=ext4 rw\0" \
	"loadimage=ext4load mmc ${mmcdev}:${active_partition} ${loadaddr} ${image}\0" \
	"loadfdt=ext4load mmc ${mmcdev}:${active_partition} ${fdt_addr} ${fdt_file}\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"mmc dev ${mmcdev}; " \
		"if mmc rescan; then " \
			"if run loadimage; then " \
				"if run loadfdt; then " \
					"bootz ${loadaddr} - ${fdt_addr}; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"fi; " \
		"fi;\0" \
	"memboot=echo Booting from memory...; " \
		"setenv bootargs console=${console},${baudrate} " \
		"g_mass_storage.stall=0 g_mass_storage.removable=1 " \
		"g_mass_storage.idVendor=0x066F g_mass_storage.idProduct=0x37FF "\
		"g_mass_storage.iSerialNumber=\"\" rdinit=/linuxrc; "\
		"bootz ${loadaddr} ${initrd} ${fdt_addr};\0" \
	"altbootcmd=echo Running from fallback root...; " \
		"run memboot; " \
		"if test ${bootcount} -gt 10; then " \
			"echo WARN: Failed too much, resetting bootcount and turning off; " \
			"setenv bootcount 0; " \
			"saveenv; " \
			"poweroff; " \
		"fi; " \
		"setenv mmcpart ${fallback_partition}; " \
		"setenv bootargs console=${console},${baudrate} " \
				"root=/dev/mmcblk2p${fallback_partition} rootwait rootfstype=ext4 quiet rw " \
				"systemd.log_level=debug systemd.log_target=kmsg memtest " \
				"log_buf_len=1M printk.devkmsg systemd.journald.forward_to_console=1; " \
		"run mmcboot;\0" \

/* Always try to boot from memory first, in case of USB download mode */
#define CONFIG_BOOTCOMMAND \
	"if test ! -e mmc 0:1 uboot.env; then " \
		"saveenv; " \
	"fi; " \
	"run memboot; " \
	"run mmcargs; " \
	"setenv mmcpart ${active_partition}; " \
	"run mmcboot; " \
	"echo WARN: unable to boot from either RAM or eMMC; " \
	"setenv upgrade_available 1; " \
	"saveenv; " \
	"reset; "

#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + 0x20000000)

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_HZ			1000

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Environment organization */
#define CONFIG_ENV_SIZE		SZ_8K

#ifdef CONFIG_ENV_IS_IN_FAT
#define CONFIG_BOOTCOUNT_LIMIT
#define CONFIG_BOOTCOUNT_ENV
#endif

#define CONFIG_SYS_FSL_USDHC_NUM	2

/* MMC Config*/
#define CONFIG_SYS_FSL_ESDHC_ADDR       0
#define CONFIG_SYS_MMC_ENV_DEV		0   /* USDHC1 */
#define CONFIG_SYS_MMC_ENV_PART		0	/* user area */
#define CONFIG_MMCROOT			"/dev/mmcblk2p2"  /* USDHC1 */

/* USB Configs */
#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)

#define CONFIG_IMX_THERMAL

#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_MXS
#define CONFIG_IMX_VIDEO_SKIP
#endif

#define CONFIG_USBD_HS

#endif	/* __ZERO_SUGAR_H */
