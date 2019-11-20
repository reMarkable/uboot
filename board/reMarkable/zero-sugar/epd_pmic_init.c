#include "epd_pmic_init.h"
#include "mmc_tools.h"

#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <dm/uclass.h>
#include <i2c.h>
#include <linux/errno.h>

#include <malloc.h>
#include <environment.h>
#include <mmc.h>

#define SY7636A_I2C_BUS 3
#define SY7636A_I2C_ADDR 0x62

#define SY7636A_REG_OPERATIONMODE 0x00
#define SY7636A_OPERATIONMODE_ONOFF 0x80
#define SY7636A_OPERATIONMODE_VCOMCTRL 0x40

#define SY7636A_REG_VCOMADJUST_L 0x01
#define SY7636A_REG_VCOMADJUST_H 0x02

#define SY7636A_VCOMADJUST_LMASK 0xff
#define SY7636A_VCOMADJUST_HMASK 0x80

#define SY7636A_REG_THERMISTOR  0x08

static int sy7636a_i2c_reg_write(struct udevice *dev, uint addr, uint mask, uint data)
{
	u8 valb;
	int ret;

	if (mask != 0xff) {
		ret = dm_i2c_read(dev, addr, &valb, 1);
		if (ret)
			return ret;

		valb &= ~mask;
		valb |= data;
	} else {
		valb = data;
	}

	ret = dm_i2c_write(dev, addr, &valb, 1);
	return ret;
}

static int sy7636a_i2c_reg_read(struct udevice *dev, u8 addr, u8 *data)
{
	u8 valb;
	int ret;

	ret = dm_i2c_read(dev, addr, &valb, 1);
	if (ret)
		return ret;

	*data = (int)valb;
	return 0;
}

static int sy7636a_vcom_get(struct udevice *dev, int *vcom)
{
	u8 low, high;
	int ret;

	ret = sy7636a_i2c_reg_read(dev, SY7636A_REG_VCOMADJUST_L, &low);
	if (ret)
		return ret;

	ret = sy7636a_i2c_reg_read(dev, SY7636A_REG_VCOMADJUST_H, &high);
	if (ret)
		return ret;

	low &= SY7636A_VCOMADJUST_LMASK;
	high &= SY7636A_VCOMADJUST_HMASK;

	*vcom = -10 * (low | ((u16)high << 1));
	*vcom = (*vcom < -5000) ? -5000 : *vcom;
	return 0;
}

static int sy7636a_vcom_set(struct udevice *dev, int vcom)
{
	u8 high, low;
	int ret;

	if (vcom < 0)
		vcom = -vcom;

	vcom /= 10;

	if (vcom > 0x01FF)
		return -EINVAL;

	low = vcom & SY7636A_VCOMADJUST_LMASK;
	high = ((u16)vcom >> 1) & SY7636A_VCOMADJUST_HMASK;

	ret = sy7636a_i2c_reg_write(dev, SY7636A_REG_VCOMADJUST_L, SY7636A_VCOMADJUST_LMASK, low);
	if (ret)
		return ret;

	return sy7636a_i2c_reg_write(dev, SY7636A_REG_VCOMADJUST_H, SY7636A_VCOMADJUST_HMASK, high);
}

static int sy7636a_thermistor_get(struct udevice *dev, int *temp)
{
	u8 value;
	int ret;

	ret = sy7636a_i2c_reg_read(dev, SY7636A_REG_THERMISTOR, &value);
	if (ret)
		return ret;

	*temp = *((s8*)&value);

	return 0;
}

int zs_do_config_epd_powerctrl_pins(void)
{
	printf("Configuring EPD PMIC I2C pullup..\n");
	gpio_request(IMX_GPIO_NR(4, 22), "EPD_PMIC_I2C_PULLUP");
	gpio_direction_output(IMX_GPIO_NR(4, 22), 1);

	printf("Configuring EPD PMIC LDO4VEN..\n");
	gpio_request(IMX_GPIO_NR(7, 10), "PMIC_LDO4VEN");
	gpio_direction_output(IMX_GPIO_NR(7, 10), 1);

	printf("Configuring EPD PMIC powerup signal..\n");
	gpio_request(IMX_GPIO_NR(7, 11), "EPD_PMIC_POWERUP");
	gpio_direction_output(IMX_GPIO_NR(7, 11), 1);

	return 0;
}

int zs_do_read_temp(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int temp, ret;
	ret = epd_read_temp(&temp);
	if (ret)
		return ret;

	printf("epd temperature: %d\n", temp);
	return 0;
}

int epd_read_temp(int *temp)
{
	int ret;
	struct udevice *bus, *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, SY7636A_I2C_BUS, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, SY7636A_I2C_BUS);
		return -1;
	}

	ret = dm_i2c_probe(bus, SY7636A_I2C_ADDR, 0, &dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
				__func__, SY7636A_I2C_ADDR, SY7636A_I2C_BUS);
		return -1;
	}

	/* Read thermistor value */
	return sy7636a_thermistor_get(dev, temp);
}

#define EPDIDVCOM_INDEX 3
#define EPDIDVCOM_LEN 25
#define VCOM_LEN 5
#define MAX_SERIAL_LEN 40

static int read_vcom_from_mmc(int dev, int part, ulong *vcom)
{
	struct mmc *mmc;
	u32 n, i, size;
	char *buffer;
	char *src;
	char serial[EPDIDVCOM_LEN + 1] = {0};
	int ret;

	const u32 blk_cnt = 1;

	mmc = mmc_set_dev_part(dev, part);
	if (!mmc)
		return -1;

	buffer = (char*)malloc(512);
	if (!buffer) {
		printf("%s: Unable to allocate memory\n", __func__);
		ret = -1;
		goto reset_mmc;
	}

	n = blk_dread(mmc_get_blk_desc(mmc), 0, blk_cnt, buffer);
	if (n != blk_cnt) {
		printf("%s: MMC read failed\n", __func__);
		ret = -1;
		goto free_buf;
	}

	src = buffer;
	size = 0;
	for (i = 0; i < EPDIDVCOM_INDEX + 1; i++) {
		src += size;
		size = ((u32)src[0] << 24) |
				((u32)src[1] << 16) |
				((u32)src[2] << 8) |
				((u32)src[3]);
		if (size > MAX_SERIAL_LEN) {
			printf("%s: Invalid size value read from MMC, giving up\n", __func__);
			ret = -1;
			goto free_buf;
		}
		src += sizeof(u32);
	}

	if (size != EPDIDVCOM_LEN) {
		printf("%s: Invalid EPD serial + vcom size\n", __func__);
		ret = -1;
		goto free_buf;
	}

	strncpy(serial, src, EPDIDVCOM_LEN - VCOM_LEN - 1);
	printf("EPD serial: \"%s\"\n", serial);

	src += (EPDIDVCOM_LEN - VCOM_LEN);

	if ((src[0] != '-') ||
		(src[1] < '0' || src[1] > '5') ||
		(src[2] != '.') ||
		(src[3] < '0' || src[3] > '9') ||
		(src[4] < '0' || src[4] > '9') ) {
		printf("%s: Invalid vcom value\n", __func__);
		ret = -1;
		goto free_buf;
	}

	*vcom = (src[1] - '0') * 1000 +
			(src[3] - '0') * 100 +
			(src[4] - '0') * 10;

	printf("EPD vcom: -%lumV\n", *vcom);

	ret = 0;

free_buf:
	free(buffer);
reset_mmc:
	mmc_reset();

	return ret;
}

static int zs_do_read_vcom_from_mmc(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong vcom;
	return read_vcom_from_mmc(0, 2, &vcom);
}

U_BOOT_CMD(
		epd_vcom_mmc,	1,	1,	zs_do_read_vcom_from_mmc,
		"Read vcom from mmc",
		""
		);

int zs_do_epd_power_on(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *bus, *dev;
	u8 mask, val;
	ulong vcom;
	int ivcom;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, SY7636A_I2C_BUS, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, SY7636A_I2C_BUS);
		return -1;
	}

	ret = dm_i2c_probe(bus, SY7636A_I2C_ADDR, 0, &dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
				__func__, SY7636A_I2C_ADDR, SY7636A_I2C_BUS);
		return -1;
	}

	/* Read vcom value from chip */
	ret = sy7636a_vcom_get(dev, &ivcom);
	if (ret)
		return ret;

	vcom = env_get_ulong("vcom", 10, 9999);

	if (vcom == 9999) {
		printf("vcom not found in environment, reading from mmc\n");
		ret = read_vcom_from_mmc(0, 2, &vcom);
		if (ret == 0) {
			env_set_ulong("vcom", vcom);
			env_save();
		} else {
			printf("Unable to read vcom from mmc, using default\n");
			vcom = 1250;
		}
	}

	printf("Chip reported vcom %dmV"
			", setting to -%lumV\n", ivcom, vcom);

	/* Set target vcom value */
	ret = sy7636a_vcom_set(dev, vcom);
	if (ret)
		return ret;

	/* Power on, include VCOM in power sequence */
	mask = (SY7636A_OPERATIONMODE_ONOFF | SY7636A_OPERATIONMODE_VCOMCTRL);
	val = SY7636A_OPERATIONMODE_ONOFF;

	return sy7636a_i2c_reg_write(dev, SY7636A_REG_OPERATIONMODE, mask, val);
}

U_BOOT_CMD(
		epd_power_on,	1,	1,	zs_do_epd_power_on,
		"Turn on power for eInk Display",
		""
		);

U_BOOT_CMD(
		epd_temp,	1,	1,	zs_do_read_temp,
		"Read epd temperature",
		""
		);
