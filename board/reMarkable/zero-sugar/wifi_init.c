#include "wifi_init.h"

#include <asm/arch/clock.h>
#include <asm/gpio.h>
#include <command.h>

void zs_do_config_wifi_powerctrl_pins(void)
{
    gpio_request(IMX_GPIO_NR(6, 13), "WIFI_PWR_EN");
    gpio_direction_output(IMX_GPIO_NR(6, 13) , 1);
}

void zs_do_setup_32K_wifi_clk(void)
{
    /* Set 32K clock source for the CLKO2 clock */
    printf("Setting IPP_D0_CLKO2 to get input from OSC_32K_CLK..\n");
    clock_set_src(IPP_DO_CLKO2, OSC_32K_CLK);
}
U_BOOT_CMD(
    32K_wifi_clk_on, 1,	1, zs_do_setup_32K_wifi_clk,
	"Turn on 32K clock for external wifi module",
	"Turn on the 32K clock which is required for the external wifi module to run"
);
