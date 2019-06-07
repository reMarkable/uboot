#include "epd_init.h"

#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <dm/uclass.h>

#include <linux/errno.h>

#define SY7636A_I2C_BUS 3
#define SY7636A_I2C_ADDR 0x62

#define SY7636A_REG_OPERATIONMODE 0x00
#define SY7636A_OPERATIONMODE_ONOFF 0x80
#define SY7636A_OPERATIONMODE_VCOMCTRL 0x40

#define SY7636A_REG_VCOMADJUST_L 0x01
#define SY7636A_REG_VCOMADJUST_H 0x02

#define SY7636A_VCOMADJUST_LMASK 0xff
#define SY7636A_VCOMADJUST_HMASK 0x80

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

int zs_do_config_epd_powerctrl_pins(void)
{
    gpio_request(IMX_GPIO_NR(4, 22), "EPD_PMIC_I2C_PULLUP");
    gpio_direction_output(IMX_GPIO_NR(4, 22), 1);

    gpio_request(IMX_GPIO_NR(7, 10), "PMIC_LDO4VEN");
    gpio_direction_output(IMX_GPIO_NR(7, 10), 1);

    gpio_request(IMX_GPIO_NR(7, 11), "EPD_PMIC_POWERUP");
    gpio_direction_output(IMX_GPIO_NR(7, 11), 1);
}

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

	ret = sy7636a_vcom_get(dev, &ivcom);
	if (ret)
		return ret;

	vcom = env_get_ulong("vcom", 10, 1250);
	printf("vcom was %dmV, setting to -%lumV\n", ivcom, vcom);

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
