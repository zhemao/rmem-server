#include "ramfs.h"

#include <assert.h>

extern "C" {
#include <rvm.h>
#include <rmem_backend.h>
#include <ramcloud_backend.h>
}

#include <unistd.h>

#include "config.h"
#include "buddy.h"
#include "log.h"

//#define USE_RAMCLOUD

#ifdef USE_RAMCLOUD
#define PORT "11100"
#define HOST "f2"
#else 
#define PORT "12345"
#define HOST "f1"
#endif

#ifdef USE_RVM
rvm_cfg_t *cfg;
#endif

#ifdef USE_CUSTOM_ALLOC

#define ONE_MB (1024*1024)
// must be power of 2
#define POOL_SIZE (32 *  ONE_MB) 
#define PG_SIZE (4*1024)
#define PAGE_ORDER (LOG2(PG_SIZE))

#define TX_COMMIT {\
   rvm_txn_commit(cfg, main_tx); \
   main_tx = rvm_txn_begin(cfg); \
}

reg_t reg_s;

#endif

void* mem_pool;
extern dentry_t* droot;

// single threaded code, there is only one transaction
rvm_txid_t main_tx;
bool in_recovery = false;

extern "C" {
   void *simple_malloc(rvm_cfg_t *cfg, size_t size);
   bool simple_free(rvm_cfg_t *cfg, void *buf);
}


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
      LOG(8, ("getattr path:%s st_mode: %d nlink: %d st_size: %d %d %d %d %d %d %d\n", 
                  path, d->in->mode, d->in->nlink, d->in->len,
                  S_ISDIR(d->in->mode), S_ISBLK(d->in->mode), S_ISCHR(d->in->mode), S_ISFIFO(d->in->mode),
                  S_ISREG(d->in->mode), S_ISLNK(d->in->mode)));

   }

   LOG(8, ("getattr returns: %d\n", res));
   return res;
}

static int ramfs_getxattr(const char *path, const char* name, char* value, size_t size)
{
   LOG(8, ("getxattr path: %s\n", path));
   return -1;
}

static int ramfs_listxattr(const char *path, char* name, size_t size)
{
   LOG(8, ("listxattr path: %s\n", path));
   return -1;
}

static int ramfs_fsync(const char *path, int only_data, struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_fsync: path: %s only_data: %d\n", path, only_data)); 

   TX_COMMIT;

   return 0;
}

static int ramfs_fsyncdir(const char *path, int only_data, struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_fsyncdir: path: %s only_data: %d\n", path, only_data)); 
   TX_COMMIT;
   return 0;
}

static int ramfs_flush(const char *path, struct fuse_file_info *fi)
{
   LOG(8, ("ramfs_flush: path: %s\n", path));

   TX_COMMIT;

   return 0;
}

static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
      off_t offset, struct fuse_file_info *fi)
{
   LOG(8, ("readdir path: %s\n", path));

   dentry_t* d;

   if(!( d = get_path(path) )) {
      LOG(8, ("readdir ENOENT\n"));
      return -ENOENT;
   }

   filler(buf, ".", NULL, 0);
   filler(buf, "..", NULL, 0);

   for (d = d->dchild; d; d = d->dnext) {
       LOG(8, ("readdir child name: %s\n", d->name));
      filler(buf, d->name, NULL, 0);
   }

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

#ifdef USE_CUSTOM_ALLOC
      tmp = buddy_memalloc(&reg_s, in->len+size);
      LOG(8, ("custom alloced %lu bytes ptr: %lx\n", in->len+size, (uintptr_t)tmp));
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
#ifdef USE_CUSTOM_ALLOC
            buddy_memfree(&reg_s, in->data);
            LOG(8, ("custom freed %d ptr: %lx\n", in->data));
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
   
   TX_COMMIT;

   return size;
}

// generic operations to create file and directories
static int __ramfs_mkentry(const char * path, mode_t mode)
{
   LOG(8, ("ramfs_mkentry %s \n", path));
   dentry_t* d;
   dentry_t* nd;
   char name[MAX_PATH_SIZE];

   LOG(8, ("ramfs_mkentry getting parent\n"));
   d = get_parent(path);
   LOG(8, ("ramfs_mkentry getting parent done\n"));

   if (!d) {
      LOG(8, ("get_parent returned ENOENT\n"));
      return -ENOENT;
   }

   LOG(8, ("ramfs_mkentry getting filename\n"));
   // get filename
   get_filename(path, name);
   
   LOG(8, ("ramfs_mkentry name:%s \n", name));

   //struct stat stbuf;
   //int st = ramfs_getattr("/", &stbuf);

   // check if it already exists
   if (get_dentry(d, name)) {
      return -EEXIST;
   }
   LOG(8, ("ramfs_mkentry allocing dentry\n"));

   // create new instance
   inode_t* inode = alloc_inode(mode);
   //st = ramfs_getattr("/", &stbuf);
   nd = alloc_dentry(name, inode);
   
   LOG(8, ("ramfs_mkentry alloced dentry\n"));
   //st = ramfs_getattr("/", &stbuf);

   LOG(8, ("ramfs_mkentry adding dentry to parent\n"));
   // add dentry to parent list
   if (d_addchild(d, nd)) {
      iunlink(nd, nd->in);
      return -ENOSPC;
   }
   
   LOG(8, ("ramfs_mkentry done\n"));

   return 0;
}

static int ramfs_mknod(const char * path, mode_t mode, dev_t dev)
{
   LOG(8, ("ramfs_mknod\n"));
   // new file
   //struct stat stbuf;
   //int st = ramfs_getattr("/", &stbuf);

   int ret = __ramfs_mkentry(path, mode);

   //st = ramfs_getattr("/", &stbuf);

   TX_COMMIT;
   return ret;
}

static int ramfs_mkdir(const char * path, mode_t mode)
{
   LOG(8, ("ramfs_mkdir %s\n", path));
   // new dir

   //struct stat stbuf;
   //int st = ramfs_getattr("/", &stbuf);

   int ret =  __ramfs_mkentry(path, mode | S_IFDIR);
   LOG(8, ("ramfs_mkdir returns %d\n", ret));
   
   //st = ramfs_getattr("/", &stbuf);

   TX_COMMIT;

   return ret;
}

static int ramfs_unlink(const char* path)
{
   LOG(8, ("ramfs_unlink\n"));
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;

   /* linux calls rmdir if target is a directory */

   int ret = iunlink(d, d->in);
   TX_COMMIT;
   
   return ret;
}

static int ramfs_rmdir(const char* path)
{
   LOG(8, ("ramfs_rmdir\n"));
   dentry_t* d;

   d = get_path(path);

   if (!d)
      return -ENOENT;

   // removes dentry reference to inode object
   int ret =  iunlink(d, d->in);
   TX_COMMIT;

   return ret;
}

static void* ramfs_init(struct fuse_conn_info *conn)
{

   LOG(8, ("ramfs_init\n"));

#ifdef USE_RVM
   rvm_opt_t opt;
   opt.host = HOST;
   opt.port = PORT;
   opt.alloc_fp = simple_malloc;
   opt.free_fp = simple_free;
   opt.recovery = in_recovery;

   if (in_recovery) {
      LOG(8, ("Recovering data.."));
   } else {
      LOG(8, ("Not Recovering data.."));
   }

   opt.recovery = in_recovery;
#ifdef USE_RAMCLOUD
   cfg = rvm_cfg_create(&opt, create_ramcloud_layer);
#else
   LOG(8, ("rvm_cfg_create.."));
   cfg = rvm_cfg_create(&opt, create_rmem_layer);
   LOG(8, ("rvm_cfg_create done.."));
#endif
   assert(cfg);

   LOG(8, ("rvm_txn_begin.."));
   main_tx = rvm_txn_begin(cfg);
#endif

#ifdef USE_CUSTOM_ALLOC
#ifdef USE_RVM
   if (in_recovery) {
       LOG(8, ("rvm_get_usr_data.."));
      mem_pool = rvm_get_usr_data(cfg);
      assert(mem_pool);
   } else {
      mem_pool = rvm_alloc(cfg, POOL_SIZE + sizeof(uintptr_t));
      assert(mem_pool);
      rvm_set_usr_data(cfg, mem_pool);
      TX_COMMIT; 
   }
   LOG(8, ("rvm_alloc mem_pool: %lx\n", (uintptr_t)mem_pool));
#else
   mem_pool = malloc(POOL_SIZE + sizeof(uintptr_t));
   assert(mem_pool);
#endif
   reg_s.sz = POOL_SIZE;
   assert( buddy_meminit(&reg_s, PAGE_ORDER, mem_pool + sizeof(uintptr_t), in_recovery) != -1);
   assert(reg_s.buf);
#endif

   ramfs_opt_init(in_recovery);
   if (!in_recovery)
       TX_COMMIT; // commit position of droot

   return NULL;
}

static void ramfs_destroy(void* v)
{
   LOG(8, ("ramfs_destroy in_recovery: %d\n", in_recovery));
   ramfs_opt_destroy();
#ifdef USE_RVM
   LOG(8, ("ramfs_destroy rvm_cfg_destroy cfg: %lx\n", (uintptr_t)cfg));
  // rvm_cfg_destroy(cfg);
#endif
}

/*
// function mapping
static struct fuse_operations ramfs_oper = {
   .init = ramfs_init,
   .destroy = ramfs_destroy,

   .getattr   = ramfs_getattr,
//   .getxattr   = ramfs_getxattr,
//   .listxattr   = ramfs_listxattr,

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
//   .fsyncdir = ramfs_fsyncdir,
};
*/

static struct fuse_operations ramfs_oper;

void check_recovery(int* argc, char* argv[])
{
    int nargs = *argc;
    for (int i = 0; i < nargs; ++i) {
        if (strcmp(argv[i], "-recovery") == 0) {
            in_recovery = true;
            (*argc)--;
            for (int j = i + 1; j < nargs; ++j) {
                strcpy(argv[j-1], argv[j]);
            }
        }
    }

    LOG(8, ("argc: %d nargs: %d\n", *argc, nargs));
    for (int i = 0; i < *argc; ++i) {
        LOG(8, ("argv[%d] = %s\n", i, argv[i]));
    }
}

int main(int argc, char *argv[])
{
   ramfs_oper.init = ramfs_init;
   ramfs_oper.destroy = ramfs_destroy;
   ramfs_oper.getattr   = ramfs_getattr;
   ramfs_oper.open   = ramfs_open;
   ramfs_oper.read   = ramfs_read;
   ramfs_oper.readdir = ramfs_readdir;
   ramfs_oper.write = ramfs_write;
   ramfs_oper.mknod = ramfs_mknod;
   ramfs_oper.mkdir = ramfs_mkdir;
   ramfs_oper.unlink = ramfs_unlink;
   ramfs_oper.rmdir = ramfs_rmdir;
   ramfs_oper.fsync = ramfs_fsync;
   ramfs_oper.fsyncdir = ramfs_fsyncdir;
   ramfs_oper.flush = ramfs_flush;
   LOG(8, ("main\n"));
   // fuse will parse mount options

   // redirect stderr to stdout
   assert(dup2(fileno(stdout), fileno(stderr)) != -1);
   assert( freopen(LOG_FILE_PATH,"a", stdout) != NULL);

   check_recovery(&argc, argv);

   return fuse_main(argc, argv, &ramfs_oper, NULL);
}

