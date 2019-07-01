#include "lcd_init.h"

#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>

void zs_do_config_touch_powerctrl_pins(void)
{
    printf("Turning on touch power..\n");
    gpio_request(IMX_GPIO_NR(1, 11), "TOUCH_PWR_EN");
    gpio_direction_output(IMX_GPIO_NR(1, 11), 1);
}
