#include "ramfs.h"
#include <rvm.h>
#include <assert.h>
#include <malloc_simple.h>
#include <rmem_backend.h>

#include "log.h"

#ifdef USE_RVM
rvm_cfg_t *cfg;
#endif

#if LOGGING
FILE* flog;
#endif

static int ramfs_getattr(const char *path, struct stat *stbuf)
{
   fprintf(stderr, "getattr\n");
   int res;
   dentry_t* d;

   res = 0;
   d = get_path(path);

   if (!d) {
      res = -ENOENT;
   }
   else {
      // pass inode info
      memset(stbuf, 0, sizeof(struct stat));
      stbuf->st_mode = d->in->mode;
      stbuf->st_nlink = d->in->nlink;
      stbuf->st_size = d->in->len;
   }

   return res;
}

static int ramfs_fsync(const char *path, int only_data, struct fuse_file_info *fi)
{
#ifdef LOGGING
   LOG(flog, ("ramfs_fsync: path: %s only_data: %d\n", path, only_data)); 
#endif
   return 0;
}
  
static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
   fprintf(stderr, "readdir\n");
   (void) offset;
   (void) fi;
  
   dentry_t* d;

   if(!( d = get_path(path) ))
      return -ENOENT;
  
   // filler fills buf with each directory entry
   filler(buf, ".", NULL, 0);
   filler(buf, "..", NULL, 0);
  
   for (d = d->dchild; d; d = d->dnext)
      filler(buf, d->name, NULL, 0);
  
   return 0;
}
  
static int ramfs_open(const char *path, struct fuse_file_info *fi)
{
    fprintf(stderr, "ramfs_open\n");
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;
  
   // you could test access permitions here
   // take a look at man open for possible errors
  
   return 0;
}
  
static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    fprintf(stderr, "ramfs_read\n");
   size_t len;
   dentry_t* d;
   (void) fi;

   d = get_path(path);

   if(!d)
      return -ENOENT;
  
   len = d->in->len;

   if (offset < len) 
   {
      if (offset + size > len)
         size = len - offset;

      // copy data to be read to buf from inode data
      memcpy(buf, d->in->data + offset, size);
   }
   else
      size = 0;
  
   return size;
}

static int ramfs_write(const char* path, const char* buf, size_t size, off_t offset,
                       struct fuse_file_info *fi)
{
    fprintf(stderr, "ramfs_write\n");
   dentry_t* d;
   inode_t* in;
   void* tmp;
   (void) fi;

   d = get_path(path);

   if(!d)
      return -ENOENT;

   in = d->in;

   // do we have to make inode data larger?
   if (size + offset > in->len)
   {
      // can't use realloc, if it fails, we'd lost data
      // this is a huge performance penalty
      // to improve performance a smarter memory management algorithm should be used

#ifdef USE_RVM
      tmp = rvm_alloc(cfg, in->len + size);
#else
      tmp = malloc(in->len + size);
#endif
      
      if (tmp)
      {
         if (in->data)
         {
            memcpy(tmp, in->data, in->len);

            int i;
            for (i = 0; i < in->len; i+=2) {
                ((char*)tmp)[i] = 'T';
            }
#ifdef USE_RVM
            rvm_free(cfg, in->data);
#else
            free(in->data);
#endif
         }

         in->data = tmp;
         in->len += size;
      }
      else
         return -EFBIG;
   }

   memcpy(in->data + offset, buf, size);

   return size;
}

// generic operations to create file and directories
static int __ramfs_mkentry(const char * path, mode_t mode)
{
    fprintf(stderr, "ramfs_mkentry\n");
   dentry_t* d;
   dentry_t* nd;
   char name[MAX_PATH_SIZE];

   d = get_parent(path);

   if (!d)
      return -ENOENT;

   // get filename
   get_filename(path, name);

   // check if it already exists
   if (get_dentry(d, name))
      return -EEXIST;

   // create new instance
   nd = alloc_dentry(name, alloc_inode(mode));

   // add dentry to parent list
   if (d_addchild(d, nd))
   {
      // if fails, rollbak
      iunlink(nd, nd->in);
      return -ENOSPC;
   }

   return 0;
}

static int ramfs_mknod(const char * path, mode_t mode, dev_t dev)
{
    fprintf(stderr, "ramfs_mknod\n");
   // new file
   return __ramfs_mkentry(path, mode);
}

static int ramfs_mkdir(const char * path, mode_t mode)
{
    fprintf(stderr, "ramfs_mkdir\n");
   // new dir
   return __ramfs_mkentry(path, mode | S_IFDIR);
}

static int ramfs_unlink(const char* path)
{
    fprintf(stderr, "ramfs_unlink\n");
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;

   /* linux calls rmdir if target is a directory */

   return iunlink(d, d->in);
}

static int ramfs_rmdir(const char* path)
{
    fprintf(stderr, "ramfs_rmdir\n");
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;

   // removes dentry reference to inode object
   return iunlink(d, d->in);
}

static void* ramfs_init(struct fuse_conn_info *conn)
{
   (void) conn;

   ramfs_opt_init();

#ifdef USE_RVM
   rvm_opt_t opt;
   opt.host = "f2";
   opt.port = "12345";
   opt.alloc_fp = simple_malloc;
   opt.free_fp = simple_free;

   opt.recovery = false;
   cfg = rvm_cfg_create(&opt, create_rmem_layer);
   assert(cfg);
#endif

#ifdef LOGGING
   flog = fopen(LOG_FILE_PATH, "w");
   assert(flog);
#endif
   return NULL;
}

static void ramfs_destroy(void* v)
{
    ramfs_opt_destroy();
}


// function mapping
static struct fuse_operations ramfs_oper = {
    .init = ramfs_init,
    .destroy = ramfs_destroy,
    .getattr   = ramfs_getattr,
    .readdir = ramfs_readdir,
    .open   = ramfs_open,
    .read   = ramfs_read,
    .write = ramfs_write,
    .mknod = ramfs_mknod,
    .mkdir = ramfs_mkdir,
    .unlink = ramfs_unlink,
    .rmdir = ramfs_rmdir,
    .fsync = ramfs_fsync,
};

int main(int argc, char *argv[])
{
   // fuse will parse mount options
   return fuse_main(argc, argv, &ramfs_oper, NULL);
}


