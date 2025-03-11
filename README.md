# IN2140_V25

Group participants
- victou
- hennion
- halvoki

## Important

The global variable `DEBUG_MODE` is a flag that enables or disables custom debugging statements created by us. The default is 0 (off). Adjust the flag in `inode.c` to 1 to enable extensive debugging statements in the terminal.
## How to read Master File Table

The function `load_inodes` reads binary data from a Master File Table (MFT) and constructs a tree of  `struct inode`. It returns a pointer to the root node (id 0).

### Parsing inode fields
For each inode, in a loop, the following fields are read from the file in this order:
- The *id*
- The *name length*
- The *flags* for is_directory and is_readonly
- The *filesize*
- The *number of entries* that the inode points to
### Creating and storing inodes

- Memory is allocated to store dynamically sized properties such as:
	- Name
	- Entries
	- Inode pointer
- An inode is constructed from the parsed data
- The function keeps an array *inode_map* indexed by node IDs.
	- If an ID is larger than the size of the array, the array is resized to accomodate the new ID.
	- inode_map\[ID] is set to the newly created inode
	- MAX ID is updated if the read ID is bigger than seen IDs
	- Root should be id 0
### Referencing from directories
After reading all inodes. The function loops back through inode_map. For each directory node, every entry in node->entries\[] is an ID that must be converted to the actual node pointer. node->entries\[j], which is originally an id, is replaced with the pointer inode_map\[node->entries\[j]].

When all pointers are set, the file system should reflect a tree where the root node is the root directory, this inode contains pointers to its files and subdirectories, which again contains more pointers to their respective files and subdirectories.

## Shortcomings
### Errors and memory leaks

The project contains no errors and no memory leaks when running the test scripts on IFI Linux machines (valgrind report).

### Not handling extents of size 2, 3 or 4

For files:
Treat each 64-bit entry as two 32-bit values representing a disk block and the extent. This would mean reading 32 bits twice to get the block_no and extent for a file. We feed the extent size into `allocate_block` to allocate the corresponding number of blocks. This would necessitate updating the deleting functions to also free the allocated blocks. 
### save_inodes not writing to MFT
The MFT is being overwritten, this is verified by implementing a custom hex-dump function that dumps the content of a binary file. Upon running the hex-dump after load_inodes is called, the content of the MFT is being updated.

However, the MFT dump done by the test scripts prints an empty table containing no blocks. 

