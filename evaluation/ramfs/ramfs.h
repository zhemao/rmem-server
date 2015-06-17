#ifndef __RAM_FS_H__
#define __RAM_FS_H__

#define FUSE_USE_VERSION  26
#define MAX_PATH_SIZE 256

#include <fuse.h>  
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#define ISROOT(D) d->dparent == NULL

typedef struct inode {
   void* data;
   size_t len;   
   nlink_t nlink;
   mode_t mode;

   struct dentry* dentries;
} inode_t;


typedef struct dentry {
   inode_t* in;
   char* name;

   struct dentry* dparent;
   struct dentry* dnext;
   struct dentry* dchild;
   struct dentry* dinode;

} dentry_t;

int ilink(dentry_t*, inode_t*);
int iunlink(dentry_t*, inode_t*);
inode_t* alloc_inode(mode_t);
int free_inode(inode_t*);
dentry_t* alloc_dentry(char*, inode_t*);
int free_dentry(dentry_t*);
dentry_t* get_path(const char*);
dentry_t* get_dentry(dentry_t*, const char*);
dentry_t* get_parent(const char*);
void get_filename(const char*, char*);
int d_addchild(dentry_t*, dentry_t*);
void ramfs_opt_init(bool);
void ramfs_opt_destroy();

#endif /*__RAM_FS_H__*/

