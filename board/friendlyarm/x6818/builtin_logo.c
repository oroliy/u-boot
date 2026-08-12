// SPDX-License-Identifier: GPL-2.0+
/*
 * x6818 built-in splash fallback.
 *
 * The vendor U-Boot stores the 1024x600 logo as BGR888 data in logo.c and
 * uses it when the external BMP is unavailable. Keep the same pixel order,
 * but use the framebuffer allocated by the modern video uclass.
 */

#include <errno.h>
#include <dm.h>
#include <stdio.h>
#include <video.h>

#include <linux/types.h>

#define X6818_LOGO_WIDTH	1024
#define X6818_LOGO_HEIGHT	600
#define X6818_LOGO_BPP		3

extern const unsigned char logo[X6818_LOGO_WIDTH * X6818_LOGO_HEIGHT *
				X6818_LOGO_BPP];

int x6818_display_builtin_logo(struct udevice *dev)
{
	struct video_priv *priv;
	u32 *fb;
	unsigned int width, height, x, y;

	if (!dev)
		return -ENODEV;

	priv = dev_get_uclass_priv(dev);
	if (!priv || !priv->fb)
		return -ENODEV;

	if (priv->bpix != VIDEO_BPP32 || priv->line_length < X6818_LOGO_WIDTH * 4)
		return -ENOTSUPP;

	width = min_t(unsigned int, priv->xsize, X6818_LOGO_WIDTH);
	height = min_t(unsigned int, priv->ysize, X6818_LOGO_HEIGHT);

	for (y = 0; y < height; y++) {
		const u8 *src = logo + y * X6818_LOGO_WIDTH * X6818_LOGO_BPP;
		u8 *dst = (u8 *)priv->fb + y * priv->line_length;

		for (x = 0; x < width; x++) {
			/* logo[] is BGR888, matching the Nexell X8R8G8B8 memory order. */
			if (priv->format == VIDEO_RGBA8888) {
				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
				dst[3] = 0xff;
			} else {
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = 0;
			}

			src += X6818_LOGO_BPP;
			dst += 4;
		}
	}

	video_set_flush_dcache(dev, true);
	video_damage(dev, 0, 0, width, height);

	if (video_sync(dev, true))
		return -EIO;

	fb = (u32 *)priv->fb;
	printf("x6818: built-in logo fb=%p first=%08x %08x %08x %08x\n",
	       priv->fb, fb[0], fb[1], fb[2], fb[3]);

	return 0;
}
