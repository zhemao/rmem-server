#include "ramfs.h"
#include "config.h"
#include "buddy.h"
#include "log.h"

extern reg_t reg_s;

static dentry_t* droot;

int ilink(dentry_t* d, inode_t* in)
{  
   dentry_t* dn;

   // safe checks
   if (!d || d->in || !in)
      return -1;

   // we will add a new dentry pointing this inode
   // increment dentry count (link count)
   in->nlink++;

   // add dentry to inode list

   if ( !in->dentries )
      in->dentries = d;
   else
   {
      for ( dn = in->dentries; dn->dinode; dn = dn->dinode )
          ;
      
      dn->dinode = d;
   }
   
   d->in = in;
   
   return 0;
}

int iunlink(dentry_t* d, inode_t* in)
{
   dentry_t* dn;
   
   // safe checks
   if (!d || !in || d->in != in || in->nlink == 0)
      return -EIO;
   
   // if in is a directory and there are children
   // or it's root
   if ( S_ISDIR(in->mode) && ( d->dchild || d == droot ) )
      return -EISDIR;

   // remove dentry from inode list
   if ( in->dentries == d )
   {
      in->dentries = d->dinode;
   }
   else
   {
      for ( dn = in->dentries; dn->dinode && dn->dinode != d; dn = dn->dinode )
         ;

      if (dn->in == NULL)
         return -EIO;

      dn->dinode = d->dinode;
   }

   // remove dentry from parent children list
   if (d->dparent->dchild == d)
      d->dparent->dchild = d->dnext;
   else
   {
      for ( dn = d->dparent->dchild; dn->dnext != d; dn = dn->dnext )
         ;

      dn->dnext = d->dnext;
   }
   
   d->in = NULL;
   // useless dentry
   free_dentry(d);
   in->nlink--;

   if (!in->nlink)
      free_inode(in);

   return 0;
}

inode_t* alloc_inode(mode_t mode)
{
   inode_t* in;

#ifdef USE_CUSTOM_ALLOC
   in = (inode_t*)buddy_memalloc(&reg_s, sizeof(inode_t));
   LOG(8, ("custom alloced %d bytes ptr: %lx\n", sizeof(inode_t), in));
#else
   in = (inode_t*)malloc(sizeof(inode_t));
#endif

   if (in) {
      in->data = NULL;
      in->len = 0;
      in->nlink = 0;
      in->mode = mode;
      in->dentries = NULL;
   }

   return in;
}

int free_inode(inode_t* in)
{
   if (in) {
      if (in->nlink)
         return -1;

      if (in->data) {
#ifdef USE_CUSTOM_ALLOC
         buddy_memfree(&reg_s, in->data);
         LOG(8, ("custom freed ptr: %lx\n", in->data));
#else
         free(in->data);
#endif
      }

#ifdef USE_CUSTOM_ALLOC
      buddy_memfree(&reg_s, in);
#else
      free(in);
#endif
   }

   return 0;
}

dentry_t* alloc_dentry(char* name, inode_t* in)
{
   dentry_t* d;
   int nlen;

#ifdef USE_CUSTOM_ALLOC
   d = (dentry_t*)buddy_memalloc(&reg_s, sizeof(dentry_t));
   LOG(8, ("custom alloced %d bytes ptr: %lx\n", sizeof(dentry_t), d));
#else 
   d = (dentry_t*)malloc(sizeof(dentry_t));
#endif

   if (d) {
      d->in = NULL;
      d->dnext = NULL;
      d->dchild = NULL;
      d->dinode = NULL;
      d->dparent = NULL;

      // links it to an inode
      if (!ilink(d, in))
      {
         nlen = strlen(name);

#ifdef USE_CUSTOM_ALLOC
         d->name = (char*)buddy_memalloc(&reg_s, sizeof(char) * nlen);
         LOG(8, ("custom alloced %d bytes ptr: %lx\n", nlen, d->name));
#else 
         d->name = (char*)malloc(sizeof(char)*nlen);
#endif
         strcpy(d->name, name);
      }
   }

   return d;
}

int free_dentry(dentry_t* d)
{
   if (d) {
      if (d->dnext || d->dchild || d->dinode)
         return -1;

      if (d->name) {
#ifdef USE_CUSTOM_ALLOC
         buddy_memfree(&reg_s, d->name);
         LOG(8, ("custom freed ptr: %lx\n", d->name));
#else
         free(d->name);
#endif
      }
#ifdef USE_CUSTOM_ALLOC
      buddy_memfree(&reg_s, d);
      LOG(8, ("custom freed ptr: %lx\n", d));
#else
      free(d);
#endif

   }

   return 0;
}

dentry_t* get_dentry(dentry_t* dparent, const char* name)
{
   dentry_t* d;

   // get dentry from parent list
   for (d = dparent->dchild; d != NULL; d = d->dnext)
      if (!strcmp(d->name, name))
          break;

   return d;
}

// internal function to get path
static dentry_t* __get_path(char* path)
{
   dentry_t* d;
   char* t;

   d = droot;

   if (!path[1])
      return d;

   t = strtok(path, "/");

   if ( (d = get_dentry(d, t)) == NULL )
      return NULL;

   while ( (t = strtok(NULL, "/")) ) {
      if ( (d = get_dentry(d, t)) == NULL )
         break;
   }

   return d;
}

dentry_t* get_parent(const char* path)
{
   char p[MAX_PATH_SIZE];
   char* l;
   size_t len;

   // copy path, but leave filename
   l = rindex(path, '/');
   len = (size_t)l - (size_t)path;

   memcpy(p, path, len);
   p[len] = '\0';
   
   return __get_path(p);
}

dentry_t* get_path(const char* path)
{
   char p[MAX_PATH_SIZE];
  
   strncpy(p, path, MAX_PATH_SIZE);

   return __get_path(p);
}

void get_filename(const char* path, char* name)
{
   char p[MAX_PATH_SIZE];
   char *t;
   char* prev;

   strncpy(p, path, MAX_PATH_SIZE);

   t = strtok(p, "/");

   do {
      prev = t; 
   } while ( ( t = strtok(NULL, "/") ) );
   
   strcpy(name, prev);

   return;
}

int d_addchild(dentry_t* dparent, dentry_t* child)
{
   dentry_t* d;

   // safe checks
   if (!dparent || !child || child->dparent)
      return -1;

   // add child to parent list
   if (!dparent->dchild)
      dparent->dchild = child;      
   else {
      for( d = dparent->dchild; d->dnext; d = d->dnext )
         ;
      
      d->dnext = child;
   } 

   child->dparent = dparent;
   
   return 0;
}

void ramfs_opt_destroy()
{
}

void ramfs_opt_init()
{
   droot = alloc_dentry("", alloc_inode(S_IFDIR | 0755));
}
