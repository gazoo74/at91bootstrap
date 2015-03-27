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
#ifndef __ATAGS_H__
#define __ATAGS_H__

#define TAG_FLAG_NONE		0x00000000
#define TAG_FLAG_CORE		0x54410001
#define TAG_FLAG_MEM		0x54410002
#define TAG_FLAG_SERIAL		0x54410006
#define TAG_FLAG_REVISION	0x54410007
#define TAG_FLAG_CMDLINE	0x54410009

#define	TAG_SIZE_HEADER		8
#define TAG_SIZE_CORE		5
#define TAG_SIZE_MEM32		4
#define TAG_SIZE_SERIAL		4
#define TAG_SIZE_REVISION	3

struct tag_header {
	unsigned int	size;
	unsigned int	tag;
};

struct tag_core {
	struct tag_header	header;
	unsigned int		flags;
	unsigned int		pagesize;
	unsigned int		rootdev;
};

struct tag_mem32 {
	struct tag_header	header;
	unsigned int		size;
	unsigned int		start;
};

struct tag_serial {
	struct tag_header	header;
	unsigned int		low;
	unsigned int		high;
};

struct tag_revision {
	struct tag_header	header;
	unsigned int		version;
};

struct tag_cmdline {
	struct tag_header	header;
	char			cmdline[1];
};

struct tag_none {
	struct tag_header	header;
};

static inline void atags_setup_commandline_tag(struct tag_cmdline *params,
					       const char *commandline)
{
	const char *p;

	if (!commandline)
		return;

	for (p = commandline; *p == ' '; p++)
		;

	if (*p == '\0')
		return;

	params->header.tag = TAG_FLAG_CMDLINE;
	params->header.size = (TAG_SIZE_HEADER + strlen(p) + 1 + 4) >> 2;

	strcpy(params->cmdline, p);
}

static inline void atags_setup_boot_params(unsigned int *params)
{
	struct tag_core *coreparam = (struct tag_core *) params;
	coreparam->header.tag = TAG_FLAG_CORE;
	coreparam->header.size = TAG_SIZE_CORE;

	coreparam->flags = 0;
	coreparam->pagesize = 0;
	coreparam->rootdev = 0;

	params = (unsigned int *)params + TAG_SIZE_CORE;

	struct tag_mem32 *memparam = (struct tag_mem32 *)params;
	memparam->header.tag = TAG_FLAG_MEM;
	memparam->header.size = TAG_SIZE_MEM32;

	memparam->start = MEM_BANK;
	memparam->size = MEM_SIZE;

	params = (unsigned int *)params + TAG_SIZE_MEM32;

	struct tag_cmdline *cmdparam = (struct tag_cmdline *)params;
	atags_setup_commandline_tag(cmdparam, bootargs);

	params = (unsigned int *)params + cmdparam->header.size;

#ifdef CONFIG_LOAD_ONE_WIRE
	struct tag_revision *revparam = (struct tag_revision *)params;
	revparam->header.tag = TAG_FLAG_REVISION;
	revparam->header.size = TAG_SIZE_REVISION;
	revparam->version = get_sys_rev();

	params = (unsigned int *)params + TAG_SIZE_REVISION;

	struct tag_serial *serialparam = (struct tag_serial *)params;
	serialparam->header.tag = TAG_FLAG_SERIAL;
	serialparam->header.size = TAG_SIZE_SERIAL;
	serialparam->low = get_sys_sn();
	serialparam->high = 0;

	params = (unsigned int *)params + TAG_SIZE_SERIAL;
#endif

	/* end tag */
	struct tag_none * noneparam = (struct tag_none *)params;
	noneparam->header.tag = TAG_FLAG_NONE;
	noneparam->header.size = 0;
}

#endif /* #ifndef __ATAGS_H__ */
