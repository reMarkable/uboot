/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Thomas Ingebretsen <thomas.ingebretsen@remarkable.com>
 */

#include "epd_display_init.h"
#include "epd_pmic_init.h"
#include "mmc_tools.h"

#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/mx7-pins.h>
#include <asm/mach-imx/video.h>
#include <mxsfb.h>
#include <video_fb.h>
#include <stdlib.h>
#include <fat.h>
#include <memalign.h>

#define TEMP_CRITICAL_HIGH 50

static int epd_splash(void);
static int splash_init(void);
static uint8_t *epd_load_image(const char *filename, u32 *x0, u32 *y0, u32 *width, u32 *height);

static GraphicDevice *pGD = NULL;	/* Pointer to Graphic array */

#define LCD_PAD_CTRL    (PAD_CTL_HYS | PAD_CTL_PUS_PU100KOHM | PAD_CTL_DSE_3P3V_49OHM)

//#define EPD_DISPLAY_INIT_DEBUG

/* Default temperature in case reading EPD temperature fails: */
#define EPD_DEFAULT_TEMPERATURE 24

static iomux_v3_cfg_t const lcd_pads[] = {
	MX7D_PAD_LCD_CLK__LCD_CLK | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_ENABLE__LCD_ENABLE | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_HSYNC__LCD_HSYNC | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_VSYNC__LCD_VSYNC | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA00__LCD_DATA0 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA01__LCD_DATA1 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA02__LCD_DATA2 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA03__LCD_DATA3 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA04__LCD_DATA4 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA05__LCD_DATA5 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA06__LCD_DATA6 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA07__LCD_DATA7 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA08__LCD_DATA8 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA09__LCD_DATA9 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA10__LCD_DATA10 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA11__LCD_DATA11 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA12__LCD_DATA12 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA13__LCD_DATA13 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA14__LCD_DATA14 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA15__LCD_DATA15 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA16__LCD_DATA16 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA17__LCD_DATA17 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA18__LCD_DATA18 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA19__LCD_DATA19 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA20__LCD_DATA20 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA21__LCD_DATA21 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA22__LCD_DATA22 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_DATA23__LCD_DATA23 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX7D_PAD_LCD_RESET__GPIO3_IO4	| MUX_PAD_CTRL(LCD_PAD_CTRL),
};

void do_enable_parallel_lcd(struct display_info_t const *dev)
{
	imx_iomux_v3_setup_multiple_pads(lcd_pads, ARRAY_SIZE(lcd_pads));
}

struct display_info_t const displays[] = {{
	.bus = ELCDIF1_IPS_BASE_ADDR,
		.addr = 0,
		.pixfmt = 24,
		.detect = NULL,
		.enable	= do_enable_parallel_lcd,
		.mode	= {
			.name			= "EPD",
			.xres           = 334,
			.yres           = 1405,
			.pixclock       = KHZ2PICOS(40000),
			.left_margin    = 1,
			.right_margin   = 1,
			.upper_margin   = 1,
			.lower_margin   = 1,
			.hsync_len      = 1,
			.vsync_len      = 1,
			.sync           = 0,
			.vmode          = FB_VMODE_NONINTERLACED

		} } };
size_t display_count = ARRAY_SIZE(displays);

struct splash_functions {
	int (*set_temp)(int);
	int (*clear)(void*);
	int (*fill)(void*, uint16_t);
	int (*init_tbl)(const uint8_t**);
	int (*blit_gc)(uint32_t*, const uint8_t*, int, int, int, int, int);
}splash;

static int splash_init(void)
{
	const char *filename = "zplash";
	const int mmc_dev = 0;
	const int mmc_part = 0;
	const u32 max_inflated_size = 100*1024;
	void *addr = (void*)0x80300000;

	if (!mmc_set_dev_part(mmc_dev, mmc_part))
		return -1;

	// Check if splash file exists
	if (!fat_exists(filename)) {
		printf("%s: %s not found\n", __func__, filename);
		return -1;
	}

	// Get file size
	loff_t size;
	if (fat_size(filename, &size)) {
		printf("%s: unable to get file size of %s\n", __func__, filename);
		return -1;
	}

	// Allocate memory
	u8 *zdata = (u8*)malloc_cache_aligned(size);
	if (!zdata) {
		printf("%s: unable to allocate memory for splash library!\n", __func__);
		return -1;
	}

	// Load binary into memory
	if (file_fat_read(filename, zdata, size) != size) {
		printf("%s: unable to load %s from disk\n", __func__, filename);
		free(zdata);
		return -1;
	}

	// Read expected crc and size
	u32 expected_crc, expected_size;
	memcpy(&expected_crc, zdata + size - 8, sizeof(expected_crc));
	expected_crc = le32_to_cpu(expected_crc);
	memcpy(&expected_size, zdata + size - 4, sizeof(expected_size));
	expected_size = le32_to_cpu(expected_size);

	if (expected_size > max_inflated_size) {
		printf("%s: expected inflated size is too large! (>%u)\n", __func__,
				max_inflated_size);
		free(zdata);
		return -1;
	}

	// Unzip into addr
	unsigned long lenp = size;
	if (gunzip(addr, max_inflated_size, zdata, &lenp)) {
		printf("%s: unable to decompress %s\n", __func__, filename);
		free(zdata);
		return -1;
	}

	// Free memory
	free(zdata);

	// Check actual size vs expected size
	if (lenp != expected_size) {
		printf("%s: unexpected uncompressed size!\n", __func__);
		return -1;
	}

	// Check actual checksum vs expected checksum
	u32 crc = crc32(0, addr, lenp);
	if (crc != expected_crc) {
		printf("%s: wrong checksum!\n", __func__);
		return -1;
	}

	// Run init
	memset(&splash, 0, sizeof(splash));
	ulong (*init_entry)(void*) = addr;
	if (init_entry(&splash)) {
		printf("%s: init_entry failed\n", __func__);
		return -1;
	}

	if (splash.set_temp == NULL ||
			splash.clear == NULL ||
			splash.fill == NULL ||
			splash.init_tbl == NULL ||
			splash.blit_gc == NULL) {
		printf("%s: splash not properly initialized\n", __func__);
		return -1;
	}

	return 0;
}

int epd_display_init()
{
	int i;

	// Check that framebuffer is initialized
	if (pGD == NULL)
		return -1;

	const int frame_size = pGD->plnSizeX * pGD->plnSizeY * pGD->gdfBytesPP;

	// Load graphics library
	if (splash_init()) {
		printf("%s: splash_init failed!\n", __func__);
		return -1;
	}

	// Read and set temperature
	int temp;
	if (epd_read_temp(&temp)) {
		temp = EPD_DEFAULT_TEMPERATURE;
		printf("%s: failed to read temperature, defaulting to %d\n", __func__, temp);
	} else {
		printf("%s: temperature = %d\n", __func__, temp);

		if (temp >= TEMP_CRITICAL_HIGH) {
			printf("EPD temperature critically high, turning off\n");
			do_poweroff(NULL, 0, 0, NULL);
		}
	}
	splash.set_temp(temp);

	mxs_pan(3);

	// Pointers to first 3 frame buffers
	u32 *frame0 = (u32*)(pGD->frameAdrs + 0 * frame_size);
	u32 *frame1 = (u32*)(pGD->frameAdrs + 1 * frame_size);
	u32 *frame2 = (u32*)(pGD->frameAdrs + 2 * frame_size);

	// Clear frames
	splash.clear(frame0);
	memcpy(frame1, frame0, frame_size);
	memcpy(frame2, frame0, frame_size);

	// Fill frame1 and frame2
	splash.fill(frame1, 0x5555);
	splash.fill(frame2, 0xAAAA);

	// Flush cache
	flush_cache(pGD->frameAdrs, roundup(pGD->memSize, ARCH_DMA_MINALIGN));

	// Get init sequence table
	const u8 *wf_init;
	int phases = splash.init_tbl(&wf_init);
	if (phases == 0) {
		printf("%s: unable to get init table\n", __func__);
		return -1;
	}

	// Run INIT sequence
	printf("%s: clearing epd\n", __func__);
	for (i = 0; i < phases; i++) {
		mxs_pan(wf_init[i]);
	}

	// Show splash screen
	int ret = epd_splash();
	if (ret) {
		printf("%s: splash failed", __func__);
		return ret;
	}

	return 0;
}

static uint8_t *epd_load_image(const char *filename, u32 *x0, u32 *y0, u32 *width, u32 *height)
{
	// Check if splash.dat exists
	if (!fat_exists(filename)) {
		printf("%s: %s not found\n", __func__, filename);
		return NULL;
	}

	// Get file size
	loff_t size;
	if (fat_size(filename, &size)) {
		printf("%s: unable to get file size of %s\n", __func__, filename);
		return NULL;
	}

	// Allocate memory
	u8 *splash_data = (u8*)malloc_cache_aligned(size);
	if (!splash_data) {
		printf("%s: unable to allocate memory for splash data!\n", __func__);
		return NULL;
	}

	// Load spash data into memory
	if (file_fat_read(filename, splash_data, size) != size) {
		printf("%s: unable to load %s from disk\n", __func__, filename);
		free(splash_data);
		return NULL;
	}

	// Get offset and dimensions
	*x0 = *((u32*)&splash_data[0]);
	*y0 = *((u32*)&splash_data[4]);
	*width = *((u32*)&splash_data[8]);
	*height = *((u32*)&splash_data[12]);

	// Check if x0, y0, width and height are valid.
	if ((size != 4*sizeof(u32) + *width * (*height)) ||
			(*x0 + *width > 1872 || *y0 + *height > 1404) ) {
		free(splash_data);
		printf("%s: corrupt splash data\n", __func__);
		return NULL;
	}

	return splash_data;
}

int epd_splash(void)
{
	if (pGD == NULL) {
		printf("%s: video not initialized, skipping splash", __func__);
		return -1;
	}

	const int frame_size = pGD->plnSizeX * pGD->plnSizeY * pGD->gdfBytesPP;

	// Pointers to first 2 frame buffers
	u32 *frame0 = (u32*)(pGD->frameAdrs + 0 * frame_size);
	u32 *frame1 = (u32*)(pGD->frameAdrs + 1 * frame_size);

	// Clear frame1
	memcpy(frame1, frame0, frame_size);

	// Load splash image
	u32 x0, y0, width, height;
	const u8 *splash_data = epd_load_image("splash.dat", &x0, &y0, &width, &height);
	if (splash_data == NULL) {
		printf("%s: unable to load splash image, skipping splash\n", __func__);
		return -1;
	}

	const u8 *bitmap = (const u8*)&splash_data[16];

	int phase = 0;

	// Insert waveform for phase into frame1/frame2
	int ret = splash.blit_gc(frame0, bitmap, x0, y0, width, height, phase++);
	while (ret) {
		flush_cache((unsigned long)frame0, roundup(frame_size, ARCH_DMA_MINALIGN));
		mxs_pan(0);
		ret = splash.blit_gc(frame1, bitmap, x0, y0, width, height, phase++);
		if (!ret) break;
		flush_cache(rounddown((unsigned long)frame1,ARCH_DMA_MINALIGN), roundup(frame_size, ARCH_DMA_MINALIGN));
		mxs_pan(1);
		ret = splash.blit_gc(frame0, bitmap, x0, y0, width, height, phase++);
	}
	mxs_pan(3);

	free((void*)splash_data);
	return 0;
}

#ifdef EPD_DISPLAY_INIT_DEBUG
int epd_do_init(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return epd_init();
}

U_BOOT_CMD(
		epd_init,	1,	1,	epd_do_init,
		"Run EPD INIT sequence",
		""
	  );
#endif

int drv_video_init(void)
{
	/* Check if video initialization should be skipped */
	if (board_video_skip())
		return 0;

	pGD = video_hw_init();
	if (pGD == NULL)
		return -1;

	return 0;
}
