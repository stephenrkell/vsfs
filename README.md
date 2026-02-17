# vsfs demo

This is a simple and incomplete implementation in C of `vsfs` as found in the OSTEP book.

It can create directories but no files.

To build it, run `make`.

To run it, first create an empty sparse file of 256 kB (262144 bytes), then pass the file name as the first parameter.

```
truncate -s 262144 test.img
./vsfs test.img
```

You should see an initialization message.

```
detected a zeroed sparse backing file; initializing a fresh vsfs
created '.' directory entry
created '..' directory entry
opened the vsfs successfully 
```

From here, you can interact with the filesystem by typing commands.

`dumpfs` will print out the filesystem state in a similar form as in the slides and book.

```
--- begin vsfs dump
superblock:
   magic:   VSFS
   fs size: 262144
   block size: 4096
   inode table size: 568
   num data blocks: 56
   root dir inode: (always 0)

inode numbers in use: [0]

data blocks ('X' denotes a block in use):
                        [x][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]
[ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]

--- end vsfs dump
```

You can also use `dumpi n` to dump a inode number `n`,
`dumpdir n` to dump as a directory the contents of the file with inode number `n`.
`dump n` to dump as a raw bytes the contents of the file with inode number `n`.

FIXME: support more commands
FIXME: add a fuse layer

Currently this filesystem has a number of limitations.

* it cannot create new files or directories
* it cannot read existing files or directories
* it cannot modify existing files
* it cannot modify existing directories, except adding directory entries
* etc

It has some support for block allocation, but only using direct blocks.

See if you can extend it in one or more of the following ways.

* support filesystems of arbitrary length, subject to 262144 byte minimum
* create new empty files
* append data to a file
* use an indirect block to support extending a file beyond 10 blocks
* delete a file (tricky!)

FIXME: make it do a few more things out of the box, but not everything

FIXME: if we are to do the interop exercise, it should not be necessary to add fields to the inode, dirent or superblock.
