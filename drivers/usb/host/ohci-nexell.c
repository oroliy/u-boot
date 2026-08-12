// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 OHCI host controller.
 */

#include <asm/arch/clk.h>
#include <asm/arch/nexell.h>
#include <dm.h>
#include <errno.h>

#include "nexell-usb-host.h"
#include "ohci.h"

#define NEXELL_USB_HOST_REFCLK		12000000UL

struct nexell_ohci_priv {
	ohci_t ohci;
	struct clk *clk;
};

static int nexell_ohci_probe(struct udevice *dev)
{
	struct nexell_ohci_priv *priv = dev_get_priv(dev);
	struct ohci_regs *regs = dev_read_addr_ptr(dev);
	long rate;
	int ret;

	if (!regs)
		return -EINVAL;

	priv->clk = clk_get(DEV_NAME_USB2HOST);
	if (!priv->clk)
		return -ENOENT;

	rate = clk_set_rate(priv->clk, NEXELL_USB_HOST_REFCLK);
	if (rate < 0)
		return rate;

	ret = clk_enable(priv->clk);
	if (ret)
		return ret;

	nexell_usb_host_phy_init(NEXELL_USB_HOST_OHCI);

	ret = ohci_register(dev, regs);
	if (ret) {
		nexell_usb_host_phy_exit();
		clk_disable(priv->clk);
	}

	return ret;
}

static int nexell_ohci_remove(struct udevice *dev)
{
	struct nexell_ohci_priv *priv = dev_get_priv(dev);
	int ret;

	ret = ohci_deregister(dev);
	nexell_usb_host_phy_exit();
	if (priv->clk)
		clk_disable(priv->clk);

	return ret;
}

static const struct udevice_id nexell_ohci_ids[] = {
	{ .compatible = "nexell,s5p6818-ohci" },
	{ }
};

U_BOOT_DRIVER(ohci_nexell) = {
	.name		= "ohci_nexell",
	.id		= UCLASS_USB,
	.of_match	= nexell_ohci_ids,
	.probe		= nexell_ohci_probe,
	.remove		= nexell_ohci_remove,
	.ops		= &ohci_usb_ops,
	.priv_auto	= sizeof(struct nexell_ohci_priv),
	.flags		= DM_FLAG_ALLOC_PRIV_DMA,
};
