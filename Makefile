.PHONY: clean

CFLAGS  := -Wall -g -fPIC 
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread
INCLUDE :=-I. -Idata

FILES   := log.h common.o rmem_table.o rmem-server.o rvm.o rmem.o data/hash.o data/list.o
APPS    := rmem-server rmem-client 
TESTS   := tests/rvm_test_normal tests/rvm_test_txn_commit tests/rvm_test_recovery tests/rvm_test_free dgemv_test

all: ${APPS} ${TESTS}

rmem-server: ${FILES}
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

rmem-client: ${FILES}
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

rmem-test: ${FILES}
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

dgemv_test: dgemv_test.o
	${LD} -o $@ $^ -lblas

tests/rvm_test_normal: tests/rvm_test_normal.c rmem.o rvm.o rmem_table.o common.o data/hash.o data/list.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_txn_commit: tests/rvm_test_txn_commit.c rmem.o rvm.o rmem_table.o common.o data/hash.o data/list.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_recovery: tests/rvm_test_recovery.c rmem.o rvm.o rmem_table.o common.o data/hash.o data/list.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_free: tests/rvm_test_free.c rmem.o rvm.o rmem_table.o common.o data/hash.o data/list.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}


clean:
	rm -f *.o ${APPS} ${TESTS}

