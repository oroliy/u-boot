/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef X6818_BOOT_CONSOLE_H
#define X6818_BOOT_CONSOLE_H

#include <errno.h>
#include <string.h>

/* Preserve storage/network/init arguments, including quoted values, while
 * giving every FDT boot route the same physical console contract.
 */
static inline int x6818_console_bootargs(char *out, size_t size,
				       const char *input)
{
	static const char consoles[] =
		"console=tty1 console=ttySAC0,115200n8 consoleblank=0";
	size_t used = 0;
	const char *start;
	int quoted;
	size_t len;

	if (!size)
		return -ENOSPC;
	out[0] = '\0';
	while (input && *input) {
		while (*input == ' ' || *input == '\t' ||
		       *input == '\r' || *input == '\n')
			input++;
		if (!*input)
			break;
		start = input;
		quoted = 0;
		while (*input) {
			if (*input == '"')
				quoted = !quoted;
			if (!quoted && (*input == ' ' || *input == '\t' ||
					*input == '\r' || *input == '\n'))
				break;
			input++;
		}
		len = input - start;
		if ((len >= 8 && !strncmp(start, "console=", 8)) ||
		    (len >= 13 && !strncmp(start, "consoleblank=", 13)))
			continue;
		if (len + !!used >= size - used)
			return -ENOSPC;
		if (used)
			out[used++] = ' ';
		memcpy(out + used, start, len);
		used += len;
		out[used] = '\0';
	}
	if (sizeof(consoles) + !!used > size - used)
		return -ENOSPC;
	if (used)
		out[used++] = ' ';
	memcpy(out + used, consoles, sizeof(consoles));
	return 0;
}

#endif
