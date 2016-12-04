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

int board_setup_logo_file(void *display_buf)
{
	int logo_width, logo_height;
	char *fs_argv[5];
	char addr[17];
	int array[3];
	ulong file_len, mmc_dev;
	char *buf, *s;
	int arg = 0, val = 0, pos = 0;
	int i, j, max_check_length;
	int row, col, row_end, col_end;

	if (!display_buf)
		return -EINVAL;

	/* Assume PGM header not exceeds 128 bytes */
	max_check_length = 128;

	if (!check_mmc_autodetect())
		mmc_dev = getenv_ulong("mmcdev", 10, 0);
	else
		mmc_dev = mmc_get_env_devno();

	fs_argv[0] = "fatsize";
	fs_argv[1] = "mmc";
	fs_argv[2] = simple_itoa(mmc_dev);
	fs_argv[3] = getenv("logo");
	if (!fs_argv[3])
		fs_argv[3] = "logo.pgm";
	if (do_fat_size(NULL, 0, 4, fs_argv)) {
		debug("File %s not found on MMC Device %lu, use black border\n", fs_argv[3], mmc_dev);
		/* Draw black border around framebuffer*/
		memset(display_buf, 0xFF, 24 * panel_info.vl_col);
		for (i = 24; i < (panel_info.vl_row - 24); i++) {
			memset((u8 *)display_buf + i * panel_info.vl_col,
			       0x00, 24);
			memset((u8 *)display_buf + i * panel_info.vl_col
				+ panel_info.vl_col - 24, 0x00, 24);
		}
		memset((u8 *)display_buf +
		       panel_info.vl_col * (panel_info.vl_row - 24),
		       0xFF, 24 * panel_info.vl_col);
		return 0;
	}

	file_len = getenv_hex("filesize", 0);
	if (!file_len)
		return -EINVAL;

	buf = memalign(ARCH_DMA_MINALIGN, file_len);
	if (!buf)
		return -ENOMEM;

	sprintf(addr, "%lx", (ulong)buf);

	fs_argv[0] = "fatload";
	fs_argv[1] = "mmc";
	fs_argv[2] = simple_itoa(mmc_dev);
	fs_argv[3] = addr;
	fs_argv[4] = getenv("logo");

	if (!fs_argv[4])
		fs_argv[4] = "logo.pgm";

	if (do_fat_fsload(NULL, 0, 5, fs_argv)) {
		printf("File %s not found on MMC Device %lu!\n", fs_argv[4], mmc_dev);
		free(buf);
		return -1;
	}

	if (strncmp(buf, "P5", 2)) {
		printf("Wrong format for epdc logo, use PGM-P5 format.\n");
		free(buf);
		return -EINVAL;
	}
	/* Skip P5\n */
	pos += 3;
	arg = 0;
	for (i = 3; i < max_check_length; ) {
		/* skip \n \t and space */
		if ((buf[i] == '\n') || (buf[i] == '\t') || (buf[i] == ' ')) {
			i++;
			continue;
		}
		/* skip comment */
		if (buf[i] == '#') {
			while (buf[i++] != '\n')
				;
			continue;
		}

		/* HEIGTH, WIDTH, MAX PIXEL VLAUE total 3 args */
		if (arg > 2)
			break;
		val = 0;
		while (is_digit(buf[i])) {
			val = val * 10 + buf[i] - '0';
			i++;
		}
		array[arg++] = val;

		i++;
	}

	/* Point to data area */
	pos = i;

	logo_width = array[0];
	logo_height = array[1];

	if ((logo_width > panel_info.vl_col) ||
	    (logo_height > panel_info.vl_row)) {
		printf("Splash screen too big for display\n");
		free(buf);
		return -EINVAL;
	}

	/* m,m means center of screen */
	row = -1;
	col = -1;
	s = getenv("splashpos");
	if (s) {
		if (s[0] == 'm')
			col = (panel_info.vl_col  - logo_width) >> 1;
		else
			col = simple_strtol(s, NULL, 0);
		s = strchr(s + 1, ',');
		if (s != NULL) {
			if (s[1] == 'm')
				row = (panel_info.vl_row  - logo_height) >> 1;
			else
				row = simple_strtol(s + 1, NULL, 0);
		}
	}

	if (row < 0) {
		row = (panel_info.vl_row  - logo_height) >> 1;
	}

	if (col < 0) {
		col = (panel_info.vl_col  - logo_width) >> 1;
	}

	if ((col + logo_width > panel_info.vl_col) ||
	    (row + logo_height > panel_info.vl_row)) {
		printf("Incorrect pos, use (0, 0)\n");
		row = 0;
		col = 0;
	}

	/* Draw picture at the center of screen */
	row_end = row + logo_height;
	col_end = col + logo_width;
	for (i = row; i < row_end; i++) {
		for (j = col; j < col_end; j++) {
			*((u8 *)display_buf + i * (panel_info.vl_col) + j) =
				 buf[pos++];
		}
	}

	free(buf);

	flush_cache((ulong)display_buf, file_len - pos - 1);

	return 0;
}
