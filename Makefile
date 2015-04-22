.PHONY: clean

CFLAGS  := -Wall -g -fPIC 
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread
INCLUDE :=-I.

FILES   := log.h common.o rmem_table.o rmem-server.o rvm.o rmem.o
APPS    := rmem-server rmem-client rmem-test rvm_test dgemv_test
TESTS   := tests/rvm_test tests/rvm_test_txn_commit tests/rvm_test_recovery tests/rvm_test_free

all: ${APPS} ${TESTS}

rmem-server: ${FILES}
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

rmem-client: ${FILES}
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

rmem-test: ${FILES}
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

dgemv_test: dgemv_test.o
	${LD} -o $@ $^ -lblas

tests/rvm_test: tests/rvm_test.c rmem.o rvm.o rmem_table.o common.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_txn_commit: tests/rvm_test_txn_commit.c rmem.o rvm.o rmem_table.o common.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_recovery: tests/rvm_test_recovery.c rmem.o rvm.o rmem_table.o common.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

tests/rvm_test_free: tests/rvm_test_free.c rmem.o rvm.o rmem_table.o common.o
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS} ${INCLUDE}

clean:
	rm -f *.o ${APPS} ${TESTS}

