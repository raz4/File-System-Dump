#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>

#define S_BLOCK_OFF 1024
#define S_BLOCK_SIZE 1024
#define BLOCK_SIZE 1024
#define GROUP_DESC_BLOCK_OFF 2048
#define GROUP_DESC_BLOCK_SIZE 32
#define INODE_SIZE 128

int fd = -1;
unsigned int num_groups = -1;

struct S_BLOCK* super_block = NULL;
struct GROUP_DESC* group_desc = NULL;
struct INODE* inodes = NULL;

int *inode_full_array = NULL;
unsigned inode_count = 0;
int *inode_dir;
int *inode_dir_number;
int inode_dir_count = 0;

struct S_BLOCK {
	__u32	s_inodes_count;		/* Inodes count 0 */
	__u32	s_blocks_count;		/* Blocks count 4 */
	__u32	s_first_data_block;	/* First Data Block 20 */
	__u32	s_block_size;	        /* Block size 24 */
	__s32	s_frag_size;	        /* Fragment size 28 */
	__u32	s_blocks_per_group;	/* # Blocks per group 32 */
	__u32	s_frags_per_group;	/* # Fragments per group 36 */
	__u32	s_inodes_per_group;	/* # Inodes per group 40 */
	__u16	s_magic;		/* Magic signature 56 */
};

void get_s_block(struct S_BLOCK *block){
  if (pread(fd, &(block->s_inodes_count), 4, S_BLOCK_OFF) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_blocks_count), 4, S_BLOCK_OFF+4) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_first_data_block), 4, S_BLOCK_OFF+20) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_block_size), 4, S_BLOCK_OFF+24) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_frag_size), 4, S_BLOCK_OFF+28) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_blocks_per_group), 4, S_BLOCK_OFF+32) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_frags_per_group), 4, S_BLOCK_OFF+36) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_inodes_per_group), 4, S_BLOCK_OFF+40) < 1){perror("ERROR");exit(1);}
  if (pread(fd, &(block->s_magic), 2, S_BLOCK_OFF+56) < 1){perror("ERROR");exit(1);}

  /* getting block and fragment size */
  block->s_block_size = 1024 << (unsigned)block->s_block_size;
  if (block->s_frag_size < 1){
    block->s_frag_size = 1024 >> -(unsigned)block->s_frag_size;
  }
  else {
    block->s_frag_size = 1024 << (unsigned)block->s_frag_size;
  }
}

void check_s_block(struct S_BLOCK *block){
  int exit_flag = 0;

  /* get file size */
  int file_size = lseek(fd, 0, SEEK_END) + 1;
  lseek(fd, 0L, SEEK_SET);
  
  /* check magic number */
  if ((int)block->s_magic != 61267){
    fprintf(stderr, "Superblock - invalid magic: %04x\n", (int)block->s_magic);
    exit_flag = 1;
  }
  
  /* check block size */
  unsigned int blk_size = (unsigned int)block->s_block_size;
  if (blk_size < 512 || blk_size > 65536){
    fprintf(stderr, "Superblock - unreasonable block size: %u\n", block->s_block_size);
    exit_flag = 1;
  }
  if (blk_size != 0){
    while (blk_size != 1){
      if (blk_size%2 != 0){
	fprintf(stderr, "Superblock - invalid block size: %u\n", block->s_block_size);
	exit_flag = 1;
      }
      blk_size = blk_size/2;
    }
  }
  else {
    fprintf(stderr, "Superblock - invalid block size: %u\n", block->s_block_size);
    exit_flag = 1;
  }

  /* check blocks per group and total blocks */
  if (block->s_blocks_count % block->s_blocks_per_group != 0){
    fprintf(stderr, "Superblock - %u blocks, %u blocks/group", block->s_blocks_count, block->s_blocks_per_group);
    exit_flag = 1;
  }

  /* check total blocks and first data block with file size*/
  if (block->s_blocks_count > file_size){
    fprintf(stderr, "Superblock - invalid block count %u > image size %u", block->s_blocks_count, file_size);
    exit_flag = 1;
  }
  if (block->s_first_data_block > file_size){
    fprintf(stderr, "Superblock - invalid first block %u > image size %u", block->s_first_data_block, file_size);
    exit_flag = 1;
  }

  /* check total inodes with inodes per group */
  if (block->s_inodes_count % block->s_inodes_per_group != 0){
    fprintf(stderr, "Superblock - %u blocks, %u blocks/group", block->s_inodes_count, block->s_inodes_per_group);
    exit_flag = 1;
  }

  if (exit_flag == 1){
    exit(1);
  }
  
}

void process_super(){
  super_block = malloc(sizeof(struct S_BLOCK));
  get_s_block(super_block);
  check_s_block(super_block);
  
  int super_fd = creat("super.csv", 0666);
  if (super_fd == -1){fprintf(stderr, "Error creating super.csv!\n"); return;}

  num_groups = (super_block->s_blocks_count + super_block->s_blocks_per_group - 1) / super_block->s_blocks_per_group;
  
  dprintf(super_fd, "%04x,%u,%u,%u,%u,%u,%u,%u,%u\n", (unsigned)super_block->s_magic, (unsigned)super_block->s_inodes_count, (unsigned)super_block->s_blocks_count, (unsigned)super_block->s_block_size, (unsigned)super_block->s_frag_size, (unsigned)super_block->s_blocks_per_group, (unsigned)super_block->s_inodes_per_group, (unsigned)super_block->s_frags_per_group, super_block->s_first_data_block);

  close(super_fd);
}

struct GROUP_DESC {
  	__u32	bg_block_bitmap;		/* Blocks bitmap block */
	__u32	bg_inode_bitmap;		/* Inodes bitmap block */
	__u32	bg_inode_table;		/* Inodes table block */
	__u16	bg_free_blocks_count;	/* Free blocks count */
	__u16	bg_free_inodes_count;	/* Free inodes count */
	__u16	bg_used_dirs_count;	/* Directories count */
  //  __u32   bg_contained_blocks;     /* Contained blocks */
  //__u32	bg_reserved[3];
};

void process_group(){
  group_desc = malloc(num_groups*sizeof(struct GROUP_DESC));
  int group_fd = creat("group.csv", 0666);

  /* read group descriptor blocks*/
  for (int i = 0; i < num_groups; i++){
    if (pread(fd, &(group_desc[i].bg_block_bitmap), 4, (i*GROUP_DESC_BLOCK_SIZE)+GROUP_DESC_BLOCK_OFF) < 1){perror("ERROR");exit(1);}
    if (pread(fd, &(group_desc[i].bg_inode_bitmap), 4, (i*GROUP_DESC_BLOCK_SIZE) + GROUP_DESC_BLOCK_OFF +4) < 1){perror("ERROR");exit(1);}
    if (pread(fd, &(group_desc[i].bg_inode_table), 4, (i*GROUP_DESC_BLOCK_SIZE) + GROUP_DESC_BLOCK_OFF +8) < 1){perror("ERROR");exit(1);}
    if (pread(fd, &(group_desc[i].bg_free_blocks_count), 2, (i*GROUP_DESC_BLOCK_SIZE) + GROUP_DESC_BLOCK_OFF +12) < 1){perror("ERROR");exit(1);}
    if (pread(fd, &(group_desc[i].bg_free_inodes_count), 2, (i*GROUP_DESC_BLOCK_SIZE) + GROUP_DESC_BLOCK_OFF +14) < 1){perror("ERROR");exit(1);}
    if (pread(fd, &(group_desc[i].bg_used_dirs_count), 2, (i*GROUP_DESC_BLOCK_SIZE) + GROUP_DESC_BLOCK_OFF +16) < 1){perror("ERROR");exit(1);}
    //    group_desc[i].bg_contained_blocks = num_blocks_per_group;

    dprintf(group_fd, "%u,%u,%u,%u", super_block->s_blocks_per_group, group_desc[i].bg_free_blocks_count, group_desc[i].bg_free_inodes_count, \
	    group_desc[i].bg_used_dirs_count);

    /* check for errors */
    unsigned int block_start = i * super_block->s_blocks_per_group;
    unsigned int block_end = (i+1) * super_block->s_blocks_per_group - 1;
    if (block_start > group_desc[i].bg_inode_bitmap || block_end < group_desc[i].bg_inode_bitmap ){
      fprintf(stderr, "Group %i: blocks %u-%u, free inode map starts at %x\n", i, block_start, block_end, group_desc[i].bg_inode_bitmap);
      dprintf(group_fd, ",-1");
    }
    else{
      dprintf(group_fd, ",%x", group_desc[i].bg_inode_bitmap);
    }
    if (block_start > group_desc[i].bg_block_bitmap || block_end < group_desc[i].bg_block_bitmap ){
      fprintf(stderr, "Group %i: blocks %u-%u, free block map starts at %x\n", i, block_start, block_end, group_desc[i].bg_block_bitmap);
      dprintf(group_fd, ",-1");
    }
    else{
      dprintf(group_fd, ",%x", group_desc[i].bg_block_bitmap);
    }
    if (block_start > group_desc[i].bg_inode_table || block_end < group_desc[i].bg_inode_table ){
      fprintf(stderr, "Group %i: blocks %u-%u, inode table starts at %x\n", i, block_start, block_end, group_desc[i].bg_inode_table);
      dprintf(group_fd, ",-1");
    }
    else {
      dprintf(group_fd, ",%x", group_desc[i].bg_inode_table);
    }
    dprintf(group_fd, "\n");
    
  }
  close(group_fd);
}

void process_bitmap(){
  //int bitmap_fd = creat("bitmap.csv", 0666);
  FILE *bitmap = fopen ("bitmap.csv", "w");
  
  __u8 byte;
  __u8 check;
  inode_full_array = malloc(super_block->s_blocks_per_group*(super_block->s_inodes_per_group)*sizeof(int));
  inode_count = 0;
  

  for (int i = 0; i < num_groups; i++){

    /*int offset = group_desc[i].bg_block_bitmap*super_block->s_block_size;
    int block_off = i*super_block->s_blocks_per_group+1;

    char *buffer = malloc(super_block->s_block_size);
    pread(fd, buffer, super_block->s_block_size, offset);
    for (int i = 0; i < super_block->s_block_size; i++){
      byte = buffer[i];
      check = 1;
      for (int j = 0; j < 8; j++){
	if (!(byte&check)){
	  fprintf(bitmap, "%x,%u\n", group_desc[i].bg_block_bitmap, block_off+i*8+j);
	}
	    check = check << 1;
      }
    }

    offset = group_desc[i].bg_inode_bitmap*super_block->s_block_size;
    int inode_off = i*super_block->s_inodes_per_group+1;

    pread(fd, buffer, super_block->s_block_size, offset);
    for	(int i = 0; i < super_block->s_block_size; i++){
      byte = buffer[i];
      check = 1;
      for (int j = 0; j < 8; j++){
	int inode_id = inode_off+j+i*8;
	if (!(byte&check)){
          fprintf(bitmap, "%x,%u\n", group_desc[i].bg_inode_bitmap, inode_off+i*8+j);
	}
	else if (super_block->s_inodes_per_group + inode_off > inode_id){
	  inode_full_array[inode_count] = inode_id;
	    inode_count++;
	}
            check = check << 1;
      }
      }*/
    
    int x = i*super_block->s_blocks_per_group;
    for (int j = 0; j < super_block->s_block_size; j++){
      check = 0x01;
      pread(fd, &byte, 1, (group_desc[i].bg_block_bitmap*super_block->s_block_size) + j);
      for (int k = 0; k < 8; k++){
	  if (!(byte&check)){
	    fprintf(bitmap, "%x,%u\n", group_desc[i].bg_block_bitmap, x + (j*8) + k+1);
	    //dprintf(1, "%i,%u\n", i, x + (j*8) + k+1);
	  }
	  check = check << 1;
      }
    }
    int y = i*super_block->s_inodes_per_group;
    for (int j = 0; j < super_block->s_block_size; j++){
      check = 1;
      pread(fd, &byte, 1, (group_desc[i].bg_inode_bitmap*super_block->s_block_size) + j);
      for (int k = 0; k < 8; k++){
	int inode_id = i*super_block->s_inodes_per_group+1+j*8+k;
          if (!(byte&check)){
            fprintf(bitmap, "%x,%u\n", group_desc[i].bg_inode_bitmap, y + (j*8) + k+1);
            //dprintf(1, "%i,%u\n", i, y + (j*8) + k+1);
          }
	  else if (super_block->s_inodes_per_group + i*super_block->s_inodes_per_group+1 > inode_id){
	    inode_full_array[inode_count] = inode_id;
	    inode_count++;
	  }
          check = check << 1;
      }
    }
  }
  fclose(bitmap);
}

struct INODE {

	__u16	i_mode;		/* File mode */
	__u32	i_uid;		/* Owner Uid */
	__u32	i_size;		/* Size in bytes */
	__u32	i_atime;	/* Access time */
	__u32	i_ctime;	/* Creation time */
	__u32	i_mtime;	/* Modification time */
  /*__u32	i_dtime;	 Deletion Time */
	__u16	i_gid;		/* Group Id */
	__u16	i_links_count;	/* Links count */
	__u32	i_blocks;	/* Blocks count */
	__u32	i_flags;	/* File flags */
  
	__u32	i_block[15];/* Pointers to blocks */
	__u32	i_version;	/* File version (for NFS) */
	__u32	i_file_acl;	/* File ACL */
	__u32	i_dir_acl;	/* Directory ACL */
	__u32	i_faddr;	/* Fragment address */
};

void process_inode(){
  //int inode_fd = creat("inode.csv", 0666);
  FILE *inode_file = fopen("inode.csv", "w");
  int num_inodes = super_block->s_inodes_count;
  inodes = malloc(num_inodes*sizeof(struct INODE));
  int16_t byte2;
  int8_t check8;
  int x = 0;
  int8_t bit8;
  int32_t byte4;
  
  inode_dir = malloc(sizeof(int)*inode_count);
  inode_dir_number = malloc(sizeof(int)*inode_count);
  
  for (int i = 0; i < inode_count; i++){
    unsigned int inode_num = inode_full_array[i];
    fprintf(inode_file, "%u,", inode_num);
    int offset = group_desc[(inode_num-1)/super_block->s_inodes_per_group].bg_inode_table*super_block->s_block_size+((inode_num-1)%super_block->s_inodes_per_group)*INODE_SIZE;
    //printf("group_desc:%u,",group_desc[(inode_num-1)/super_block->s_inodes_per_group].bg_inode_table);
    pread(fd, &byte2, 2, offset);
    byte2 = byte2 >> 12;
    //    printf("%x, %i,    ", byte2, offset);
    if (byte2 & 0x8){
      fprintf(inode_file, "f,");
    }
    else if (byte2 & 0xA){
      fprintf(inode_file, "s,");
    }
    else if (byte2 & 0x4){
      inode_dir[inode_dir_count] = offset;
      inode_dir_number[inode_dir_count] = inode_num;
      inode_dir_count++;
      fprintf(inode_file, "d,");
    }
    else {
      fprintf(inode_file, "?,");
    }

    byte2 = 0;
    pread(fd, &byte2, 2, offset);
    fprintf(inode_file, "%o,", byte2);

    // uid
    pread(fd, &byte2, 2, offset+2);
    fprintf(inode_file, "%u,", byte2);

    //gid
    pread(fd, &byte2, 2, offset+24);
    fprintf(inode_file, "%u,", byte2);

    //link count
    pread(fd, &byte2, 2, offset+26);
    fprintf(inode_file, "%u,", byte2);

    //creation time
    pread(fd, &byte4, 4, offset+12);
    fprintf(inode_file, "%x,", byte4);

    //modification time
    pread(fd, &byte4, 4, offset+16);
    fprintf(inode_file, "%x,", byte4);

    //access time
    pread(fd, &byte4, 4, offset+8);
    fprintf(inode_file, "%x,", byte4);

    //file size
    pread(fd, &byte4, 4, offset+4);
    fprintf(inode_file, "%u,", byte4);

    //number of blocks
    pread(fd, &byte4, 4, offset+28);
    uint b = byte4;
    b = b/2;
    fprintf(inode_file, "%u,", b);

    // get data blocks
    for (int k = 0; k < 14; k++){
      pread(fd, &byte4, 4, offset+40+4*k);
      fprintf(inode_file, "%x,", byte4);
    }
    pread(fd, &byte4, 4, offset+40+4*14);
    fprintf(inode_file, "%x\n", byte4);
    
   }

  fclose(inode_file);
}

void process_directory(){
  FILE *dir_file = fopen("directory.csv", "w");
  int32_t byte4;
  int entry = 0;
  int32_t offset = 0;
  int8_t byte1;
  int16_t byte2;
  
  for (int i = 0; i < inode_dir_count; i++){
    for (int j = 0; j < 12; j++){
      pread(fd, &byte4, 4, inode_dir[i] + j * 4 + 40);
      offset = byte4 * super_block->s_block_size;
      //printf("offset: %u\n", offset);
      //printf("byte4: %u\n", byte4);
      if (byte4){
	int32_t offset2 = offset;
	while(offset2 < offset + super_block->s_block_size){
	  int inode_number;
	  pread(fd, &inode_number, 4, offset2);
	  pread(fd, &byte2, 2, offset2+4);
	  int entry_length = byte2;
	  if (inode_number == 0){
	    offset2 = offset2 + entry_length;
	    entry++;
	  }
	  else{
	    fprintf(dir_file, "%u,", inode_dir_number[i]);
	    fprintf(dir_file, "%u,", entry);
	    fprintf(dir_file, "%u,", entry_length);
	    pread(fd, &byte1, 1, offset2+6);
	    fprintf(dir_file, "%u,", byte1);
	    fprintf(dir_file, "%u,", inode_number);

	    int name_length = byte1;
	    char ch;
	    for (int k = 0; k < name_length; k++){
	      pread(fd, &ch, 1, offset2 + k + 8);
	      fprintf(dir_file, "%c", ch);
	    }
	    fprintf(dir_file, "\n");
	    entry++;
	    offset2 = offset2 + entry_length;
	  }
	  
	}
      }
    }
  }

  fclose(dir_file);
}

void process_indirect(){
  FILE *ind_file = fopen("indirect.csv", "w");
  int entry = 0;
  int32_t byte4;
  
  for (int i = 0; i < inode_count; i++){
    unsigned int inode_num = inode_full_array[i];
    pread(fd, &byte4, 4, inode_num + 12 * 4 + 40);
    //pread(fd, &byte4, 4, offset);  
    int block = byte4;
    entry = 0;
    //printf("block: %u\n", byte4);
    for (int j = 0; j < super_block->s_block_size/4; j++){
      pread(fd, &byte4, 4, block * super_block->s_block_size + j*4);
      //printf("block_ptr_value: %u\n", byte4);
      if (byte4){
	fprintf(ind_file, "%x,", block);
	fprintf(ind_file, "%u,", entry);
	fprintf(ind_file, "%x\n,", byte4);
	entry++;
      }
    }
  }

  fclose(ind_file);
}

int main(int argc, char* argv[]){
  if (argc != 2){
    printf("ERROR: only one argument is required\n");
    exit(1);
  }

  /* open disk image */
  fd = open(argv[1], O_RDONLY);
  if (fd == -1){
    perror("ERROR: couldn't open file");
    exit(1);
  }

  process_super();
  process_group();
  process_bitmap();
  process_inode();
  process_directory();
  process_indirect();
  
  return 0;
}
