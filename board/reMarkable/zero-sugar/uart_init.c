#include "uart_init.h"

#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/mx7-pins.h>

#define UART_PAD_CTRL  (PAD_CTL_DSE_3P3V_49OHM | PAD_CTL_PUS_PU100KOHM | PAD_CTL_HYS)

static iomux_v3_cfg_t const uart1_pads[] = {
	MX7D_PAD_UART1_TX_DATA__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX7D_PAD_UART1_RX_DATA__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
}
