// SPDX-License-Identifier: GPL-2.0+
/* x6818 compatibility commands retained from the vendor U-Boot. */

#include <command.h>
#include <blk.h>
#include <cpu_func.h>
#include <malloc.h>
#include <mmc.h>
#include <part.h>
#include <mapmem.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <vsprintf.h>

#define X6818_MMC_BLOCK_SIZE 512

struct x6818_boot_dev_mmc {
	u8 port_no;
	u8 reserved[3];
	u32 reserved1;
	u32 crc32;
};

struct x6818_boot_header {
	u32 vector[8];
	u32 vector_rel[8];
	u32 dev_addr;
	u32 load_size;
	u32 load_addr;
	u32 jump_addr;
	struct x6818_boot_dev_mmc mmc;
	u32 reserved[104];
	u32 signature;
};

typedef char x6818_nsih_header_size_check[
	(sizeof(struct x6818_boot_header) == X6818_MMC_BLOCK_SIZE) ? 1 : -1];

static int x6818_mmc_write(struct blk_desc *desc, lbaint_t start,
			   lbaint_t count, ulong addr)
{
	void *buf = map_sysmem(addr, 0);
	void *verify;
	ulong written;
	int ret;

	flush_dcache_all();
	written = blk_dwrite(desc, start, count, buf);

	if (written != count) {
		printf("update_mmc: write failed, expected 0x%lx blocks, got 0x%lx\n",
		       (ulong)count, written);
		unmap_sysmem(buf);
		return CMD_RET_FAILURE;
	}

	verify = malloc(X6818_MMC_BLOCK_SIZE);
	if (!verify) {
		unmap_sysmem(buf);
		return CMD_RET_FAILURE;
	}

	ret = blk_dread(desc, start, 1, verify) == 1 &&
	      !memcmp(buf, verify, X6818_MMC_BLOCK_SIZE);
	unmap_sysmem(buf);
	if (!ret) {
		printf("update_mmc: readback verification failed at block 0x%lx\n",
		       (ulong)start);
		free(verify);
		return CMD_RET_FAILURE;
	}
	free(verify);

	printf("update_mmc: wrote 0x%lx blocks at block 0x%lx; readback OK\n",
	       (ulong)count, (ulong)start);
	return CMD_RET_SUCCESS;
}

static int do_x6818_update_mmc(struct cmd_tbl *cmdtp, int flag, int argc,
			       char *const argv[])
{
	struct blk_desc *desc;
	struct disk_partition part;
	struct x6818_boot_header *header;
	const char *type;
	ulong mem, dst_addr, length, load_addr = CONFIG_TEXT_BASE;
	lbaint_t start, count;
	int ret, dev;

	if (argc < 6 || argc > 7)
		return CMD_RET_USAGE;

	ret = blk_get_device_by_str("mmc", argv[1], &desc);
	if (ret < 0) {
		printf("update_mmc: cannot open mmc %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}
	dev = hextoul(argv[1], NULL);
	ret = blk_select_hwpart_devnum(UCLASS_MMC, dev, 0);
	if (ret < 0) {
		printf("update_mmc: cannot select mmc %s user area (%d)\n",
		       argv[1], ret);
		return CMD_RET_FAILURE;
	}

	type = argv[2];
	mem = hextoul(argv[3], NULL);
	dst_addr = hextoul(argv[4], NULL);
	length = hextoul(argv[5], NULL);
	count = DIV_ROUND_UP(length, X6818_MMC_BLOCK_SIZE);

	if (!count || !length ||
	    length > (ulong)(~0UL - X6818_MMC_BLOCK_SIZE))
		return CMD_RET_USAGE;

	if (!strcmp(type, "2ndboot")) {
		if (dst_addr % X6818_MMC_BLOCK_SIZE)
			return CMD_RET_USAGE;
		start = dst_addr / X6818_MMC_BLOCK_SIZE;
		header = map_sysmem(mem, sizeof(*header));
		header->mmc.port_no = hextoul(argv[1], NULL);
		unmap_sysmem(header);
	} else if (!strcmp(type, "boot")) {
		if (argc == 7)
			load_addr = hextoul(argv[6], NULL);
		if (!dst_addr || dst_addr % X6818_MMC_BLOCK_SIZE ||
		    mem < X6818_MMC_BLOCK_SIZE ||
		    length > (ulong)(~0UL - X6818_MMC_BLOCK_SIZE))
			return CMD_RET_FAILURE;

		start = dst_addr / X6818_MMC_BLOCK_SIZE;
		mem -= X6818_MMC_BLOCK_SIZE;
		header = map_sysmem(mem, sizeof(*header));
		memset(header, 0, X6818_MMC_BLOCK_SIZE);
		header->load_size = length;
		header->load_addr = load_addr;
		header->jump_addr = load_addr;
		header->mmc.port_no = hextoul(argv[1], NULL);
		header->signature = 0x4853494e;
		unmap_sysmem(header);
		length += X6818_MMC_BLOCK_SIZE;
		count = DIV_ROUND_UP(length, X6818_MMC_BLOCK_SIZE);
	} else if (!strcmp(type, "part")) {
		int partno = dst_addr;

		if (partno < 1 || part_get_info(desc, partno, &part)) {
			printf("update_mmc: invalid partition %d\n", partno);
			return CMD_RET_FAILURE;
		}
		if (count > part.size)
			return CMD_RET_FAILURE;
		start = part.start;
	} else if (!strcmp(type, "raw")) {
		/* The vendor command accepts a byte address, then writes whole blocks. */
		if (dst_addr % X6818_MMC_BLOCK_SIZE)
			return CMD_RET_USAGE;
		start = dst_addr / X6818_MMC_BLOCK_SIZE;
	} else {
		return CMD_RET_USAGE;
	}

	if (start == 0) {
		printf("update_mmc: refusing to write the MBR block\n");
		return CMD_RET_FAILURE;
	}

	printf("update_mmc %s: mmc %s block 0x%lx count 0x%lx mem 0x%lx\n",
	       type, argv[1], (ulong)start, (ulong)count, mem);
	return x6818_mmc_write(desc, start, count, mem);
}

U_BOOT_CMD(
	update_mmc, 7, 1, do_x6818_update_mmc,
	"write an image to x6818 MMC",
	"<dev> <2ndboot|boot|raw|part> <mem> <addr|part> <length> [loadaddr]\n"
	"  Numeric arguments are hexadecimal; addr is a byte address and length is in bytes."
);
