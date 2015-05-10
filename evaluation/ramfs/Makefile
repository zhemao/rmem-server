CC       = gcc
CFLAGS   = -Wall -std=gnu11 -ggdb
INCLUDES = -I/nscratch/joao/repos/rmem-server -I/nscratch/joao/repos/rmem-server/backends
LIBS     = -lrvm -lrdmacm -libverbs -lpthread
LFLAGS   = -L/nscratch/joao/repos/rmem-server
FUSE_LIB = `pkg-config fuse --cflags --libs`
TARGETS = ramfs

all: $(TARGETS)

ramfs: ramfs.h ramfs.c opt.c buddy.c
	$(CC) $^ -o $@ $(CFLAGS) $(FUSE_LIB) $(LIBS) $(LFLAGS) $(INCLUDES)

clean:
	rm -f $(TARGETS) *.o
