// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 direct reset path for AArch32 U-Boot.
 * AArch64 BL33 uses TF-A's PSCI SYSTEM_RESET interface instead.
 */

#include <asm/io.h>
#include <asm/arch/nexell.h>
#include <linux/bitops.h>

#define CLKPWR_PWRCONT		(PHY_BASEADDR_CLKPWR + 0x224)
#define CLKPWR_PWRMODE		(PHY_BASEADDR_CLKPWR + 0x228)
#define CLKPWR_SWRSTENB		BIT(3)
#define CLKPWR_SWRESET		BIT(12)

void reset_cpu(void)
{
	u32 val;

	/* This is the sequence used by the vendor U-Boot and Linux BSP. */
	val = readl((void __iomem *)CLKPWR_PWRCONT);
	writel(val | CLKPWR_SWRSTENB, (void __iomem *)CLKPWR_PWRCONT);
	writel(CLKPWR_SWRESET, (void __iomem *)CLKPWR_PWRMODE);

	while (1)
		;
}
