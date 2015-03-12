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
#ifndef __OF_H__
#define __OF_H__

extern unsigned int of_get_magic_number(void *blob);
extern unsigned int of_get_format_version(void *blob);

extern unsigned int of_get_offset_dt_strings(void *blob);
extern void of_set_offset_dt_strings(void *blob, unsigned int offset);

extern const char *of_get_string_by_offset(void *blob, unsigned int offset);

extern unsigned int of_get_offset_dt_struct(void *blob);
extern unsigned int of_dt_struct_offset(void *blob, unsigned int offset);

extern unsigned int of_get_dt_total_size(void *blob);
extern void of_set_dt_total_size(void *blob, unsigned int size);

extern unsigned int of_get_dt_strings_len(void *blob);
extern void of_set_dt_strings_len(void *blob, unsigned int len);

extern unsigned int of_get_dt_struct_len(void *blob);
extern void of_set_dt_struct_len(void *blob, unsigned int length);
extern unsigned int of_blob_data_size(void *blob);

typedef int (*of_element_f)(const char *name);
typedef int (*of_property_f)(const char *name,
			     const unsigned char *data,
			     unsigned int size);

extern int (*of_element_cb) (const char *name);
extern int (*of_property_cb) (const char *name,
			      const unsigned char *data,
			      unsigned int size);

extern unsigned char *of_get_root_node(unsigned char *blob);

extern unsigned char *of_get_node(unsigned char *blob, const char *name);

extern unsigned char *of_get_property(unsigned char *blob, const char *name);



extern unsigned char *of_recurse(unsigned char *blob);

#endif /* #ifndef __OF_H__ */
