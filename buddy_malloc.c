/* Nathan Pemberton
 * Lincoln Thurlow
 * Shuyu Pan
 * P3 CS111, UCSC Sprint 2012
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include "rvm.h"

//#define BUDDY_DEBUG 1

/** Size of the pool of recoverable pages used for buddy allocation. Must be
 * a multiple of the page size and a power of 2 */
#define POOL_SZ (1 << 14)

/** Minimum allocation returned by the buddy allocator in bytes. Must be a
 * power of 2 < POOL_SZ. */
#define MIN_PG_SZ 64

typedef struct {
  int32_t num_pg;
  int32_t max_lvl; //Deepest level possible (aka biggest block size)
  int32_t sz;       //length of map, i.e. map[map_sz - 1] is last entry in map
  uint64_t map[];
} __attribute__ ((packed)) map_t;

/* Index of the buddy of x */
#define BUDDY(x) (((x) % 2) ? (x) - 1 : (x) + 1)

/* Index of parent bit one level up */
#define PARENT(x) ((x) >> 1)

/* Index of child bit one level down (always the even indexed child) */
#define CHILD(x) ((x) << 1)

/* Literal bit index of the beginning of lvl
 * sz is the number of bits in level 0*/
//#define LVL_TO_BIT(lvl, sz) ( (lvl == 0) ? 0 : ((2 - (1 / (1 << (lvl - 1))))*sz))
#define LVL_TO_BIT(lvl, sz) ( (lvl == 0) ? 0 : (int)((2.0 - (1.0 / (double)(1 << (lvl - 1))))*(double)sz))

/* Number of pages at lvl if there are num_0pg pages at lvl 0 */
#define LVL_NUM_PG(lvl, num_0pg) ( num_0pg / (1 << lvl))

/* Size (in bytes) of a page at lvl
 * lvl: The target level
 * sz: Size of a page at level 0*/
#define LVL_PG_SZ(lvl, sz) (sz * (1 << lvl))

/* Smallest level that can contain n bytes
 * n: number of bytes to find the level of
 * sz: the size of a page at level 0*/
#define LEVEL(n, sz) ( (n) <= (sz) ? 0 : LOG2((n - 1) / (sz)) + 1)

/* Returns a pointer to the beginning of the actual memory region of map cast
 * to a char*
 */
#define MEM_REG(map) (((uint8_t *)(map)) + ((map)->sz*sizeof(uint64_t) + sizeof(map_t)))

bool map_check(int lvl, int lvl_idx, map_t *map);
void map_set(int lvl, int lvl_idx, map_t *map, bool val);
int map_next(int lvl, map_t *map);
int map_split(int lvl, map_t *map);
int map_merge(int lvl, int idx, map_t *map);
void print_map(rvm_cfg_t *cfg);

/* Initialize the buddy allocator. Returns a pointer to the buddy allocator
 * state in recoverable memory.
 */
void *buddy_meminit(rvm_cfg_t *cfg)
{
  size_t map_sz; //Size of map in groups of 64-bits (8 bytes)
  size_t new_reg_sz;
  map_t *map;

  //Allocate enough space for bitmaps in groups of 64-bits
  //Round up just in case
  map_sz = ((POOL_SZ / MIN_PG_SZ)*2 / 64);

  new_reg_sz = POOL_SZ + sizeof(map_t) + map_sz*sizeof(uint64_t);
  void *pool = rvm_blk_alloc(cfg, POOL_SZ);
  if(pool == NULL) {
      return NULL; //Use errno from rvm_blk_alloc
  }

  map = (map_t *)pool;
  map->num_pg = POOL_SZ / MIN_PG_SZ;
  map->sz = map_sz;
  map->max_lvl = LOG2((uint64_t)(POOL_SZ / MIN_PG_SZ));
  memset(map->map, 0, map_sz*sizeof(uint64_t)); //mark all regions free

#define BUDDY_DEBUG 1
#ifdef BUDDY_DEBUG
  printf("init: min_pg_sz %d\n", MIN_PG_SZ);
  printf("init: num_pg %d\n", map->num_pg);
  printf("init: map sz (num 64-bit ints) %d\n", map->sz);
  printf("init: max_lvl %d\n", map->max_lvl);
  printf("init: reg_sz %zd\n", POOL_SZ);
#endif
#undef BUDDY_DEBUG

  return 1;
}

void *buddy_malloc(rvm_cfg_t *cfg, size_t n_bytes)
{
  int idx;
  int lvl;  //The originally requested level

  if(cfg->alloc_data == NULL) {
      cfg->alloc_data = buddy_meminit();
  }
  map_t *map = (map_t *)cfg->alloc_data;

  if(n_bytes > (MIN_PG_SZ * map->num_pg))
  {
    fprintf(stderr, "memalloc: request exceeds memory\n");
    return NULL;
  }

  lvl = LEVEL((uint64_t)n_bytes, MIN_PG_SZ);
  idx = map_next(lvl, map);

#ifdef BUDDY_DEBUG
  printf("alloc: n_bytes %ld\n", n_bytes);
  printf("alloc: pg_sz %d\n", MIN_PG_SZ);
  printf("alloc: lvl %d\n", lvl);
  printf("alloc: initial idx %d\n", idx);
  printf("alloc: map before: \n");
  print_map(reg);
#endif

  if(idx >= 0)
  {
    //A space is available, allocate it
    map_set(lvl, idx, map, true);
  } else {
    //Need to look for more space
    idx = map_split(lvl + 1, map);
    if(idx >= 0) {
      map_set(lvl, idx, map, true);
    } else {
      //No space available
      fprintf(stderr, "buddy memalloc: No space!\n");
      return NULL;
    }
  }

#ifdef BUDDY_DEBUG
  printf("alloc: final idx %d\n", idx);
  printf("alloc: ptr %p\n", MEM_REG(map) + LVL_PG_SZ(lvl, MIN_PG_SZ)*idx);
  printf("map after:\n");
  print_map(reg);
#endif

  return MEM_REG(map) + LVL_PG_SZ(lvl, MIN_PG_SZ)*idx;
}

int buddy_memfree(rvm_cfg_t *cfg, void *buf)
{
  if(buf == NULL) {
    fprintf(stderr, "buddy_memfree: Tried to free NULL!\n");
    return -1;
  }

  map_t *map = (map_t *)cfg->alloc_data;
  uint64_t idx;

#ifdef BUDDY_DEBUG
  fprintf(stderr, "trying to free %p\n", buf);
  printf("free: map before:\n");
  print_map(reg);
#endif

  idx = (uint64_t)((uint8_t *)buf - MEM_REG(map)) / MIN_PG_SZ;
  map_set(0, idx, map, false);

#ifdef BUDDY_DEBUG
  int err = map_merge(0, idx, map);
  printf("free: idx %zd\n", idx);
  printf("free: map after: \n");
  print_map(reg);
  return err;
#else
  return map_merge(0, idx, map);
#endif
}

int map_merge(int lvl, int idx, map_t *map)
{
  if(lvl == map->max_lvl)
  {
    //Top level doesn't have a buddy to merge with so just end
    return 1;
  }

  if(!map_check(lvl, idx, map) && !map_check(lvl, BUDDY(idx), map))
  {
    //Both buddies are free, so merge up
    map_set(lvl + 1, PARENT(idx), map, false);
    return map_merge(lvl + 1, PARENT(idx), map);
  } else {
#ifdef BUDDY_DEBUG
    printf("merge: recursion stoped at lvl %d\n", lvl);
#endif
    //One of the buddies is still in use, stop merging
    return 1;
  }
}

bool map_check(int lvl, int lvl_idx, map_t *map)
{
  int32_t idx = LVL_TO_BIT(lvl, map->num_pg) + lvl_idx; //Literal bit index
  int32_t word_idx = idx / 64;  //Index of appropriate word in map
  uint64_t bit_idx = idx % 64; //Index of bit within the word

  if(lvl > map->max_lvl) {
    return -1;
  } else {
    return (map->map[word_idx] & (1ull << bit_idx));
  }
}

void map_set(int lvl, int lvl_idx, map_t *map, bool set)
{
  int32_t idx = LVL_TO_BIT(lvl, map->num_pg) + lvl_idx; //Literal bit index
  int32_t word_idx = idx / 64;  //Index of appropriate word in map
  int64_t bit_idx = idx % 64; //Index of bit within the word

  if(set) {
    map->map[word_idx] |= (1ull << bit_idx);
  } else {
    map->map[word_idx] &= ~(1ull << bit_idx);
  }

  return;
}

/* Find the next available slot at lvl which has it's buddy set.
 * Returns:
 *  Index of next available slot
 *  -1 if no slot is available (either no set buddy, or no free space)
 */
int map_next(int lvl, map_t *map)
{
  if(lvl == map->max_lvl) {
    return -map_check(lvl, 0, map);
  }

  for(int idx = 0; idx < LVL_NUM_PG(lvl, map->num_pg); idx += 2)
  {
    if(!map_check(lvl, idx, map) && map_check(lvl, idx + 1, map))
    {
      //idx is free, it's buddy is set, return idx
      return idx;
    }
    else if(map_check(lvl, idx, map) && !map_check(lvl, idx + 1, map))
    {
      //idx is set but it's buddy is free, return buddy
      return idx + 1;
    }
  }

  //No suitable slot found, return -1
  return -1;
}

/* Recursively splits the tree until a free slot is found
 * Returns:
 *  Index of slot to use at lvl - 1
 *  -1 if no slots available (tree full)
 */
int map_split(int lvl, map_t *map)
{
  int idx;

  idx = map_next(lvl, map);
  if(idx >= 0) {
#ifdef BUDDY_DEBUG
    printf("split: recursion stoped at lvl %d\n", lvl);
#endif
    map_set(lvl, idx, map, true);
    return CHILD(idx);
  } else {
    if(lvl == map->max_lvl) {
#ifdef BUDDY_DEBUG
      printf("split: recursion topped out, no free space\n");
#endif
      return -1;
    }
    idx = map_split(lvl + 1, map);
    if(idx >= 0) {
      map_set(lvl, idx, map, true);
      return CHILD(idx);
    } else {
      return -1;
    }
  }
}

void print_map(rvm_cfg_t *cfg)
{
  map_t *map = (map_t *)cfg->alloc_data;

  for(int i = 0; i < map->sz; i++)
  {
    printf("%lx ", map->map[i]);
  }
  printf("\n");
}

void buddy_memuse(rvm_cfg_t *cfg)
{
  uint32_t pg_sz;
  uint32_t used = 0;
  uint32_t used_prev = 0;
  uint32_t tot = 0;
  uint32_t amt_split; //the amount of this level this is allocated due to splitting

  map_t *map = (map_t *)cfg->alloc_data;

  printf("Pg_sz\t:\tnum_used\n");
  for(int lvl = 0; lvl <= map->max_lvl; lvl++)
  {
    for(int i = 0; i < LVL_NUM_PG(lvl, map->num_pg); i++)
    {
      used += map_check(lvl, i, map);
    }

    amt_split = !(used_prev % 2) ? used_prev / 2 : (used_prev + 1) / 2;
    tot += (used - amt_split) * LVL_PG_SZ(lvl, MIN_PG_SZ);

    printf("%d\t:\t%d\n", LVL_PG_SZ(lvl, MIN_PG_SZ), used - amt_split);
    used_prev = used;
    used = 0;
  }
  printf("Useful allocated space: %d\n", tot);
  printf("Total allocated space: %d\n\n", tot + map->sz * sizeof(uint64_t));
}
