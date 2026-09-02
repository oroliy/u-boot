// SPDX-License-Identifier: GPL-2.0+
/*
 * S5P6818 AArch64 system reset through the TF-A PSCI conduit.
 *
 * U-Boot is entered by the S5P6818 BL31 at non-secure EL2.  The platform
 * firmware conduit is fixed to SMC by the AArch64 hand-off, so call the
 * architected PSCI reset entry point directly.  This keeps reset tied to the
 * fixed SMC hand-off contract instead of depending on generic PSCI child
 * enumeration in U-Boot's driver model.
 */

#include <dm.h>
#include <errno.h>
#include <sysreset.h>
#include <asm/system.h>

static int nexell_psci_sysreset_request(struct udevice *dev,
					enum sysreset_t type)
{
	(void)dev;

	switch (type) {
	case SYSRESET_WARM:
	case SYSRESET_COLD:
		psci_system_reset();
		/* psci_system_reset() is not expected to return. */
		return -EINPROGRESS;
	default:
		return -EPROTONOSUPPORT;
	}
}

static const struct sysreset_ops nexell_psci_sysreset_ops = {
	.request = nexell_psci_sysreset_request,
};

U_BOOT_DRIVER(nexell_psci_sysreset) = {
	.name = "nexell-psci-sysreset",
	.id = UCLASS_SYSRESET,
	.ops = &nexell_psci_sysreset_ops,
};

/* The reset path is available before any DT child devices are probed. */
U_BOOT_DRVINFO(nexell_psci_sysreset) = {
	.name = "nexell-psci-sysreset",
};
