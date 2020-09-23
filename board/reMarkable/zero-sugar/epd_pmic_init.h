/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Lars Ivar Miljeteig <lars.ivar.miljeteig@remarkable.com>
 */

#ifndef EPD_PMIC_INIT_H
#define EPD_PMIC_INIT_H

#include <command.h>

int epd_read_temp(int *temp);
extern int zs_do_config_epd_powerctrl_pins(void);
int epd_set_power(bool enabled);

#endif
