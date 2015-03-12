/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2012, Atmel Corporation
 *               2015, Gaël PORTAY <gael.portay@gmail.com>
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
#include "string.h"
#ifdef CONFIG_DEBUG
#include "debug.h"
#endif /* CONFIG_DEBUG */

struct boot_param_header {
	unsigned int	magic_number;
	unsigned int	total_size;
	unsigned int	offset_dt_struct;
	unsigned int	offset_dt_strings;
	unsigned int	offset_reserve_map;
	unsigned int	format_version;
	unsigned int	last_compatible_version;

	/* version 2 field */
	unsigned int	mach_id;
	/* version 3 field */
	unsigned int	dt_strings_len;
	/* version 17 field */
	unsigned int	dt_struct_len;
};

unsigned int of_get_magic_number(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->magic_number);
}

unsigned int of_get_format_version(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->format_version);
}

unsigned int of_get_offset_dt_strings(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->offset_dt_strings);
}

void of_set_offset_dt_strings(void *blob, unsigned int offset)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	header->offset_dt_strings = swap_uint32(offset);
}

const char *of_get_string_by_offset(void *blob, unsigned int offset)
{
	return (const char *)((unsigned int)blob
				+ of_get_offset_dt_strings(blob) + offset);
}

unsigned int of_get_offset_dt_struct(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->offset_dt_struct);
}

unsigned int of_dt_struct_offset(void *blob, unsigned int offset)
{
	return (unsigned int)blob + of_get_offset_dt_struct(blob) + offset;
}

unsigned int of_get_dt_total_size(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->total_size);
}

void of_set_dt_total_size(void *blob, unsigned int size)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	header->total_size = swap_uint32(size);
}

unsigned int of_get_dt_strings_len(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->dt_strings_len);
}

void of_set_dt_strings_len(void *blob, unsigned int len)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	header->dt_strings_len = swap_uint32(len);
}

unsigned int of_get_dt_struct_len(void *blob)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	return swap_uint32(header->dt_struct_len);
}

void of_set_dt_struct_len(void *blob, unsigned int len)
{
	struct boot_param_header *header = (struct boot_param_header *)blob;

	header->dt_struct_len = swap_uint32(len);
}

unsigned int of_blob_data_size(void *blob)
{
	return (unsigned int)of_get_offset_dt_strings(blob)
			+ of_get_dt_strings_len(blob);
}

int (*of_element_cb) (const char *name) = NULL;
int (*of_property_cb) (const char *name,
		       const unsigned char *data,
		       unsigned int size) = NULL;

#ifdef CONFIG_OF_DEBUG
static char indent[10];
static int level;

inline int is_character(unsigned char c) {
	return (c >= 0x20) && (c < 0x7f);
}

inline int is_string(const unsigned char *data, unsigned int size)
{
	int i;

	for (i = 0; i < size-1; i++)
		if (!is_character(data[i]))
			return 0;

	return 1;
}

inline void dump(const unsigned char *data, unsigned int size)
{
	int i;

	for (i = 0; i < size; i ++) {
		dbg_info(" %02x", data[i]);
	}

	dbg_info(" | ");

	for (i = 0; i < size; i ++) {
		if (is_character(data[i])) {
			dbg_info("%c", data[i]);
		}
		else {
			dbg_info("%c", '.');
		}
	}
}

int dbg_dump_element(const char *name)
{
	if (name) {
		dbg_info("%s%s {", indent, *name ? name : "/");
		dbg_loud(" // length: %u, aligned: %u", strlen(name), OF_ALIGN(strlen(name)+1));
		dbg_info("\n");
		indent[level++] = '\t';
	}
	else {
		indent[--level] = 0;
		dbg_info("%s};\n", indent);
	}

	return 0;
}

int dbg_dump_property(const char *name,
		      const unsigned char *data,
		      unsigned int size)
{
	dbg_info("%s%s = ", indent, name);
	if (is_string(data, size)) {
		dbg_info("\"%s\";", (const char *) data);
	}
	else if (size == 4) {
		unsigned int *u32 = (unsigned int *) data;
		dbg_info("<0x%08x>;", swap_uint32(*u32));
	}
	else {
		dbg_info("<", name);
		dump(data, MIN(16, size));
		dbg_info("%s>;", MIN(16, size) < size ? " (...) " : " ");
	}
	dbg_loud(" // size: 0x%08x\n", size);
	dbg_info("\n");

	return 0;
}

of_element_cb = dbg_dump_element;
of_property_cb = dbg_dump_property;
#endif /* CONFIG_OF_DEBUG */

unsigned char *recurse(unsigned char *blob, unsigned char *node)
{
	unsigned int type, *ptr = (unsigned int *) node;

	type = swap_uint32(*ptr++);
	if (type == 1) {
		if (!of_element_cb || of_element_cb((const char *) ptr))
			return node;

		ptr += (OF_ALIGN(strlen((const char *) ptr)+1) >> 2);
	}
	else if (type == 2) {
		if (!of_element_cb || of_element_cb(NULL))
			return node;
	}
	else if (type == 3) {
		unsigned int size = swap_uint32(*ptr++);
		unsigned int offset = swap_uint32(*ptr++);
		const char *name = (const char *) &blob[of_get_offset_dt_strings(blob) + offset];
		unsigned char *data = (unsigned char *) ptr;

		if (!of_property_cb || of_property_cb(name, data, size))
			return node;

		ptr += (OF_ALIGN(size) >> 2);
	}
	else if (type == 9) {
		return (unsigned char *) ptr;
	}
	else {
		dbg_info("OF: Invalid type %u!\n", type);
		return NULL;
	}

	return recurse(blob, (unsigned char *) ptr);
}

unsigned char *of_get_root_node(unsigned char *blob)
{
	return blob + of_get_offset_dt_struct(blob);
}

unsigned char *of_recurse(unsigned char *blob) {
	return recurse(blob, of_get_root_node(blob));
}
