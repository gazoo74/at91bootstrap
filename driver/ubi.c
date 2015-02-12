/*
 * Copyright (c) 2014, Gaël PORTAY <gael.portay@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "common.h"
#include "ubi.h"
#ifdef CONFIG_UBI_CRC
#include "crc32.h"
#endif
#include "div.h"
#include "debug.h"
#include "nandflash.h"

#include <string.h>

static int ubi_read_peb(struct ubi_device *ubi,
		unsigned int pnum,
		unsigned int lnum,
		unsigned int *len,
		unsigned char *dest) {
	unsigned int l = 0, block = pnum, page = 0, size, lebsize, s, offset;
	unsigned char *data, *d = dest;
	struct ubi_ec_hdr *ec_hdr;
	struct ubi_vid_hdr *vid_hdr;
#ifdef CONFIG_UBI_CRC
	unsigned int hdr_crc, crc;
#endif

	if (nand_check_badblock(ubi->nand, block, ubi->buf)) {
		dbg_info("UBI: Bad block %u!\n", block);
		return -1;
	}

	if (nand_read_page(ubi->nand, block, page, 0, ubi->buf)) {
		dbg_info("UBI: Failed to read page %u in block %u!\n", page, block);
		return -1;
	}

	ec_hdr = (struct ubi_ec_hdr *) ubi->buf;
	if (swap_uint32(ec_hdr->magic) != UBI_EC_HDR_MAGIC) {
		if (swap_uint32(ec_hdr->magic) != 0XFFFFFFFF)
			dbg_loud("UBI: Mismatch UBI Erase-Counter Header magic at PEB %u! (%x != %x)\n",
				block, swap_uint32(ec_hdr->magic), UBI_EC_HDR_MAGIC);
		return -1;
	}

#ifdef CONFIG_UBI_CRC
	crc = crc32(UBI_CRC32_INIT, (unsigned char const *) ec_hdr, UBI_EC_HDR_SIZE_CRC);
	hdr_crc = swap_uint32(ec_hdr->hdr_crc);

	if (hdr_crc != crc) {
		dbg_info("UBI: Bad Erase-Counter Header CRC at PEB %u! (%x != %x)\n", block, hdr_crc, crc);
		return -1;
	}
#endif

	if (swap_uint32(ec_hdr->vid_hdr_offset) >= ubi->nand->pagesize) {
		unsigned int p, o;
		unsigned char *b;

		division(swap_uint32(ec_hdr->vid_hdr_offset), ubi->nand->pagesize, &p, &o);
		b = (unsigned char *) (ubi->buf + (p * ubi->nand->pagesize));
		dbg_loud("UBI: Reading page %u from block %u for Volume-ID Header at offset %x!\n",
			p, block, o);
		if (nand_read_page(ubi->nand, block, p, 0, b)) {
			dbg_loud("UBI: Failed to read page %u in block %u!\n", p, block);
			return -1;
		}

		vid_hdr = (struct ubi_vid_hdr *) b;
	}
	else {
		vid_hdr = (struct ubi_vid_hdr *) (ubi->buf + swap_uint32(ec_hdr->vid_hdr_offset));
	}

	if (swap_uint32(vid_hdr->magic) != UBI_VID_HDR_MAGIC) {
		if (swap_uint32(ec_hdr->magic) != -1)
			dbg_info("UBI: Mismatch UBI Volume-ID Header magic at PEB %u! (%x != %x)\n",
				block, swap_uint32(vid_hdr->magic), UBI_VID_HDR_MAGIC);
		return -1;
	}

#ifdef CONFIG_UBI_CRC
	crc = crc32(UBI_CRC32_INIT, (unsigned char const *) vid_hdr, UBI_VID_HDR_SIZE_CRC);
	hdr_crc = swap_uint32(vid_hdr->hdr_crc);

	if (hdr_crc != crc) {
		dbg_info("UBI: Bad Volume-ID Header CRC at PEB %u! (%x != %x)\n", block, hdr_crc, crc);
		return -1;
	}
#endif

	lebsize = ubi->nand->blocksize - swap_uint32(ec_hdr->data_offset);
	offset = lnum * lebsize;
	if (*len < offset) {
		*len = 0;
		return 0;
	}

	*len -= offset;
	d += offset;

	size = swap_uint32(vid_hdr->data_size);
	if (vid_hdr->vol_type != UBI_VID_STATIC) {
		size = lebsize;
	}
	size = MIN(size, *len);

	if (swap_uint32(ec_hdr->data_offset) >= ubi->nand->pagesize) {
		unsigned int p, o;
		unsigned char *b;

		division(swap_uint32(ec_hdr->data_offset), ubi->nand->pagesize, &p, &o);
		b = (unsigned char *) (ubi->buf + (p * ubi->nand->pagesize));
		dbg_very_loud("UBI: Reading page %u from block %u for Erase-Counter Data at offset %x!\n",
			p, block, o);
		if (nand_read_page(ubi->nand, block, p, 0, b)) {
			dbg_loud("UBI: Failed to read page %u in block %u\n", p, block);
			return -1;
		}
		page = p;

		data = b;
	}
	else {
		data = (unsigned char *) (ubi->buf + swap_uint32(ec_hdr->data_offset));
		page++;

		s = MIN(ubi->nand->pagesize - swap_uint32(ec_hdr->data_offset), size);
		dbg_very_loud("UBI: Writing @%x-@%x.\n", d - dest, (d - dest) + s - 1);
		memcpy(d, data, s);
		d += s;
		size -= s;
		l += s;
	}

	while ((size > 0) && (page < ubi->nand->pages_block)) {
		unsigned char *b = ubi->buf + 2 * ubi->nand->pagesize;
		s = MIN(ubi->nand->pagesize, size);
		if (nand_read_page(ubi->nand, block, page, 0, b)) {
			dbg_info("UBI: Failed to read page %u in block %u!\n", page, block);
			return -1;
		}
		page++;

		dbg_very_loud("UBI: Writing @%x-@%x.\n", (d - dest), (d - dest) + s - 1);
		memcpy(d, b, s);
		d += s;
		size -= s;
		l += s;
	}

#ifdef CONFIG_UBI_CRC
	crc = crc32(UBI_CRC32_INIT, dest, swap_uint32(vid_hdr->data_size));
	hdr_crc = swap_uint32(vid_hdr->data_crc);

	if (hdr_crc != crc) {
		dbg_info("UBI: Bad Volume-ID Data CRC at PEB %u! (%x != %x)\n", block, hdr_crc, crc);
		return -1;
	}
#endif

	*len = l;
	return *len ? 0 : -1;
}

int ubi_init(struct ubi_device *ubi, struct nand_info *nand) {
	unsigned int pnum, block, addr = UBI_ADDRESS;
	unsigned char *data;

	ubi->nand = nand;
	ubi->firstblock = div(UBI_OFFSET, nand->blocksize);
	ubi->numpebs = nand->numblocks;
	ubi->pebs = (struct ubi_peb *) addr;
	memset(ubi->pebs, 0xFF, ubi->numpebs * sizeof(struct ubi_peb));
	addr += (ubi->numpebs * sizeof(struct ubi_peb));
	memset(ubi->vols, 0x00, sizeof(struct ubi_peb *) * UBI_VOL_NAME_MAX);
	ubi->vol_table = (struct ubi_vtbl_record *) addr;
	addr += (sizeof(struct ubi_vtbl_record) * UBI_MAX_VOLUMES);
	ubi->buf = (unsigned char *) addr;
	dbg_loud("UBI: Blocks from %u-%u (%u)\n", ubi->firstblock, ubi->numpebs, nand->numblocks);

	for (block = ubi->firstblock; block < ubi->numpebs; block++) {
		unsigned int page = 0;
		struct ubi_ec_hdr *ec_hdr;
		struct ubi_vid_hdr *vid_hdr;
#ifdef CONFIG_UBI_CRC
		unsigned int hdr_crc, crc;
#endif

		if (nand_check_badblock(nand, block, ubi->buf)) {
			dbg_info("UBI: Bad block %u!\n", block);
			continue;
		}

		if (nand_read_page(nand, block, page, 0, ubi->buf)) {
			dbg_info("UBI: Failed to read page %u in block %u!\n", page, block);
			continue;
		}
		page++;

		ec_hdr = (struct ubi_ec_hdr *) ubi->buf;
		if (swap_uint32(ec_hdr->magic) != UBI_EC_HDR_MAGIC) {
			if (swap_uint32(ec_hdr->magic) != 0XFFFFFFFF)
				dbg_loud("UBI: Mismatch UBI Erase-Counter Header magic at PEB %u! (%x != %x)\n",
					block, swap_uint32(ec_hdr->magic), UBI_EC_HDR_MAGIC);
			continue;
		}

#ifdef CONFIG_UBI_CRC
		crc = crc32(UBI_CRC32_INIT, (unsigned char const *) ec_hdr, UBI_EC_HDR_SIZE_CRC);
		hdr_crc = swap_uint32(ec_hdr->hdr_crc);

		if (hdr_crc != crc) {
			dbg_info("UBI: Bad Erase-Counter Header CRC at PEB %u! (%x != %x)\n", block, hdr_crc, crc);
			continue;
		}
#endif

		if (swap_uint32(ec_hdr->vid_hdr_offset) >= nand->pagesize) {
			unsigned int p, o;
			unsigned char *b;

			division(swap_uint32(ec_hdr->vid_hdr_offset), nand->pagesize, &p, &o);
			b = (unsigned char *) (ubi->buf + (p * nand->pagesize));
			dbg_very_loud("UBI: Reading page %u from block %u for Volume-ID Header at offset %x!\n",
				p, block, o);
			if (nand_read_page(nand, block, p, 0, b)) {
				dbg_info("UBI: Failed to read page %u in block %u!\n", p, block);
				return -1;
			}

			vid_hdr = (struct ubi_vid_hdr *) b;
		}
		else {
			vid_hdr = (struct ubi_vid_hdr *) (ubi->buf + swap_uint32(ec_hdr->vid_hdr_offset));
		}

		if ((swap_uint32(vid_hdr->magic) == 0xFFFFFFFF) || (swap_uint32(vid_hdr->magic) == 0x00000000)) {
			continue;
		}
		else if (swap_uint32(vid_hdr->magic) != UBI_VID_HDR_MAGIC) {
			dbg_info("UBI: Mismatch UBI Volume-ID Header magic at PEB %u! (%x != %x)\n",
				block, swap_uint32(vid_hdr->magic), UBI_VID_HDR_MAGIC);
			continue;
		}

#ifdef CONFIG_UBI_CRC
		crc = crc32(UBI_CRC32_INIT, (unsigned char const *) vid_hdr, UBI_VID_HDR_SIZE_CRC);
		hdr_crc = swap_uint32(vid_hdr->hdr_crc);

		if (hdr_crc != crc) {
			dbg_info("UBI: Bad Volume-ID Header CRC at PEB %u! (%x != %x)\n", block, hdr_crc, crc);
			continue;
		}
#endif

		ubi->pebs[block].vol_id = swap_uint32(vid_hdr->vol_id);
		ubi->pebs[block].lnum = swap_uint32(vid_hdr->lnum);
		ubi->pebs[block].data_crc = swap_uint32(vid_hdr->data_crc);
		ubi->pebs[block].sqnum = swap_uint64(vid_hdr->sqnum);
		ubi->pebs[block].copy_flag = swap_uint32(vid_hdr->copy_flag);
		ubi->pebs[block].next = NULL;
		ubi->pebs[block].prev = NULL;

		if ((ubi->pebs[block].vol_id < UBI_LAYOUT_VOLUME_ID) && (ubi->pebs[block].lnum == 0)) {
			dbg_loud("UBI: First LEB for volume-id %u is PEB %x!\n", ubi->pebs[block].vol_id, block);
			ubi->vols[ubi->pebs[block].vol_id] = &ubi->pebs[block];
		}
	}

	data = (unsigned char *) ubi->vol_table;
	for (block = ubi->firstblock; block < ubi->numpebs; block++) {
		unsigned int size = ubi->nand->pagesize;

		if (ubi->pebs[block].vol_id == 0xFFFFFFFF) {
			continue;
		}

		for (pnum = block + 1; pnum < ubi->numpebs; pnum++) {
			if ((ubi->pebs[block].vol_id > UBI_LAYOUT_VOLUME_ID) || (ubi->pebs[pnum].vol_id != ubi->pebs[block].vol_id)) {
				continue;
			}

			if (ubi->pebs[block].lnum == (ubi->pebs[pnum].lnum + 1)) {
				ubi->pebs[block].prev = &ubi->pebs[pnum];
				ubi->pebs[pnum].next = &ubi->pebs[block];
				continue;
			}
			else if (ubi->pebs[block].lnum == (ubi->pebs[pnum].lnum - 1)) {
				ubi->pebs[pnum].prev = &ubi->pebs[block];
				ubi->pebs[block].next = &ubi->pebs[pnum];
				continue;
			}
			else if (ubi->pebs[pnum].lnum != ubi->pebs[block].lnum) {
				continue;
			}

			if (ubi->pebs[pnum].sqnum > ubi->pebs[block].sqnum) {
				if (!ubi->pebs[pnum].copy_flag) {
					ubi->pebs[block].copy_flag = 1;
					ubi->pebs[block].vol_id = 0xFFFFFFFF;
					dbg_loud("UBI: New PEB is %u! Old was %u.\n", pnum, block);
					continue;
				}
			}
			else {
				if (!ubi->pebs[pnum].copy_flag) {
					ubi->pebs[pnum].copy_flag = 1;
					ubi->pebs[pnum].vol_id = 0xFFFFFFFF;
					dbg_loud("UBI: New PEB is %u! Old was %u.\n", block, pnum);
					continue;
				}
			}
		}

		if (ubi->pebs[block].vol_id != UBI_LAYOUT_VOLUME_ID) {
			continue;
		}

		if (ubi->pebs[block].copy_flag) {
			dbg_info("UBI: Skipping LEB %u due to copy flag!\n", block);
			continue;
		}

		dbg_loud("UBI: Reading LEB %u of Volume-ID %x to @%x!\n", block, UBI_LAYOUT_VOLUME_ID, data);
		size = sizeof(struct ubi_vtbl_record) * UBI_MAX_VOLUMES;
		if (ubi_read_peb(ubi, block, ubi->pebs[block].lnum, &size, data)) {
			dbg_info("UBI: Failed to read PEB %u!\n", block);
			continue;
		}
	}

	return 0;
}

const char *ubi_getvolname(struct ubi_device *ubi, unsigned int id) {
	if (id < UBI_INTERNAL_VOL_START) {
		return (const char *) ubi->vol_table[id].name;
	}
	else if (id == UBI_LAYOUT_VOLUME_ID) {
		return UBI_LAYOUT_VOLUME_NAME;
	}

	return NULL;
}

unsigned int ubi_searchvolume(struct ubi_device *ubi, const char *volname) {
	unsigned int i;

	for (i = 0; i < UBI_MAX_VOLUMES; i++) {
		if (!ubi->vol_table[i].name_len || !*ubi->vol_table[i].name) {
			continue;
		}

		if (strncmp(volname, (const char *) ubi->vol_table[i].name, ubi->vol_table[i].name_len) == 0) {
			return i;
		}
	}

	return -1;
}

int ubi_loadimage(struct ubi_device *ubi, unsigned int id, unsigned int *len, unsigned char *dest) {
	struct ubi_peb *a = ubi->vols[id];
	unsigned int length = 0, pnum = (unsigned int) (a - ubi->pebs);

	for (pnum = 0; pnum < ubi->numpebs; pnum++) {
		if (ubi->pebs[pnum].vol_id == id && ubi->pebs[pnum].lnum == 0) {
			break;
		}
	}

	a = &ubi->pebs[pnum];
	dbg_loud("UBI: First LEB for volume-id %u is PEB %x! @%x\n", id, pnum, a);
	while (a) {
		int ret;
		unsigned int l = *len;
		pnum = (unsigned int) (a - ubi->pebs);

		dbg_loud("UBI: Reading LEB %x at PEB %x for Volume-ID %u...\n", a->lnum, pnum, id);
		ret = ubi_read_peb(ubi, pnum, a->lnum, &l, dest);
		if (ret) {
			dbg_info("Warning: UBI: Failed to read PEB %u!\n", pnum);
		}

		a = a->next;
		length += l;
	}

	*len = length;
	return 0;
}
