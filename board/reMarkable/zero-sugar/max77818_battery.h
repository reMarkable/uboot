#ifndef FG_INIT_H
#define FG_INIT_H

#include <linux/types.h>

int max77818_init_fg_device(void);
int max77818_read_fgcc_state(bool *state);
int max77818_set_fgcc_state(bool enabled, bool *restore_state);
int max77818_restore_fgcc(bool restore_state);

int max77818_get_battery_capacity(u8 *capacity);

#endif /* FG_INIT_H */
