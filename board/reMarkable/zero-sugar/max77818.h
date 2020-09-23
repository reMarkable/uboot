/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Steinar Bakkemo <steinar.bakkemo@remarkable.com>
 */

#ifndef MAX77818_H
#define MAX77818_H

#include <asm/arch/sys_proto.h>
#include <dm/device.h>

#define MAX77818_I2C_BUS						1

#define MAX77818_CHARGER_I2C_ADDR					0x69
#define MAX77818_FG_I2C_ADDR						0x36
#define MAX77818_ID_I2C_ADDR						0x66

#define MAX77818_REG_SAFEOUTCTRL					0xC6
#define MAX77818_SAFEOUTCTRL_ENSAFEOUT1					BIT(6)

struct udevice *max77818_get_bus(void);

int max77818_i2c_reg_read16(struct udevice *dev, u8 addr, u16 *data);
int max77818_i2c_reg_write16(struct udevice *dev, u8 addr, u16 mask, u16 data);
int max77818_i2c_reg_write8(struct udevice *dev, u8 addr, u8 mask, u8 data);

int max77818_init_i2c_bus(void);
int max77818_init_idDev(void);

int max77818_enable_safeout1(void);

#endif /* MAX77818_H */
