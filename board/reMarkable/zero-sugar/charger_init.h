#ifndef CHARGER_INIT_H
#define CHARGER_INIT_H

#include <command.h>
#include <dm/device.h>

int max77818_init_device(void);

/* STATUS QUERY */
int max77818_get_battery_status(struct udevice *dev);
int max77818_get_wcin_details(struct udevice *dev);
int max77818_get_chgin_details(struct udevice *dev);

/* CHARGER CONFIG */
int max77818_set_otg_pwr(struct udevice *dev, bool otg_on);
int max77818_set_fast_charge_current_2800_ma(struct udevice *dev);
int max77818_set_pogo_input_current_limit(struct udevice *dev);
int max77818_set_usbc_input_current_limit(struct udevice *dev);
int max77818_set_charge_termination_voltage(struct udevice *dev);

int max77818_enable_safeout1(void);

/* CLI COMMAND ROUTINES*/
int zs_do_set_otg_pwr(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
int zs_do_set_fastcharge_current_2800_ma(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
int zs_do_set_charge_termination_voltage(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
int zs_do_set_pogo_input_current_limit(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
int zs_do_set_usbc_input_current_limit(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

#endif /* CHARGRE_INIT_H */
