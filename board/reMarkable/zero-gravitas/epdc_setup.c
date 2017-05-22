/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2016 reMarkable AS. All Rights Reserved.
 *
 * Peng Fan <Peng.Fan@freescale.com>
 * Martin Sandsmark <martin.sandsmark@remarkable.no>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <lcd.h>
#include <linux/err.h>
#include <linux/types.h>
#include <malloc.h>

#ifdef CONFIG_WAVEFORM_BUF_SIZE
#include <mxc_epdc_fb.h>

#define is_digit(c)	((c) >= '0' && (c) <= '9')
__weak int mmc_get_env_devno(void)
{
	return 0;
}
__weak int check_mmc_autodetect(void)
{
	return 0;
}

struct waveform_data_header {
	unsigned int wi0;
	unsigned int wi1;
	unsigned int wi2;
	unsigned int wi3;
	unsigned int wi4;
	unsigned int wi5;
	unsigned int wi6;
	unsigned int xwia:24;
	unsigned int cs1:8;
	unsigned int wmta:24;
	unsigned int fvsn:8;
	unsigned int luts:8;
	unsigned int mc:8;
	unsigned int trc:8;
	unsigned int reserved0_0:8;
	unsigned int eb:8;
	unsigned int sb:8;
	unsigned int reserved0_1:8;
	unsigned int reserved0_2:8;
	unsigned int reserved0_3:8;
	unsigned int reserved0_4:8;
	unsigned int reserved0_5:8;
	unsigned int cs2:8;
};

struct mxcfb_waveform_data_file {
	struct waveform_data_header wdh;
	u32 *data;	/* Temperature Range Table + Waveform Data */
};


int board_setup_waveform_file(ulong waveform_buf)
{
	char *fs_argv[5];
	char addr[17];
	ulong file_len, mmc_dev;

	struct mxcfb_waveform_data_file *wf_file;
	int wf_offset, i;
	int temperature_entries;

	if (!check_mmc_autodetect())
		mmc_dev = getenv_ulong("mmcdev", 10, 0);
	else
		mmc_dev = mmc_get_env_devno();

	/* Allocate memory for storing the full waveform file */
	wf_file = memalign(ARCH_DMA_MINALIGN, CONFIG_WAVEFORM_BUF_SIZE);
	if (!wf_file) {
		printf("Failed to allocate temporary waveform file buffer\n");
		return -1;
	}
	sprintf(addr, "%p", wf_file);

	fs_argv[0] = "fatload";
	fs_argv[1] = "mmc";
	fs_argv[2] = simple_itoa(mmc_dev);
	fs_argv[3] = addr;
	fs_argv[4] = getenv("epdc_waveform");

	if (!fs_argv[4])
		fs_argv[4] = "waveform.bin";

	if (do_fat_fsload(NULL, 0, 5, fs_argv)) {
		printf("EPDC: File %s not found on MMC Device %lu!\n", fs_argv[4], mmc_dev);
		free(wf_file);
		return -1;
	}

	file_len = getenv_hex("filesize", 0);
	if (!file_len) {
		printf("EPDC: Failed to get file size from environment\n");
		free(wf_file);
		return -1;
	}

	/* Parse header to find offset for raw waveform data for EPDC */
	temperature_entries = wf_file->wdh.trc + 1;
	for (i = 0; i < temperature_entries; i++) {
		printf("temperature entry #%d = 0x%x\n", i, *((u8 *)&wf_file->data + i));
	}

	wf_offset = sizeof(wf_file->wdh) + temperature_entries + 1;
	memcpy((u8*)waveform_buf, (u8*)(wf_file) + wf_offset, file_len - wf_offset);
	free(wf_file);

	flush_cache(waveform_buf, file_len - wf_offset);

	return 0;
}
#endif
