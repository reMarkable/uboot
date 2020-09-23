/*
 * (C) Copyright 2020
 * reMarkable AS - http://www.remarkable.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Author: Lars Ivar Miljeteig <lars.ivar.miljeteig@remarkable.com>
 */

#include "mmc_tools.h"

#include <asm/arch/sys_proto.h>
#include <mmc.h>

struct mmc *mmc_set_dev_part(int dev, int part)
{
	int ret;
	struct mmc *mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("%s: no mmc device at slot %x\n", __func__, dev);
		return NULL;
	}
	mmc->has_init = 0;

	if (mmc_init(mmc)) {
		printf("%s: Unable to initialize mmc\n", __func__);
		return NULL;
	}

	ret = blk_select_hwpart_devnum(IF_TYPE_MMC, dev, part);
	if (ret) {
		printf("%s: Unable to switch partition, returned %d\n", __func__, ret);
		return NULL;
	}

	return mmc;
}

int mmc_reset(void)
{
	struct mmc *mmc = mmc_set_dev_part(mmc_get_env_dev(), 0);
	return mmc ? 0 : -1;
}
