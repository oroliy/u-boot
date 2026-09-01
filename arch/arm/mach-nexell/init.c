// SPDX-License-Identifier: GPL-2.0+
/*
 * AArch64 Nexell early initialisation.
 *
 * The ARMv7 Nexell CPU glue calls clk_init() from arch_cpu_init().  The
 * ARM64 build does not compile that ARMv7 directory, but the legacy Nexell
 * peripheral drivers still use the same clock table.  Initialise it before
 * timer_init() and before driver-model probing.
 */

#include <asm/arch/clk.h>

int mach_cpu_init(void)
{
	clk_init();

	return 0;
}
