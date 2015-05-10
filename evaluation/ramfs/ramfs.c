#include "ramfs.h"
#include <rvm.h>
#include <assert.h>
#include <malloc_simple.h>
#include <rmem_backend.h>
#include <unistd.h>

#include "log.h"

#ifdef USE_RVM
rvm_cfg_t *cfg;
#endif

#define LOG_FILE_PATH "/nscratch/joao/repos/rmem-server/evaluation/ramfs/log_out"

static int ramfs_getattr(const char *path, struct stat *stbuf)
{
   LOG(8, ("getattr path: %s\n", path));
   int res;
   dentry_t* d;

   res = 0;
   d = get_path(path);

   if (!d) {
      LOG(8, ("getattr ENOENT\n"));
      res = -ENOENT;
   }
   else {
      // pass inode info
      memset(stbuf, 0, sizeof(struct stat));
      stbuf->st_mode = d->in->mode;
      stbuf->st_nlink = d->in->nlink;
      stbuf->st_size = d->in->len;
   }

   LOG(8, ("getattr returns: %d\n", res));
   return res;
}

static int ramfs_getxattr(const char *path, const char* name, char* value, size_t size)
{
   LOG(8, ("getxattr path: %s\n", path));
   assert(0);
   return -1;
}

static int ramfs_listxattr(const char *path, char* name, size_t size)
{
   LOG(8, ("listxattr path: %s\n", path));
   assert(0);
   return -1;
}

static int ramfs_fsync(const char *path, int only_data, struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_fsync: path: %s only_data: %d\n", path, only_data)); 
   return 0;
}

static int ramfs_fsyncdir(const char *path, int only_data, struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_fsyncdir: path: %s only_data: %d\n", path, only_data)); 
   return 0;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
      off_t offset, struct fuse_file_info *fi)
{
   LOG(8, ("readdir\n"));

   dentry_t* d;

   if(!( d = get_path(path) )) {
      LOG(8, ("readdir ENOENT\n"));
      return -ENOENT;
   }

   filler(buf, ".", NULL, 0);
   filler(buf, "..", NULL, 0);

   for (d = d->dchild; d; d = d->dnext)
      filler(buf, d->name, NULL, 0);

   LOG(8, ("readdir return 0\n"));
   return 0;
}

static int ramfs_open(const char *path, struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_open path: %s\n", path));
   dentry_t* d;

   d = get_path(path);

   if (!d) {
      LOG(8, ("ramfs_open ENOENT\n"));
      return -ENOENT;
   }

   return 0;
}

static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
      struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_read path: %s\n", path));
   size_t len;
   dentry_t* d;
   (void) fi;

   d = get_path(path);

   if(!d) {
      LOG(8, ("ramfs_read ENOENT\n"));
      return -ENOENT;
   }

   len = d->in->len;

   if (offset < len) {
      if (offset + size > len)
         size = len - offset;

      // copy data to be read to buf from inode data
      memcpy(buf, d->in->data + offset, size);
   }
   else
      size = 0;
      
   LOG(8, ("ramfs_read return size:%ld\n", size));

   return size;
}

static int ramfs_write(const char* path, const char* buf, size_t size, off_t offset,
      struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_write\n"));
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
   LOG(8, ("ramfs_mkentry\n"));
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
   LOG(8, ("ramfs_mknod\n"));
   // new file
   return __ramfs_mkentry(path, mode);
}

static int ramfs_mkdir(const char * path, mode_t mode)
{
   LOG(8, ("ramfs_mkdir %s\n", path));
   // new dir
   return __ramfs_mkentry(path, mode | S_IFDIR);
}

static int ramfs_unlink(const char* path)
{
   LOG(8, ("ramfs_unlink\n"));
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;

   /* linux calls rmdir if target is a directory */

   return iunlink(d, d->in);
}

static int ramfs_rmdir(const char* path)
{
   LOG(8, ("ramfs_rmdir\n"));
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;

   // removes dentry reference to inode object
   return iunlink(d, d->in);
}

static void* ramfs_init(struct fuse_conn_info *conn)
{
   LOG(8, ("ramfs_init\n"));
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

   // redirect stderr to stdout
   assert(dup2(fileno(stdout), fileno(stderr)) != -1);
   assert( freopen(LOG_FILE_PATH,"a", stdout) != NULL);
   return NULL;
}

static void ramfs_destroy(void* v)
{
   LOG(8, ("ramfs_destroy\n"));
   ramfs_opt_destroy();
}


// function mapping
static struct fuse_operations ramfs_oper = {
   .init = ramfs_init,
   .destroy = ramfs_destroy,

   .getattr   = ramfs_getattr,
   .getxattr   = ramfs_getxattr,
   .listxattr   = ramfs_listxattr,

   .open   = ramfs_open,
// .create = ramfs_create, // open will be called
   
   .read   = ramfs_read,
   .readdir = ramfs_readdir,
   .write = ramfs_write,

   .mknod = ramfs_mknod,
   .mkdir = ramfs_mkdir,

   .unlink = ramfs_unlink,
   .rmdir = ramfs_rmdir,

   .fsync = ramfs_fsync,
   .fsyncdir = ramfs_fsyncdir,
};

int main(int argc, char *argv[])
{
   LOG(8, ("main\n"));
   // fuse will parse mount options
   return fuse_main(argc, argv, &ramfs_oper, NULL);
}

