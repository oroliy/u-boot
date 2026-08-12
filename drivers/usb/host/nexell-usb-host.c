// SPDX-License-Identifier: GPL-2.0+
/*
 * Shared S5P6818 USB 2.0 host PHY and controller setup.
 *
 * EHCI and OHCI have separate register windows, but share the host PHY,
 * reset, clock and TIEOFF configuration on this SoC.
 */

#include <asm/io.h>
#include <asm/arch/ehci.h>
#include <asm/arch/nexell.h>
#include <asm/arch/reset.h>
#include <linux/delay.h>

#include "nexell-usb-host.h"

static void nexell_usb_host_set_fl_adj(void __iomem *tieoff)
{
	const u32 fladj = 0x20;
	u32 value = fladj;
	int bit;

	/* The S5P6818 tieoff stores one 3-bit value for each FLADJ bit. */
	for (bit = 0; bit < NX_HOST_CON2_SS_FLADJ_VAL_NUM; bit++) {
		if (fladj & BIT(bit))
			value |= NX_HOST_CON2_SS_FLADJ_VAL_MAX <<
				(NX_HOST_CON2_SS_FLADJ_VAL_0_OFFSET -
				 bit * NX_HOST_CON2_SS_FLADJ_VAL_OFFSET);
	}

	writel(value, tieoff + NX_HOST_CON2);
}

void nexell_usb_host_phy_init(enum nexell_usb_host_type type)
{
	void __iomem *tieoff = (void __iomem *)PHY_BASEADDR_TIEOFF;
	u32 value;

	/* Match the S5P6818 vendor/kernel USB host PHY sequence. */
	nexell_usb_host_set_fl_adj(tieoff);

	nx_rstcon_setrst(RESET_ID_USB20HOST, RSTCON_ASSERT);
	udelay(1);
	nx_rstcon_setrst(RESET_ID_USB20HOST, RSTCON_NEGATE);

	value = readl(tieoff + NX_HOST_CON2);
	value &= ~NX_HOST_CON2_SS_DMA_BURST_MASK;
	value |= type == NEXELL_USB_HOST_OHCI ?
		NX_HOST_CON2_OHCI_SS_ENABLE_DMA_BURST :
		NX_HOST_CON2_EHCI_SS_ENABLE_DMA_BURST;
	writel(value, tieoff + NX_HOST_CON2);

	value = readl(tieoff + NX_HOST_CON0);
	value &= ~GENMASK(26, 25);
	value |= NX_HOST_CON0_SS_WORD_IF_16;
	writel(value, tieoff + NX_HOST_CON0);

	value = readl(tieoff + NX_HOST_CON4);
	value &= ~GENMASK(9, 8);
	value |= NX_HOST_CON4_WORDINTERFACE_16;
	writel(value, tieoff + NX_HOST_CON4);

	value = readl(tieoff + NX_HOST_CON3);
	value &= ~NX_HOST_CON3_POR_MASK;
	value |= NX_HOST_CON3_POR_ENB;
	writel(value, tieoff + NX_HOST_CON3);
	udelay(40);

	/* The board uses the UTMI path, not the optional HSIC path. */
	value = readl(tieoff + NX_HOST_CON0);
	value |= NX_HOST_CON0_N_HOST_PHY_RESET_SYNC |
		 NX_HOST_CON0_N_HOST_UTMI_RESET_SYNC;
	value &= ~NX_HOST_CON0_N_HOST_HSIC_RESET_SYNC;
	writel(value, tieoff + NX_HOST_CON0);
	udelay(2);

	value |= NX_HOST_CON0_N_RESET_SYNC |
		 NX_HOST_CON0_N_OHCI_RESET_SYNC |
		 NX_HOST_CON0_N_AUXWELL_RESET_SYNC;
	writel(value, tieoff + NX_HOST_CON0);
}

void nexell_usb_host_phy_exit(void)
{
	nx_rstcon_setrst(RESET_ID_USB20HOST, RSTCON_ASSERT);
}
