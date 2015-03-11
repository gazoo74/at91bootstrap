/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2006, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
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
#include "common.h"
#include "hardware.h"
#include "arch/at91_pmc.h"
#include "string.h"
#include "slowclk.h"
#include "dataflash.h"
#include "nandflash.h"
#include "sdcard.h"
#include "fdt.h"
#include "mon.h"
#include "tz_utils.h"
#include "kernel.h"

#include "debug.h"

#ifdef CMDLINE
char *bootargs = CMDLINE;
#endif

#ifdef CONFIG_OF_LIBFDT
/* The /chosen node
 * property "bootargs": This zero-terminated string is passed
 * as the kernel command line.
 */
static int fixup_chosen_node(void *blob, const char *bootargs)
{
	int nodeoffset;
	const char *value = bootargs;
	int valuelen = strlen(value) + 1;
	int ret;

	ret = of_get_node_offset(blob, "chosen", &nodeoffset);
	if (ret) {
		dbg_info("Kernel/FDT: Failed to get chosen node!\n");
		return ret;
	}

	ret = of_set_property(blob, nodeoffset, "bootargs", value, valuelen);
	if (ret) {
		dbg_info("Kernel/FDT: Failed to set bootargs property!\n");
		return ret;
	}

	return 0;
}

/* The /memory node
 * Required properties:
 * - device_type: has to be "memory".
 * - reg: this property contains all the physical memory ranges of your boards.
 */
static int fixup_memory_node(void *blob,
			     unsigned int *mem_bank,
			     unsigned int *mem_size)
{
	int nodeoffset;
	unsigned int data[2];
	int valuelen;
	int ret;

	ret = of_get_node_offset(blob, "memory", &nodeoffset);
	if (ret) {
		dbg_info("Kernel/FDT: Failed to add memory node!\n");
		return ret;
	}

	/* set "device_type" property */
	ret = of_set_property(blob, nodeoffset,
			      "device_type", "memory", sizeof("memory"));
	if (ret) {
		dbg_info("Kernel/FDT: Failed to set device_type property!\n");
		return ret;
	}

	/* set "reg" property */
	valuelen = 8;
	data[0] = swap_uint32(*mem_bank);
	data[1] = swap_uint32(*mem_size);

	ret = of_set_property(blob, nodeoffset, "reg", data, valuelen);
	if (ret) {
		dbg_info("Kernel/FDT: Failed to set reg property!\n");
		return ret;
	}

	return 0;
}

static int setup_dt_blob(void *blob)
{
	unsigned int mem_bank = MEM_BANK;
	unsigned int mem_size = MEM_SIZE;
	int ret;

	if (check_dt_blob_valid(blob)) {
		dbg_info("DT: the blob is not a valid fdt\n");
		return -1;
	}

	dbg_info("\nUsing device tree in place at %d\n",
						(unsigned int)blob);

#ifdef CMDLINE
	if (bootargs) {
		char *p;

		/* set "/chosen" node */
		for (p = bootargs; *p == ' '; p++)
			;

		if (*p == '\0')
			return -1;

		ret = fixup_chosen_node(blob, p);
		if (ret)
			return ret;
	}
#endif

	ret = fixup_memory_node(blob, &mem_bank, &mem_size);
	if (ret)
		return ret;

	return 0;
}
#endif /* #ifdef CONFIG_OF_LIBFDT */

static int load_kernel_image(struct image_info *image)
{
	int ret;

#if defined(CONFIG_DATAFLASH)
	ret = load_dataflash(image);
#elif defined(CONFIG_NANDFLASH)
	ret = load_nandflash(image);
#elif defined(CONFIG_SDCARD)
	ret = load_sdcard(image);
#else
#error "No booting media specified!"
#endif
	if (ret)
		return ret;

	return 0;
}

int load_kernel(struct image_info *image)
{
	unsigned char *addr = image->dest;
	unsigned int entry_point;
	unsigned int r2;
	unsigned int mach_type;
	int ret;

	void (*kernel_entry)(int zero, int arch, unsigned int params);

	ret = load_kernel_image(image);
	if (ret)
		return ret;

#ifdef CONFIG_SCLK
	slowclk_switch_osc32();
#endif

#if defined(CONFIG_LINUX_IMAGE)
	ret = boot_image_setup(addr, &entry_point);
#endif
	if (ret)
		return -1;

	kernel_entry = (void (*)(int, int, unsigned int))entry_point;

#ifdef CONFIG_OF_LIBFDT
	ret = setup_dt_blob((char *)image->of_dest);
	if (ret)
		return ret;

	mach_type = 0xffffffff;
	r2 = (unsigned int)image->of_dest;
#else
	atags_setup_boot_params();

	mach_type = MACH_TYPE;
	r2 = (unsigned int)(MEM_BANK + 0x100);
#endif

	dbg_info("\nStarting linux kernel ..., machid: %d\n\n",
							mach_type);
#if defined(CONFIG_ENTER_NWD)
	monitor_init();

	init_loadkernel_args(0, mach_type, r2, (unsigned int)kernel_entry);

	dbg_info("Enter Normal World, Run Kernel at %d\n",
					(unsigned int)kernel_entry);

	enter_normal_world();
#else
	kernel_entry(0, mach_type, r2);
#endif

	return 0;
}
