#ifndef TAG_ADDR_MAP_H
#define TAG_ADDR_MAP_H

#define TAG_ADDR_PAIR_SIZEOF sizeof(tag_addr_entry)
#define TAG_ADDR_MAP_SIZE_MSG 20

typedef struct tag_addr_entry {
    uint32_t tag;
    uintptr_t addr;
} tag_addr_entry_t;

#endif
