/* Here we implement vsfs as described in the OSTEP book,
 * as a server process backed by a file on disk.
 *
 * How will it execute?
 *
 * 0. For testing, via a command-line interface
 *
 * 1. via a fuse wrapper, for the students to run
 * on an emulated Linux machine.
 *
 * --
 *
 * 2. The students could wrap it themselves in an NFS server
 * and then mount it from their QEMU Linux machine.
 *
 * 3. Some extensions:
 *     -- replication: NFS server now talks to two or more
 *        vsfs backends, using a quorum approach
 *
 * The initial version that we supply to the students is
 * missing some implementation: it can only create empty files,
 * and cannot delete files?
 */

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "vsfs.h"
#include "bitmap.h"

unsigned debug_level;
FILE *debug_out;

static void *mapping;
static size_t mapping_size;
static long page_size;
static struct superblock *super;
static bitmap_word_t *inode_bitmap;
static bitmap_word_t *inode_bitmap_end;
static bitmap_word_t *data_bitmap;
static bitmap_word_t *data_bitmap_end;
static struct inode *inodes;
static struct inode *inodes_end;
static data_block_t *data_blocks;
static data_block_t *data_blocks_end;

/* internal operations */
static struct inode *inode_alloc(void);
static void *data_alloc(void);
static void inode_free(struct inode *i);
static void data_free(void *pos);
static struct dirent *append_dir_entry(struct inode *dir, struct inode *tgt, const char *name);
static _Bool ensure_allocated_length(struct inode *i, unsigned len);

void vsfs_init(const char *backing_file_name, size_t expected_size)
{
	page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size == -1) err(EXIT_FAILURE, "getting page size");

	FILE *f = fopen(backing_file_name, "r+");
	if (!f) err(EXIT_FAILURE, "opening backing file `%s'", backing_file_name);

	/* Does it have the expected size? */
	struct stat statbuf;
	int ret = fstat(fileno(f), &statbuf);
	if (ret != 0) err(EXIT_FAILURE, "getting status of backing file `%s'", backing_file_name);
	if (statbuf.st_size != expected_size)
	{
		errx(EXIT_FAILURE, "backing file `%s' does not have size %ld bytes; try using `truncate -s'?",
			backing_file_name, (long) expected_size);
	}

	mapping_size = ROUND_UP_TO(page_size, expected_size);
	mapping = mmap(NULL, mapping_size, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(f), 0);
	if (mapping == MAP_FAILED) err(EXIT_FAILURE, "mapping backing file `%s'", backing_file_name);

	struct superblock expected_super = {
		.magic = "VSFS",
		.fs_size_in_bytes = expected_size,
		.block_size_in_bytes = BLOCK_SIZE,
		/* We have five blocks' worth of inodes */
		.num_inodes = (5 * BLOCK_SIZE) / sizeof (struct inode),
		/* We have exactly two bitmap blocks and one superblock, so the total
		 * non-data blocks is 8 a.k.a. START_BLOCKS_RESERVED. */
		.num_data_blocks = (expected_size / BLOCK_SIZE) - START_BLOCKS_RESERVED
	};
	super = mapping;
	inode_bitmap = (void*)((char*)mapping + BLOCK_SIZE);
	inode_bitmap_end = (void*)((char*)mapping + 2*BLOCK_SIZE);
	data_bitmap = inode_bitmap_end;
	data_bitmap_end = (void*)((char*)mapping + 3*BLOCK_SIZE);
	inodes = (void*)data_bitmap_end;
	inodes_end = (void*)((char*)mapping + START_BLOCKS_RESERVED*BLOCK_SIZE);
	data_blocks = (void*) inodes_end;
	data_blocks_end = data_blocks + super->num_data_blocks;
	if (statbuf.st_blocks == 0)
	{
		debug_printf(0, "detected a zeroed sparse backing file; initializing a fresh vsfs\n");
		*super = expected_super;
		/* Manually create the root directory also, using inode 0 and data block 0 */
		struct inode *root = inode_alloc();
		root->ftype = VSF_DIR;
		data_block_t *d = data_alloc();
		inodes[0] = (struct inode) {
			.ftype = VSF_DIR,
			.size = sizeof (struct dirent), /* a single null entry */
			.direct[0] = d - data_blocks, /* NB blocks are numbered from start of data blocks */
			.nblocks = 1
		};
		assert(inodes[0].refcount == 0);
		struct dirent *d1 = append_dir_entry(&inodes[0], &inodes[0], ".");
		assert(d1);
		debug_printf(0, "created '.' directory entry\n");
		assert(inodes[0].refcount == 1);
		struct dirent *d2 = append_dir_entry(&inodes[0], &inodes[0], "..");
		assert(d2);
		assert(inodes[0].refcount == 2);
		debug_printf(0, "created '..' directory entry\n");
	}
	else if (0 == memcmp(super, &expected_super, sizeof *super))
	{
		debug_printf(1, "superblock matched OK\n");
	}
	else errx(EXIT_FAILURE, "superblock check failed");

	debug_printf(1, "opened the vsfs successfully \n");
	fclose(f);
}

__attribute__((destructor))
static void vsfs_deinit(void)
{
	if (mapping) munmap(mapping, mapping_size);
}

static struct inode *inode_alloc(void)
{
	unsigned idx = bitmap_find_first_clear(inode_bitmap, inode_bitmap_end, NULL);
	if (idx == -1 || idx >= super->num_inodes) return NULL;
	bitmap_set(inode_bitmap, idx);
	return &inodes[idx];
}
static void *data_alloc(void)
{
	unsigned idx = bitmap_find_first_clear(data_bitmap, data_bitmap_end, NULL);
	if (idx == -1 && idx >= super->num_data_blocks) return NULL;
	bitmap_set(data_bitmap, idx);
	return &data_blocks[idx];
}
static void inode_free(struct inode *i)
{
	bitmap_clear(inode_bitmap, i - inodes);
}
static void data_free(void *pos)
{
	bitmap_clear(data_bitmap, (data_block_t *) pos - data_blocks);
}
static data_block_t *get_data_block(struct inode *i, unsigned byte_offset)
{
	if (byte_offset >= i->size) return NULL;
	if (byte_offset >= NDIRECT * BLOCK_SIZE) return NULL;
	return &data_blocks[i->direct[byte_offset / BLOCK_SIZE]];
	/* TODO: support indirect blocks etc */
}
/* Grow the file's block allocation s.t. it can hold at least 'len' bytes.
 * NOTE: does not update the file's 'size' field! Do this after writing the
 * data to any newly allocated space. */
static _Bool ensure_allocated_length(struct inode *i, unsigned len)
{
	if (BLOCK_SIZE * i->nblocks >= len) return 1;
	/* TODO: support growing */
	/* XXX: must zero any fresh length allocated */
	return 0;
}
static struct dirent *append_dir_entry(struct inode *dir, struct inode *tgt, const char *string)
{
	unsigned initial_nentries_incl_terminator = dir->size / sizeof (struct dirent);
	_Bool success = ensure_allocated_length(dir,
		1 + initial_nentries_incl_terminator * sizeof (struct dirent));
	if (!success) return NULL;
	/* The last entry should be a null entry. */
	data_block_t *data_block = get_data_block(dir, ROUND_DOWN_TO(BLOCK_SIZE,
		(initial_nentries_incl_terminator - 1) * sizeof (struct dirent)));
	char *data = (char*) data_block;
	unsigned byte_offset = ((initial_nentries_incl_terminator  - 1)
		* sizeof (struct dirent)) % BLOCK_SIZE;
	assert(byte_offset + sizeof (struct dirent) <= BLOCK_SIZE);
	struct dirent null_dirent;
	bzero(&null_dirent, sizeof null_dirent);
	assert(0 == memcmp(data + byte_offset, &null_dirent, sizeof null_dirent));
	/* Write our directory entry into the null entry; we know that the
	 * remaining allocated length of the file was zeroed when we grew it, so
	 * we have a terminator following us already. */
	struct dirent *d = (struct dirent *)(data + byte_offset);
	*d = (struct dirent) {
		.present = 1,
		.inode_num = tgt - inodes
	};
	++tgt->refcount;
	dir->size += sizeof (struct dirent);
	strncpy(d->name, string, MAX_NAME_LEN);
	return d;
}
/* public functions */

struct inode *vsfs_creat(struct inode *dir, const char *name)
{
	/* Create an empty regular file */
}
struct dirent *vsfs_link(struct inode *dir, struct inode *tgt, const char *name)
{
	return append_dir_entry(dir, tgt, name);
}
struct dirent *vsfs_unlink(struct dirent *ent)
{
	
}
struct inode *vsfs_lookup(struct inode *dir, const char *pathname)
{
	
}
static struct dirent *find_dirent_by_name(struct dirent *first, struct dirent *limit,
	const char *name)
{
	
}

void vsfs_dumpfs(void)
{
	struct superblock *super = mapping;
	debug_printf(0, "--- begin vsfs dump\n");
	debug_printf(0, "superblock:\n");
	debug_printf(0, "   magic:   %c%c%c%c\n", super->magic[0], super->magic[1], super->magic[2], super->magic[3]);
	debug_printf(0, "   fs size: %u\n", (unsigned) super->fs_size_in_bytes);
	debug_printf(0, "   block size: %u\n", (unsigned) super->block_size_in_bytes);
	debug_printf(0, "   num inodes: %u\n", (unsigned) super->num_inodes);
	debug_printf(0, "   num data blocks: %u\n", (unsigned) super->num_data_blocks);
	debug_printf(0, "   root dir inode: (always 0)\n");
	debug_printf(0, "\ninode numbers in use: [");
	_Bool printed = 0;
#define print_it(idx) do { debug_printf(0, "%s%d", printed ? ", " : "", (int) idx); printed = 1; } while(0)
	BITMAP_FOR_EACH_BIT_SET(inode_bitmap, inode_bitmap_end, print_it);
	debug_printf(0, "]\n");

	debug_printf(0, "\ndata blocks ('X' denotes a block in use):\n");
	debug_printf(0, "                        ");
	for (unsigned i = 0; i < 32-START_BLOCKS_RESERVED; ++i)
	{
		debug_printf(0, "[%c]", bitmap_get(data_bitmap, i) ? 'x' : ' ');
	}
	debug_printf(0, "\n");
	for (unsigned i = 32-START_BLOCKS_RESERVED; i < TOTAL_BLOCKS - START_BLOCKS_RESERVED; ++i)
	{
		debug_printf(0, "[%c]", bitmap_get(data_bitmap, i) ? 'x' : ' ');
	}
	debug_printf(0, "\n\n");

	debug_printf(0, "--- end vsfs dump\n");
}

void vsfs_dumpi(unsigned idx)
{
	struct inode *inode = &inodes[idx];

	debug_printf(0, "inode %u:\n", idx);
	debug_printf(0, "   type: %s\n", (inode->ftype == VSF_FREE) ? "free" :
	                                 (inode->ftype == VSF_FILE) ? "regular file" :
	                                 (inode->ftype == VSF_DIR) ? "directory" : "(invalid)");
	debug_printf(0, "   links: %d\n", (int) inode->refcount);
	debug_printf(0, "   size in bytes: %u\n", (unsigned) inode->size);
	debug_printf(0, "   # blocks allocated: %u\n", (unsigned) inode->nblocks);
	for (unsigned i = 0; i < NDIRECT; ++i)
	{
		debug_printf(0, "   direct block %d: %u\n", (int) i, (unsigned) inode->direct[i]);
	}
}
