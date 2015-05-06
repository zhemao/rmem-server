.PHONY: clean

CC = g++
CFLAGS  := -Wall -g 
LD      := g++
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread 
INCLUDE :=-I. -Idata -Iutils -Ibackends
RAMC_INCLUDES = -I/nscratch/joao/ramcloud/src/ -I/nscratch/joao/ramcloud/ -I/nscratch/joao/ramcloud/obj.master 
LINCLUDES = -L/nscratch/joao/ramcloud/obj.master
RAMC_LIBS = -lramcloud -lboost_system -lboost_program_options

FILES   := utils/log.h common.o rmem_table.o rmem-server.o rvm.o backends/rmem_backend.o data/hash.o data/list.o data/stack.o
APPS    := rmem-server 
TESTS   := tests/rvm_test_normal_rc tests/rvm_test_normal tests/rvm_test_txn_commit tests/rvm_test_txn_commit_rc tests/rvm_test_recovery tests/rvm_test_recovery_rc tests/rvm_test_free tests/rvm_test_free_rc  tests/rvm_test_big_commit tests/rvm_test_size_alloc tests/rvm_test_full
COMMON_FILES := common.o data/hash.o data/list.o data/stack.o
SERVER_FILES := rmem_table.o $(COMMON_FILES)
CLIENT_FILES := rvm.o backends/rmem_backend.o backends/ramcloud_backend.o buddy_malloc.o $(COMMON_FILES)
RVM_LIB := -L. -lrvm

HEADERS := $(wildcard *.h)

STATIC_LIB = librvm.a
TARGETS = $(APPS) $(TESTS)

all: $(TARGETS)

include .depend

librvm.a: $(CLIENT_FILES)
	$(AR) rcs $@ $(CLIENT_FILES)

rmem-server: $(SERVER_FILES) rmem-server.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

rmem-test: $(SERVER_FILES) rmem-test.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_normal: tests/rvm_test_normal.c $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_full: tests/rvm_test_full.c $(STATIC_LIB)
	${LD} -o $@ $^ $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_txn_commit: tests/rvm_test_txn_commit.c $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_recovery: tests/rvm_test_recovery.c $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_free: tests/rvm_test_free.c $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_big_commit: tests/rvm_test_big_commit.c $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_size_alloc: tests/rvm_test_size_alloc.c $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/dgemv_test: tests/dgemv_test.c backends/rmem_backend.o rvm.o rmem_table.o common.o data/hash.o data/list.o
	${LD} -o $@ $^ -lblas ${LDFLAGS} ${INCLUDE} 

tests/rvm_test_normal_rc: tests/rvm_test_normal_rc.c backends/ramcloud_backend.cpp rvm.o common.o data/hash.o data/list.o /nscratch/joao/ramcloud/obj.master/OptionParser.o
	$(LD) -o $@ $^ -ggdb $(LDFLAGS)  ${INCLUDE}  $(LINCLUDES) $(RAMC_LIBS) $(RAMC_INCLUDES)  -std=gnu++0x

tests/rvm_test_txn_commit_rc: tests/rvm_test_txn_commit_rc.c backends/ramcloud_backend.cpp rvm.o common.o data/hash.o data/list.o /nscratch/joao/ramcloud/obj.master/OptionParser.o
	$(LD) -o $@ $^ -ggdb $(LDFLAGS)  ${INCLUDE}  $(LINCLUDES) $(RAMC_LIBS) $(RAMC_INCLUDES)  -std=gnu++0x

backends/ramcloud_backend.o: backends/ramcloud_backend.cpp
	$(LD) -c -o $@ $^ ${INCLUDE} $(RAMC_INCLUDES) $(LINCLUDES) $(RAMC_LIBS) -std=gnu++0x

tests/rvm_test_recovery_rc: tests/rvm_test_recovery_rc.c backends/ramcloud_backend.o rvm.o rmem_table.o common.o data/hash.o data/list.o /nscratch/joao/ramcloud/obj.master/OptionParser.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE} $(RAMC_INCLUDES) $(LINCLUDES) $(RAMC_LIBS) -std=gnu++0x

tests/rvm_test_free_rc: tests/rvm_test_free_rc.c backends/ramcloud_backend.o rvm.o rmem_table.o common.o data/hash.o data/list.o /nscratch/joao/ramcloud/obj.master/OptionParser.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE} $(RAMC_INCLUDES) $(LINCLUDES) $(RAMC_LIBS) -std=gnu++0x

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CC) $(CFLAGS) -MM $^ > ./.depend;

clean:
	rm -f data/*.o backends/*.o *.o $(TARGETS)

