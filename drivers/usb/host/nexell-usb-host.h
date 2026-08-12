/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __NEXELL_USB_HOST_H
#define __NEXELL_USB_HOST_H

enum nexell_usb_host_type {
	NEXELL_USB_HOST_EHCI,
	NEXELL_USB_HOST_OHCI,
};

void nexell_usb_host_phy_init(enum nexell_usb_host_type type);
void nexell_usb_host_phy_exit(void);

#endif /* __NEXELL_USB_HOST_H */
