#ifndef EPD_PMIC_INIT_H
#define EPD_PMIC_INIT_H

#include <command.h>

int epd_read_temp(int *temp);
extern int zs_do_config_epd_powerctrl_pins(void);
extern int zs_do_epd_power_on(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

#endif
