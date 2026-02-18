# vsfs demo

This is a simple and incomplete implementation in C of `vsfs` as found in the OSTEP book.

It can create directories but not files.

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

* it cannot create new files or directories (creating the root directory is special-cased)
* it cannot read existing files or directories
* it cannot modify existing files
* it cannot modify existing directories, except adding directory entries
* etc

It has some support for block allocation, but only using direct blocks.

As an exercise, see if you can extend it in one or more of the following ways.

* create new empty files
* append data to a file
* update existing data within a file
* use an indirect block to support extending a file beyond 10 blocks
* support filesystems longer than 64 blocks
* delete a file (tricky!)

For a more step-by-step tutorial approach, read below.

# Tutorial/suggested steps

Firstly, make sure you can build and run the filesystem by following the instructions at the top.

When you run it, try all the `dump` commands listed above on inode 0, a.k.a.~the root directory.

Study the code that creates the root directory when initializing a new filesystem. It is somewhere in `vsfs_init()`.

Next, see `append_dir_entry()`. It is doing quite a lot of work, to create links in the filesystem's directory tree by adding directory entries. Currently it is only used to add the `.` (current directory) and `..` (parent directory) entries. Note also that the root's parent directory is set, unusually, to be itself.

If you've understood these two pieces of code, you should be able to implement a call that creates an empty file and puts it under a given name in a particular directory (which for now will have to be the root directory, but could be any directory in principle). Try implementing this in `vsfs_creat()`. There is a command `creat <inode> <name>` which you can use to run this function.

If you get that far, try implementing other missing functions or some of the enhancements listed above. Note that you may want to extend the command-line interpreter (in `cmdline.c`) to test any new functions you implement.
