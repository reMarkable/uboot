#ifndef EPD_INIT_H
#define EPD_INIT_H

#include <command.h>

extern int zs_do_config_epd_powerctrl_pins(void);
extern int zs_do_epd_power_on(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

#endif
