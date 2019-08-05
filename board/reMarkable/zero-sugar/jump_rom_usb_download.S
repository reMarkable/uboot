/*
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm-offsets.h>
#include <config.h>
#include <linux/linkage.h>

.pushsection .text.lowlevel_init, "ax"
WEAK(jump_rom_usb_download)
	ldr	r9, =0x72a4   /* TO1.3 */
	/* make it thumb instruction */
	add	r9, r9, #0x1
	bx	r9
ENDPROC(jump_rom_usb_download)
.popsection
