#include "wifi_init.h"

#include <asm/arch/clock.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/mx7-pins.h>
#include <command.h>

static iomux_v3_cfg_t const wifi_32K_clk_pad[] = {
    MX7D_PAD_SD1_WP__CCM_CLKO2	| MUX_PAD_CTRL(NO_PAD_CTRL),
    MX7D_PAD_ECSPI2_SS0__GPIO4_IO23 | MUX_PAD_CTRL (NO_PAD_CTRL)
};

void zs_do_wifi_poweron_cycle(void)
{
    printf("Turning off WIFI/BT power before configuring 32K clock..\n");
    gpio_request(IMX_GPIO_NR(6, 13), "WIFI_PWR_EN");
    gpio_direction_output(IMX_GPIO_NR(6, 13) , 0);

    gpio_request(IMX_GPIO_NR(4, 23), "BT_REG_EN");
    gpio_direction_output(IMX_GPIO_NR(4, 23), 0);

    printf("Setting IPP_D0_CLKO2 to get input from OSC_32K_CLK..\n");
    clock_set_src(IPP_DO_CLKO2, OSC_32K_CLK);

    printf("Configuring SD1_WP pad to output CLKO2..\n");
    imx_iomux_v3_setup_multiple_pads(wifi_32K_clk_pad, ARRAY_SIZE(wifi_32K_clk_pad));

    printf("Waiting 200us before turning on WIFI power..\n");
    udelay(200);

    printf("Turning on WIFI power..\n");
    gpio_set_value(IMX_GPIO_NR(6, 13), 1);
}
