/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Steinar Bakkemo <steinar.bakkemo@remarkable.com>
 */

#include "max77818.h"

#include <linux/errno.h>
#include <dm/uclass.h>
#include <i2c.h>

static struct udevice *bus = NULL, *idDev = NULL;

struct udevice *max77818_get_bus(void)
{
	return bus;
}

int max77818_i2c_reg_write16(struct udevice *dev, u8 addr, u16 mask, u16 data)
{
	int ret;
	u8 val[2];
	u16 read_val, final_val;
	u16 masked_data;
	struct dm_i2c_chip *chip;

	if (!dev) {
		printf("%s: Cannot write to not initialized dev !\n",
		       __func__);
		return -EINVAL;
	}

	chip = dev_get_parent_platdata(dev);

	ret = max77818_i2c_reg_read16(dev, addr, &read_val);
	if (ret)
		return ret;

	masked_data = data & mask;
	final_val = read_val & ~mask;
	final_val |= masked_data;
	val[0] = final_val & 0xff;
	val[1] = (final_val >> 8) & 0xff;

	ret = dm_i2c_write(dev, addr, val, 2);
	if (ret) {
		printf("%s: Failed to write to addr 0x%02x/dev 0x%02x: %d\n",
		       __func__,
		       addr,
		       chip->chip_addr,
		       ret);

		return ret;
	}

	return 0;
}

int max77818_i2c_reg_read16(struct udevice *dev, u8 addr, u16 *data)
{
	int ret;
	u8 val[2];
	struct dm_i2c_chip *chip;

	if (!dev) {
		printf("%s: Cannot write to not initialized dev !\n",
		       __func__);
		return -EINVAL;
	}

	chip = dev_get_parent_platdata(dev);

	ret = dm_i2c_read(dev, addr, val, 2);
	if (ret) {
		printf("%s: Failed to read from addr 0x%02x/dev 0x%02x: %d\n",
		       __func__,
		       addr,
		       chip->chip_addr,
		       ret);
		return ret;
	} else {
		*data = val[0] | ((u16)val[1] << 8);
	}

	return 0;
}

int max77818_i2c_reg_write8(struct udevice *dev, u8 addr, u8 mask, u8 data)
{
	u8 valb;
	int ret;
	struct dm_i2c_chip *chip;

	if (!dev) {
		printf("%s: Cannot write to not initialized dev !\n",
		       __func__);
		return -EINVAL;
	}

	chip = dev_get_parent_platdata(dev);

	if (mask != 0xff) {
		ret = dm_i2c_read(dev, addr, &valb, 1);
		if (ret)
			return ret;

		valb &= ~mask;
		valb |= data;
	} else
		valb = data;

	ret = dm_i2c_write(dev, addr, &valb, 1);
	if (ret) {
		printf("%s: Failed to write to addr 0x%02x/dev 0x%02x: %d\n",
		       __func__,
		       addr,
		       chip->chip_addr,
		       ret);

		return ret;
	}

	return 0;
}

int max77818_init_i2c_bus(void)
{
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, MAX77818_I2C_BUS, &bus);
	if (ret) {
		printf("%s: Can't find I2C bus %d (expected for MAX77818)\n",
		       __func__, MAX77818_I2C_BUS);
	}
	return ret;
}

int max77818_init_idDev(void)
{
	int ret;

	if (!bus) {
		ret = max77818_init_i2c_bus();
		if (ret) {
			printf("%s: Unable to complete ID device initialization",
			       __func__);
			return ret;
		}
	}

	ret = dm_i2c_probe(bus, MAX77818_ID_I2C_ADDR, 0, &idDev);
	if (ret) {
		printf("%s: Can't find device id=0x%x, on bus %d\n",
		__func__, MAX77818_ID_I2C_ADDR, MAX77818_I2C_BUS);
		return ret;
	}

	return 0;
}

int max77818_enable_safeout1(void)
{
	int ret;
	u8 val;

	if (!idDev) {
		ret = max77818_init_idDev();
		if (ret) {
			printf("%s: Unable to enable SAFEOUT\n",
			       __func__);
			return ret;
		}
	}

	ret = dm_i2c_read(idDev, MAX77818_REG_SAFEOUTCTRL, &val, 1);
	if (ret)
		return ret;

	val |= MAX77818_SAFEOUTCTRL_ENSAFEOUT1;

	return dm_i2c_write(idDev, MAX77818_REG_SAFEOUTCTRL, &val, 1);
}
