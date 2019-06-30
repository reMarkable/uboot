#include "digitizer_init.h"

#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>

void zs_do_config_digitizer_powerctrl_pins(void)
{
    printf("Configuring digitizer interrupt..\n");
    gpio_request(IMX_GPIO_NR(1, 1), "DIGITIZER_INT");
    gpio_direction_input(IMX_GPIO_NR(1, 1));

    printf("Turning on digitizer power..\n");
    gpio_request(IMX_GPIO_NR(1, 6), "DIGITIZER_PWR_EN");
    gpio_direction_output(IMX_GPIO_NR(1, 6), 1);
}
