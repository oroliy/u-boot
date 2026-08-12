// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 EHCI host controller.
 *
 * The controller needs SoC-specific TIEOFF and reset programming before
 * the standard EHCI core can access the host registers.
 */

#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/ehci.h>
#include <asm/arch/nexell.h>
#include <asm/arch/reset.h>
#include <dm.h>
#include <linux/delay.h>
#include <mapmem.h>

#include "ehci.h"

#define NEXELL_USB_HOST_REFCLK		12000000UL

struct nexell_ehci_priv {
	struct ehci_ctrl ctrl;
	struct clk *clk;
	struct ehci_hccr *hccr;
};

static void nexell_ehci_set_fl_adj(void __iomem *tieoff)
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

static void nexell_ehci_phy_init(void)
{
	void __iomem *tieoff = (void __iomem *)PHY_BASEADDR_TIEOFF;
	u32 value;

	/* Match the S5P6818 vendor/kernel EHCI PHY sequence. */
	nexell_ehci_set_fl_adj(tieoff);

	nx_rstcon_setrst(RESET_ID_USB20HOST, RSTCON_ASSERT);
	udelay(1);
	nx_rstcon_setrst(RESET_ID_USB20HOST, RSTCON_NEGATE);

	value = readl(tieoff + NX_HOST_CON2);
	value &= ~NX_HOST_CON2_SS_DMA_BURST_MASK;
	value |= NX_HOST_CON2_EHCI_SS_ENABLE_DMA_BURST;
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

	/* EHCI uses UTMI, not the optional HSIC path. */
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

static void nexell_ehci_phy_exit(void)
{
	nx_rstcon_setrst(RESET_ID_USB20HOST, RSTCON_ASSERT);
}

static int nexell_ehci_probe(struct udevice *dev)
{
	struct nexell_ehci_priv *priv = dev_get_priv(dev);
	struct ehci_hcor *hcor;
	fdt_addr_t addr;
	long rate;
	int ret;

	priv->clk = clk_get(DEV_NAME_USB2HOST);
	if (!priv->clk)
		return -ENOENT;

	rate = clk_set_rate(priv->clk, NEXELL_USB_HOST_REFCLK);
	if (rate < 0)
		return rate;

	ret = clk_enable(priv->clk);
	if (ret)
		return ret;

	nexell_ehci_phy_init();

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE) {
		ret = -EINVAL;
		goto err_phy;
	}

	priv->hccr = map_physmem(addr, 0x100, MAP_NOCACHE);
	if (!priv->hccr) {
		ret = -EINVAL;
		goto err_phy;
	}

	hcor = (struct ehci_hcor *)((uintptr_t)priv->hccr +
			HC_LENGTH(ehci_readl(&priv->hccr->cr_capbase)));

	ret = ehci_register(dev, priv->hccr, hcor, NULL, 0, USB_INIT_HOST);
	if (ret)
		goto err_phy;

	return 0;

err_phy:
	nexell_ehci_phy_exit();
	clk_disable(priv->clk);
	return ret;
}

static int nexell_ehci_remove(struct udevice *dev)
{
	struct nexell_ehci_priv *priv = dev_get_priv(dev);
	int ret;

	ret = ehci_deregister(dev);
	nexell_ehci_phy_exit();
	if (priv->clk)
		clk_disable(priv->clk);

	return ret;
}

static const struct udevice_id nexell_ehci_ids[] = {
	{ .compatible = "nexell,s5p6818-ehci" },
	{ }
};

U_BOOT_DRIVER(ehci_nexell) = {
	.name		= "ehci_nexell",
	.id		= UCLASS_USB,
	.of_match	= nexell_ehci_ids,
	.probe		= nexell_ehci_probe,
	.remove		= nexell_ehci_remove,
	.ops		= &ehci_usb_ops,
	.priv_auto	= sizeof(struct nexell_ehci_priv),
	.plat_auto	= sizeof(struct usb_plat),
	.flags		= DM_FLAG_ALLOC_PRIV_DMA,
};
