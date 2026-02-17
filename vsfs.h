#ifndef VSFS_H_
#define VSFS_H_

#include <stdint.h>
#include <stdio.h>

extern unsigned debug_level;
extern FILE *debug_out;
#define debug_printf(lvl, args...) \
   (((lvl) <= debug_level) ? fprintf(debug_out, args) : 0)

#define BLOCK_SIZE 4096
typedef char data_block_t[BLOCK_SIZE];

/* XXX: we do memcmp on this struct, so it should not contain any padding. */
struct superblock
{
	char magic[4];
	uint32_t fs_size_in_bytes;
	uint32_t block_size_in_bytes;
	uint32_t num_inodes;
	uint32_t num_data_blocks;
};
/* We reserve 8 initial blocks: one for superblock, two for bitmaps, 5 for inode table */
#define START_BLOCKS_RESERVED 8
#define TOTAL_BLOCKS 64

#define ROUND_UP_TO(mult, quant) \
	( ((quant) % (mult) == 0) ? (quant) : (mult)*(1+((quant)/(mult))) )
#define ROUND_DOWN_TO(mult, quant) \
	( (mult)*((quant)/(mult)) )

#define NDIRECT 10  /* 10 directly addressed blocks */
struct inode
{
	enum ftype_t { VSF_FREE, VSF_FILE, VSF_DIR } ftype;
	unsigned short refcount;
	unsigned       size; /* size in bytes of file/dir */
	unsigned       nblocks; /* number of blocks allocated to this file */
	unsigned short direct[NDIRECT];
};

#define MAX_NAME_LEN 254
struct dirent
{
	uint16_t present:1;
	uint16_t inode_num:15; /* XXX: implies we can have max 32768 inodes */
	char name[MAX_NAME_LEN];
};

/* utility code: open or create a vsfs */
void vsfs_init(const char *backing_file_name, size_t expected_size);

/* External operations. Each of these may be lightly glued into
 * the command-line front-end and the fuse front-end. */
#ifndef CMDLINE_FMT
#define CMDLINE_FMT(ident, argstr) \
	extern const char ident ## _cmdline[];
#endif

struct inode *vsfs_creat(struct inode *dir, const char *name); CMDLINE_FMT(creat, "%u %d %s");
struct dirent *vsfs_link(struct inode *dir, struct inode *tgt, const char *name); CMDLINE_FMT(link, "%u %u %s");
struct dirent *vsfs_unlink(struct dirent *ent); CMDLINE_FMT(unlink, "%u");
struct inode *vsfs_lookup(struct inode *dir, const char *pathname); CMDLINE_FMT(lookup, "%u");
void vsfs_dumpfs(void); CMDLINE_FMT(dumpfs, "");

#endif
