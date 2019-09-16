#ifndef CHARGER_INIT_H
#define CHARGER_INIT_H

#include <command.h>
#include <dm/device.h>

enum fast_charge_current {
	FASTCHARGE_1P5_A,
	FASTCHARGE_2P8_A
};

enum pogo_ilim {
	ILIM_1P5_A,
	ILIM_2P8_A
};

int max77818_init_device(void);

/* STATUS QUERY */
int max77818_get_battery_status(struct udevice *dev);
int max77818_get_wcin_details(struct udevice *dev);
int max77818_get_chgin_details(struct udevice *dev);

/* CHARGER CONFIG */
int max77818_set_otg_pwr(struct udevice *dev, bool otg_on);
int max77818_set_fast_charge_current(struct udevice *dev, enum fast_charge_current fsc);
int max77818_set_charge_termination_voltage(struct udevice *dev);
int max77818_set_pogo_input_current_limit(struct udevice *dev, enum pogo_ilim ilim);
int max77818_set_usbc_input_current_limit(struct udevice *dev);

int max77818_enable_safeout1(void);

#endif /* CHARGRE_INIT_H */
