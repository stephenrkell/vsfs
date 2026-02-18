#include <stdio.h>

#include "vsfs.h"

const char *print_dirent(struct dirent *d)
{
#define DIRENT_BUF_SIZE 1024
	static char buf[DIRENT_BUF_SIZE];
	if (!d) { snprintf(buf, sizeof buf, "not found"); }
	else
	{
		snprintf(buf, sizeof buf, "present %d, inode %u, name %s",
			(int)      d->present,
			(unsigned) d->inode_num,
			(char*)    d->name);
	}
	return buf;
}

const char *print_inode(struct inode *i)
{
#define INODE_BUF_SIZE 256
	static char buf[INODE_BUF_SIZE];
	if (!i) { snprintf(buf, sizeof buf, "not found"); }
	else if (i->ftype == VSF_FREE) snprintf(buf, sizeof buf, "free inode");
	else
	{
		snprintf(buf, sizeof buf, "%s refcount %u nblocks %u size %lu",
			(i->ftype == VSF_DIR) ? "directory" :
			(i->ftype == VSF_FILE) ? "file" : "(invalid)",
			(unsigned) i->refcount,
			(unsigned) i->nblocks,
			(unsigned long) i->size);
	}
	return buf;
}

enum cb_res_t dump_one_block_as_dirents(data_block_t *block, unsigned block_idx_in_file,
	uintptr_t total_file_size)
{
	unsigned bytes_remaining_in_file = total_file_size - BLOCK_SIZE * block_idx_in_file;
	unsigned bytes_to_dump_in_this_block = (bytes_remaining_in_file < BLOCK_SIZE) ? bytes_remaining_in_file : BLOCK_SIZE;
	unsigned dirents_to_dump = bytes_to_dump_in_this_block / sizeof (struct dirent);
	struct dirent *block_dirents = (struct dirent *) block;
	unsigned starting_dirent_idx = block_idx_in_file * BLOCK_SIZE / sizeof (struct dirent);
	for (unsigned i = 0; i < dirents_to_dump; ++i)
	{
		debug_printf(0, "entry %u: %s\n",
			(unsigned) starting_dirent_idx + i,
			print_dirent(&block_dirents[i]));
	}
	return VSF_CONTINUE;
}

enum cb_res_t dump_one_block_as_raw_data(data_block_t *block, unsigned block_idx_in_file,
	uintptr_t total_file_size)
{
	unsigned bytes_remaining_in_file = total_file_size - BLOCK_SIZE * block_idx_in_file;
	unsigned bytes_to_dump_in_this_block = (bytes_remaining_in_file < BLOCK_SIZE) ? bytes_remaining_in_file : BLOCK_SIZE;
	char *block_data = (char*) block;
	for (unsigned i = 0; i < bytes_to_dump_in_this_block; ++i)
	{
		if (i != 0 && i % 16 == 0) debug_printf(0, "\n");
		else if (i != 0 && i % 8 == 0) debug_printf(0, "  ");
		debug_printf(0, " %02x", block_data[i]);
	}
	debug_printf(0, "\n");
	return VSF_CONTINUE;
}
