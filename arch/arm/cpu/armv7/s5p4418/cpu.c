// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Nexell
 * Hyunseok, Jung <hsjung@nexell.co.kr>
 */

#include <command.h>
#include <asm/system.h>
#include <asm/cache.h>
#include <asm/sections.h>
#include <asm/io.h>
#include <asm/arch/nexell.h>
#include <asm/arch/clk.h>
#include <asm/arch/tieoff.h>
#include <cpu_func.h>

#ifndef	CONFIG_ARCH_CPU_INIT
#error must be define the macro "CONFIG_ARCH_CPU_INIT"
#endif

void s_init(void)
{
}

static void cpu_soc_init(void)
{
	if (IS_ENABLED(CONFIG_ARCH_S5P6818)) {
		/* Match the vendor reset path and stop an inherited watchdog. */
		writel(0, (void *)PHY_BASEADDR_WDT);
	}

	/*
	 * NOTE> ALIVE Power Gate must enable for Alive register access.
	 *	     must be clear wfi jump address
	 */
	writel(1, ALIVEPWRGATEREG);
	writel(0xFFFFFFFF, SCR_ARM_SECOND_BOOT);

	/* write 0xf0 on alive scratchpad reg for boot success check */
	writel(readl(SCR_SIGNAGURE_READ) | 0xF0, (SCR_SIGNAGURE_SET));

	/* The S5P4418 has these L2 retention tie-offs; 6818 does not. */
#if defined(CONFIG_ARCH_S5P4418)
	nx_tieoff_set(NX_TIEOFF_CORTEXA9MP_TOP_QUADL2C_L2RET1N_0, 1);
	nx_tieoff_set(NX_TIEOFF_CORTEXA9MP_TOP_QUADL2C_L2RET1N_1, 1);
#endif
}

int arch_cpu_init(void)
{
	flush_dcache_all();
	cpu_soc_init();
	clk_init();

	return 0;
}

#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	return 0;
}
#endif

void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
