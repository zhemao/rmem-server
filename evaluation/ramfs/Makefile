CC       = g++
CFLAGS   = -Wall -ggdb 
#-std=gnu11
INCLUDES = -I/nscratch/joao/repos/rmem-server -I/nscratch/joao/repos/rmem-server/backends
LIBS     = -lrvm -lrdmacm -libverbs -lpthread
LFLAGS   = -L/nscratch/joao/repos/rmem-server
FUSE_LIB = `pkg-config fuse --cflags --libs`

RAMC_INCLUDES = -I/nscratch/joao/ramcloud/src/ -I/nscratch/joao/ramcloud/ -I/nscratch/joao/ramcloud/obj.master 
LINCLUDES = -L/nscratch/joao/ramcloud/obj.master
RAMC_LIBS = -lramcloud -lboost_system -lboost_program_options
RAMC_OBJS := /nscratch/joao/ramcloud/obj.master/OptionParser.o

TARGETS = ramfs

all: $(TARGETS)

ramfs: ramfs.h ramfs.c opt.c buddy.c $(RAMC_OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(FUSE_LIB) $(LIBS) $(LFLAGS) $(INCLUDES) $(RAMC_LIBS) $(RAMC_INCLUDES) $(LINCLUDES)

kill:
	kill -9 `ps aux | grep ./ramfs | cut -b10-15 | head -n1`

clean:
	rm -f $(TARGETS) *.o
