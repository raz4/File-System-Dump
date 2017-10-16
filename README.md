# EXT2 File System Dump

Objectives:

	1. Reinforce the basic file system concepts of directory objects, file objects,
	and free space
  
	2. Gain experience with the examining, interpreting, and processing information
	in binary data structures
  
	3. Gain practical experience with complex data structures in general, and on-disk
	data formats in particular

prog.c - C source module that analyzes ext2 file system and outputs six csv files (super.csv, group.csv, bitmap.csv, inode.csv, directory.csv, indirect.csv)

README - this file

super.csv - contains the magic number, total number of inodes, total number of blocks,
block size, fragment size, blocks per group, inodes per group, fragments per group,
first data block

group.csv - contains the number of contained blocks, number of free blocks, number of free
inodes, number of directories, (free) inode bitmap block, block bitmap black, inode table
(start) block

bitmap.csv - contains the block number of the map, free block/inode number

inode.csv - contains the inode number, file type, mode, owner, group, link count, creation
time, modification time, access time, access time, file size, number of blocks, block
pointers * 15

directory.csv - contains the parent inode number, entry number, entry length, name length,
inode number of the file entry, name

indirect.csv - contains the block number of the containing block, entry number within that
block, block pointer value

The program was tested and confirmed to work as intended on SEASnet linux server 09.

