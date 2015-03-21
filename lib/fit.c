/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Gaël PORTAY <gael.portay@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "common.h"
#include "fit.h"
#include "string.h"
#ifdef CONFIG_CRC32
#include "crc32.h"
#endif
#include "debug.h"
#include "of.h"

enum image_type_t
{
	IMAGE_TYPE_KERNEL,
	IMAGE_TYPE_FDT,
	IMAGE_TYPE_RAMDISK,
	IMAGE_TYPE_COUNT,
};

static const char *conf_default = NULL;
static enum image_type_t image_type = IMAGE_TYPE_COUNT;

static unsigned int load;
static unsigned int entry;

struct images_t
{
	const char *configuration;
	const unsigned char *data;
#ifdef CONFIG_FIT_CRC32
	const unsigned char *crc32;
#endif
};

static struct images_t images[IMAGE_TYPE_COUNT];

#ifdef CONFIG_FIT_CRC32
int hash_property(const char *name, const unsigned char *data,
		  unsigned int size)
{
	if ((strcmp(name, "algo") == 0) &&
	    (strcmp(data, "crc32") == 0)) {
		dbg_info("FIT: Image %s %s hash is %x\n",
			 images[image_type], name, data);
		images.data[image_type] = data;
		return 0;
	}

	return 0;
}
#endif

int images_property(const char *name, const unsigned char *data,
		    unsigned int size)
{
	if (image_type == IMAGE_TYPE_COUNT)
		return 0;

	if (strcmp(name, "data") == 0) {
		dbg_info("FIT: Image %s is at @%x\n",
			 images[image_type].configuration, data);
		images[image_type].data = data;
		return 0;
	}
	else if (strcmp(name, "description") == 0) {
		dbg_info("FIT: Image %s is \"%s\"\n",
			 images[image_type].configuration, data);
		return 0;
	}

	if ((image_type == IMAGE_TYPE_KERNEL) ||
	    (image_type == IMAGE_TYPE_RAMDISK)) {
		unsigned int *u32 = (unsigned int *) data;
		if (strcmp(name, "load") == 0) {
			load = swap_uint32(*u32);
			dbg_info("FIT: Image %s loads at @%x\n",
				 images[image_type].configuration, load);
			return 0;
		}
		else if (strcmp(name, "entry") == 0) {
			entry = swap_uint32(*u32);
			dbg_info("FIT: Image %s entry is at @%x\n",
				 images[image_type].configuration, entry);
			return 0;
		}
	}

	return 0;
}

int configuration_property(const char *name, const unsigned char *data,
			   unsigned int size)
{
	enum image_type_t type = IMAGE_TYPE_COUNT;

	if (strcmp(name, "kernel") == 0)
		type = IMAGE_TYPE_KERNEL;
#ifdef CONFIG_OF_FDT
	else if (strcmp(name, "fdt") == 0)
		type = IMAGE_TYPE_FDT;
#endif
	else if (strcmp(name, "ramdisk") == 0)
		type = IMAGE_TYPE_RAMDISK;
	else
		return 0;

	images[type].configuration = (const char *) data;
	dbg_info("FIT: Using %s %s.\n", name, images[type].configuration);

	return 0;
}

int configurations_property(const char *name, const unsigned char *data,
			    unsigned int size)
{
	if (strcmp(name, "default") == 0) {
		conf_default = (const char *) data;
		dbg_info("FIT: Using configuration %s.\n", conf_default);
	}

	return 0;
}

int element(const char *name)
{
	int i;
	static of_property_f stack[3];
	static int stack_index;

	if (!name) {
		of_property_cb = stack[--stack_index];
		return 0;
	}

	if (conf_default && strcmp(name, conf_default) == 0) {
		dbg_info("FIT: Found default configuration...\n");
		of_property_cb = configuration_property;
		stack[stack_index++] = of_property_cb;
		return 0;
	}

	for (i = 0; i < IMAGE_TYPE_COUNT; i++) {
		if (images[i].configuration &&
		    strcmp(name, images[i].configuration) == 0) {
			dbg_info("FIT: Found image %s...\n", name);
			image_type = i;
			of_property_cb = images_property;
			stack[stack_index++] = of_property_cb;
			return 0;
		}
	}

	return 0;
}

int fit_loadimage(unsigned char *blob, struct image_info *image)
{
#ifdef CONFIG_ITB_CONFIGURATION
	default_conf = ITB_CONFIGURATION;
#endif /* #ifdef CONFIG_ITB_CONFIGURATION */

	dbg_info("FIT: Magic number %x\n", of_get_magic_number(blob));
	dbg_info("FIT: Version: %u\n", of_get_format_version(blob));

	of_element_cb = element;
	of_property_cb = configurations_property;

	if (!of_recurse(blob)) {
		dbg_info("FIT: Invalid image tree blob!\n");
		return -1;
	}

	dbg_info("kernel:  %s %x (load: %x, entry: %x)\n",
		 images[IMAGE_TYPE_KERNEL].configuration,
		 images[IMAGE_TYPE_KERNEL].data,
		 load, entry);
#ifdef CONFIG_OF_FDT
	dbg_info("fdt:     %s %x\n",
		 images[IMAGE_TYPE_FDT].configuration,
		 images[IMAGE_TYPE_FDT].data);
#endif
	dbg_info("ramdisk: %s %x\n",
		 images[IMAGE_TYPE_RAMDISK].configuration,
		 images[IMAGE_TYPE_RAMDISK].data);

	if (!images[IMAGE_TYPE_KERNEL].data) {
		dbg_info("FIT: No kernel image found!\n");
		return -1;
	}

	image->dest = (unsigned char *) images[IMAGE_TYPE_KERNEL].data;
	dbg_hexdump((unsigned int) images[IMAGE_TYPE_KERNEL].data,
		    (unsigned char *) images[IMAGE_TYPE_KERNEL].data, 16);

#ifdef CONFIG_OF_FDT
	if (images[IMAGE_TYPE_FDT].data) {
		image->fdt_dest =
				(unsigned char *) images[IMAGE_TYPE_FDT].data;
		dbg_hexdump((unsigned long int) images[IMAGE_TYPE_FDT].data,
			    images[IMAGE_TYPE_FDT].data, 16);
	}
#endif

	return 0;
}
