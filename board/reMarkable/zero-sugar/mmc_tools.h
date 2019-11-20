#ifndef MMC_TOOLS_H
#define MMC_TOOLS_H

struct mmc;

struct mmc *mmc_set_dev_part(int dev, int part);
int mmc_reset(void);

#endif
