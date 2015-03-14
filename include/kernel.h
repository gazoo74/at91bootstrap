/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2015, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <string.h>

#ifndef CONFIG_OF_LIBFDT
#include "atags.h"
#endif

#include "debug.h"

#define LINUX_UIMAGE_MAGIC	0x27051956
struct uimage_header {
	unsigned int	magic;
	unsigned int	header_crc;
	unsigned int	time;
	unsigned int	size;
	unsigned int	load;
	unsigned int	entry_point;
	unsigned int	data_crc;
	unsigned char	os_type;
	unsigned char	arch;
	unsigned char	image_type;
	unsigned char	comp_type;
	unsigned char	name[32];
};

#define LINUX_ZIMAGE_MAGIC	0x18286f01
struct zimage_header {
	unsigned int	code[9];
	unsigned int	magic;
	unsigned int	start;
	unsigned int	end;
};

static inline int kernel_size(unsigned char *addr)
{
	struct uimage_header *uimage_header = (struct uimage_header *)addr;
	struct zimage_header *zimage_header = (struct zimage_header *)addr;
	unsigned int size = -1;

	if (swap_uint32(uimage_header->magic) == LINUX_UIMAGE_MAGIC)
		size = swap_uint32(uimage_header->size)
			+ sizeof(struct uimage_header);
	else if (swap_uint32(zimage_header->magic) == LINUX_ZIMAGE_MAGIC)
		size = zimage_header->end + zimage_header->start;

	if ((int)size < 0)
		return -1;

	return (int)size;
}

static inline int boot_image_setup(unsigned char *addr, unsigned int *entry)
{
	struct zimage_header *zimage_header = (struct zimage_header *)addr;
	struct uimage_header *uimage_header = (struct uimage_header *)addr;
	unsigned int src, dest;
	unsigned int size;

	if (swap_uint32(zimage_header->magic) == LINUX_ZIMAGE_MAGIC) {
		dbg_info("Kernel: Booting zImage...\n");

		*entry = ((unsigned int)addr + zimage_header->start);
		return 0;
	}
	else if (swap_uint32(uimage_header->magic) == LINUX_UIMAGE_MAGIC) {
		dbg_info("Kernel: Booting uImage...\n");

		if (uimage_header->comp_type != 0) {
			dbg_info("Kernel: Compress image is not supported yet!\n");
			return -1;
		}

		size = swap_uint32(uimage_header->size);
		dest = swap_uint32(uimage_header->load);
		src = (unsigned int)addr + sizeof(struct uimage_header);

		dbg_info("Kernel/uImage: Relocating image from %x to %x... ", src, dest);
		memcpy((void *)dest, (void *)src, size);
		dbg_info("%d bytes.\n", size);

		*entry = swap_uint32(uimage_header->entry_point);
		return 0;
	}

	dbg_info("Kernel: Unknown Image!\n");

	return -1;
}

#endif /* #ifndef __KERNEL_H__ */
