.PHONY: clean

CFLAGS  := -Wall -g -fPIC -std=gnu11
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -std=gnu11
INCLUDE :=-I. -Idata -Iutils
RVM_LIB := -L. -lrvm

COMMON_FILES := common.o data/hash.o data/list.o data/stack.o
SERVER_FILES := rmem_table.o $(COMMON_FILES)
CLIENT_FILES := rvm.o rmem.o buddy_malloc.o $(COMMON_FILES)
APPS    := rmem-server rmem-test
TESTS   := tests/rvm_test_normal tests/rvm_test_txn_commit tests/rvm_test_recovery tests/rvm_test_free tests/rvm_test_big_commit tests/rvm_test_size_alloc tests/rvm_test_full
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

tests/dgemv_test: tests/dgemv_test.c $(STATIC_LIB)
	${LD} -o $@ $^ -lblas $(RVM_LIB) ${CFLAGS} ${LDFLAGS} ${INCLUDE} 

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

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CC) $(CFLAGS) -MM $^ > ./.depend;

clean:
	rm -f *.o ${APPS} ${TESTS} $(STATIC_LIBS)

