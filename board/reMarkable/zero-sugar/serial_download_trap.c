#include "serial_download_trap.h"

#include <asm/arch/mx7-pins.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>

static iomux_v3_cfg_t const pogo_pads[] = {
    MX7D_PAD_SAI1_RX_DATA__GPIO6_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL),
    MX7D_PAD_SAI1_RX_BCLK__GPIO6_IO17 | MUX_PAD_CTRL(PAD_CTL_PUS_PU100KOHM)
};

extern int jump_rom_usb_download(void);

int probe_serial_download_trap(void)
{
    int usbInput;

    imx_iomux_v3_setup_multiple_pads(pogo_pads, ARRAY_SIZE(pogo_pads));

    gpio_request(IMX_GPIO_NR(6, 12), "USB-C LOCAL SBU2");
    gpio_direction_output(IMX_GPIO_NR(6, 12), 0);

    gpio_request(IMX_GPIO_NR(6, 17), "USB-C LOCAL SBU1");
    gpio_direction_input(IMX_GPIO_NR(6, 17));

    /*
     * Due to the internal 100K pullup on pad, GPIO6_17 should be read
     * as 1 here.
     */
    usbInput = gpio_get_value(IMX_GPIO_NR(6, 17));

    /* Set GPIO6_12 to 0 ... */
    gpio_set_value(IMX_GPIO_NR(6, 12), 0);
    udelay(1000);

    /* ... if GPIO6_17 follows to be 0, we get it! */
    usbInput = gpio_get_value(IMX_GPIO_NR(6, 17));
    if (usbInput == 0) {
        printf("Going to serial download mode ...\n");
        jump_rom_usb_download();
        while (1);
    }

    return 0;
}
