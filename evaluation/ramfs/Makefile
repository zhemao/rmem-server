CC       = gcc
CFLAGS   = -g -Wall
INCLUDES = -I/nscratch/joao/repos/rmem-server -I/nscratch/joao/repos/rmem-server/backends
LIBS     = -lrvm -lrdmacm -libverbs -lpthread
LFLAGS   = -L/nscratch/joao/repos/rmem-server
FUSE_LIB = `pkg-config fuse --cflags --libs`

all: ramfs

ramfs: ramfs.h ramfs.c opt.c
	$(CC) $^ -o $@ $(CFLAGS) $(FUSE_LIB) $(LIBS) $(LFLAGS) $(INCLUDES)

prepare: all
	mkdir -p test
	fusermount -u test

test: prepare
	./ramfs -s test

clean:
	rm -f ramfs
	fusermount -u test
