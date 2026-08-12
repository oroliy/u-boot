// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 EHCI host controller.
 *
 * The controller needs SoC-specific TIEOFF and reset programming before
 * the standard EHCI core can access the host registers.
 */

#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/nexell.h>
#include <dm.h>
#include <mapmem.h>

#include "ehci.h"
#include "nexell-usb-host.h"

#define NEXELL_USB_HOST_REFCLK		12000000UL

struct nexell_ehci_priv {
	struct ehci_ctrl ctrl;
	struct clk *clk;
	struct ehci_hccr *hccr;
};

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

	nexell_usb_host_phy_init(NEXELL_USB_HOST_EHCI);

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
	nexell_usb_host_phy_exit();
	clk_disable(priv->clk);
	return ret;
}

static int nexell_ehci_remove(struct udevice *dev)
{
	struct nexell_ehci_priv *priv = dev_get_priv(dev);
	int ret;

	ret = ehci_deregister(dev);
	nexell_usb_host_phy_exit();
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
