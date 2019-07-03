/* Copyright 2018 NXP
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __IMX_COMMON_CONFIG_H
#define __IMX_COMMON_CONFIG_H

#ifdef CONFIG_ARM64
    #define MFG_BOOT_CMD "booti "
#else
    #define MFG_BOOT_CMD "bootz "
#endif

#define CONFIG_MFG_ENV_SETTINGS_DEFAULT \
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
	 "gpio input 1;" \
     "gpio set 11:" \
	 "gpio set 118;" \
	 "gpio set 202;" \
	 "gpio set 203;" \
     "gpio set 6;" \
	 "bootz 0x82000000 - 0x88000000;" \
	"\0" \

#endif
