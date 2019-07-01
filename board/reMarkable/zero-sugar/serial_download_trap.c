#include "serial_download_trap.h"

#include <asm/arch/mx7-pins.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <mmc.h>

static iomux_v3_cfg_t const pogo_pads[] = {
    MX7D_PAD_SAI1_TX_DATA__GPIO6_IO15 | MUX_PAD_CTRL(PAD_CTL_PUS_PU5KOHM),
    MX7D_PAD_SAI1_RX_DATA__GPIO6_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL),
    MX7D_PAD_SAI1_RX_BCLK__GPIO6_IO17 | MUX_PAD_CTRL(PAD_CTL_PUS_PU100KOHM)
};

#define TRAPMODE_POGO 1
#define TRAPMODE_USB 2
int probe_serial_download_trap(void)
{
    int msecs = 0;
    int pogoInput = 0;
    int usbInput = 0;
    int detectionCode = 0xB4B4;
    int curDetectionPos = 0;
    int curOutputValue = 0;
    int curInputValue = 0;
    int trapMode = 0;

    bool serial_download_trap_is_active = false;

    imx_iomux_v3_setup_multiple_pads(pogo_pads, ARRAY_SIZE(pogo_pads));

    gpio_request(IMX_GPIO_NR(6, 15), "POGO GPIO PIN");
    gpio_direction_input(IMX_GPIO_NR(6, 15));

    gpio_request(IMX_GPIO_NR(6, 12), "USB-C LOCAL SBU2");
    gpio_direction_output(IMX_GPIO_NR(6, 12), 0);

    gpio_request(IMX_GPIO_NR(6, 17), "USB-C LOCAL SBU1");
    gpio_direction_input(IMX_GPIO_NR(6, 17));


    printf("\n----------------------------------------------\n");
    printf("Probing for serial download trap..\n");
    while (msecs < 2000) {
        printf("----------------------------------------------\n");
        pogoInput = gpio_get_value(IMX_GPIO_NR(6, 15));
        usbInput = gpio_get_value(IMX_GPIO_NR(6, 17));
        printf("msecs: %d\n", msecs);
        printf("Detection state: %s\n", (serial_download_trap_is_active ? "DETECTING" : "NOT DETECTING"));

        /* Check if state has changed */
        if (!serial_download_trap_is_active && ((pogoInput == 0) || (usbInput == 0)))
        {
            // Active pin detected, restart process
            printf("Entering detection state !\n");
            msecs = 0;
            curDetectionPos = 0;
            curOutputValue = detectionCode;
            curInputValue = 0;
            serial_download_trap_is_active = true;

            trapMode = ((pogoInput == 0) ? TRAPMODE_POGO : TRAPMODE_USB);
        }

        if (serial_download_trap_is_active) {
            switch(trapMode) {
                case TRAPMODE_USB:
                    printf("Current trap mode USB\n");
                    if (curDetectionPos < 16)
                    {
                        printf("Generating detection bit %d\n", curDetectionPos);
                        printf("curOutputValue: 0x%04X => setting output = %d\n", curOutputValue, curOutputValue & 0x01);
                        gpio_set_value(IMX_GPIO_NR(6, 12), curOutputValue & 0x01);
                        udelay(1000);

                        curInputValue = gpio_get_value(IMX_GPIO_NR(6, 17));
                        printf("Read input: %d\n", curInputValue);
                        if(curInputValue == (curOutputValue & 0x01)) {
                            printf("Match !\n");

                            curDetectionPos++;
                            curOutputValue >>= 1;

                            if (curDetectionPos >= 16) {
                                printf("------------------------------------------------------\n");
                                printf("Sequence complete - enabling serial download mode !!\n\n");
                                erase_boot0();
                                printf("\n\nPlease restart device to enter serial download mode !\n");
                                printf("------------------------------------------------------\n");
                                while(1);
                            }

                            printf("curDetectionPos => %d\n", curDetectionPos);
                            printf("curOutputValue => 0x%04X\n", curOutputValue);
                        }
                        else {
                            printf("Mismatch .. aborting \n");
                            msecs = 0;
                            gpio_set_value(IMX_GPIO_NR(6, 12), 0);
                            serial_download_trap_is_active = false;
                        }
                    }
                    break;

                case TRAPMODE_POGO:
                    printf("Current trapmode POGO\n");
                    if (pogoInput == 0){
                        printf("POGO serial download trap still detected\n");
                        if (msecs >= 1500) {
                            printf("Sequence complete - enabling serial download mode !!\n\n");
                            erase_boot0();
                            printf("\n\nPlease restart device to enter serial download mode !\n");
                            printf("------------------------------------------------------\n");
                            while(1);
                        }
                    }
                    else {
                        printf("POGO serial download trap no longer active, aborting\n");
                        msecs = 0;
                        serial_download_trap_is_active = false;
                    }
                    break;

                default:
                    printf("Invalid trapmode, aborting\n");
                    printf("----------------------------------------------\n");
                    return;
            }
        }

        udelay(100000);
        msecs += 100;
    }

    printf("-----------------------------------------------------------------------\n");
    printf("msecs: %d\n", msecs);
    printf("Serial download mode request not detected, continuing normal boot..\n");
    printf("-----------------------------------------------------------------------\n");
}
