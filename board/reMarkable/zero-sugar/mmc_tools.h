/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Lars Ivar Miljeteig <lars.ivar.miljeteig@remarkable.com>
 */

#ifndef MMC_TOOLS_H
#define MMC_TOOLS_H

struct mmc;

struct mmc *mmc_set_dev_part(int dev, int part);
int mmc_reset(void);

#endif
