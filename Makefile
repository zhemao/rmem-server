.PHONY: clean

CC := gcc
CXX := g++
LD := g++
RMEM_LIBS := -lrdmacm -libverbs -lpthread 
INCLUDES := -I. -Idata -Iutils -Ibackends
RAMC_INCLUDES = -I/nscratch/joao/ramcloud/src/ -I/nscratch/joao/ramcloud/ -I/nscratch/joao/ramcloud/obj.master 
LINCLUDES = -L/nscratch/joao/ramcloud/obj.master
RAMC_LIBS = -lramcloud -lboost_system -lboost_program_options
CFLAGS  := -Wall -O3 -fPIC -std=gnu11 $(INCLUDES) 
CXXFLAGS := -Wall -O3 -fPIC -std=gnu++0x $(RAMC_INCLUDES) $(INCLUDES)
RAMC_OBJS := /nscratch/joao/ramcloud/obj.master/OptionParser.o

APPS    := rmem-server 
TESTS   := tests/rvm_test_normal_rc tests/rvm_test_normal tests/rvm_test_txn_commit tests/rvm_test_txn_commit_rc tests/rvm_test_recovery tests/rvm_test_recovery_rc tests/rvm_test_free tests/rvm_test_free_rc  tests/rvm_test_big_commit tests/rvm_test_size_alloc tests/rvm_test_full tests/rvm_test_full_rc

COMMON_FILES := common.o data/hash.o data/list.o data/stack.o
SERVER_FILES := rmem_table.o rmem_multi_ops.o $(COMMON_FILES)
CLIENT_FILES := rvm.o backends/rmem_backend.o backends/ramcloud_backend.o buddy_malloc.o $(COMMON_FILES)
RVM_LIB := -L. -lrvm

STATIC_LIB = librvm.a
TARGETS = $(APPS) $(TESTS)

all: $(TARGETS)

librvm.a: $(CLIENT_FILES)
	$(AR) rcs $@ $(CLIENT_FILES)

rmem-server: $(SERVER_FILES) rmem-server.o
	${LD} -o $@ $^ ${CFLAGS} ${RMEM_LIBS}

rmem-test: $(SERVER_FILES) rmem-test.o
	${LD} -o $@ $^ ${CFLAGS} ${RMEM_LIBS}

tests/rvm_test_normal: tests/rvm_test_normal.o $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_full: tests/rvm_test_full.o $(STATIC_LIB)
	${LD} -o $@ $^ $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_txn_commit: tests/rvm_test_txn_commit.o $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_recovery: tests/rvm_test_recovery.o $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_free: tests/rvm_test_free.o $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_big_commit: tests/rvm_test_big_commit.o $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_size_alloc: tests/rvm_test_size_alloc.o $(STATIC_LIB)
	${LD} -o $@ $< $(RVM_LIB) ${RMEM_LIBS}

tests/dgemv_test: tests/dgemv_test.c $(STATIC_LIB)
	${LD} -o $@ $< -lblas $(RVM_LIB) ${RMEM_LIBS}

tests/rvm_test_normal_rc: tests/rvm_test_normal_rc.o $(STATIC_LIB) $(RAMC_OBJS)
	$(LD) -o $@ $< $(RAMC_OBJS) $(RVM_LIB) $(LINCLUDES) $(RAMC_LIBS)

tests/rvm_test_full_rc: tests/rvm_test_full_rc.o $(STATIC_LIB) $(RAMC_OBJS)
	$(LD) -o $@ $< $(RAMC_OBJS) $(RVM_LIB) $(LINCLUDES) $(RAMC_LIBS)

tests/rvm_test_txn_commit_rc: tests/rvm_test_txn_commit_rc.o $(STATIC_LIB) $(RAMC_OBJS)
	$(LD) -o $@ $< $(RAMC_OBJS) $(RVM_LIB) $(LINCLUDES) $(RAMC_LIBS)

backends/ramcloud_backend.o: backends/ramcloud_backend.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(RAMC_INCLUDES)

tests/rvm_test_recovery_rc: tests/rvm_test_recovery_rc.o $(STATIC_LIB) $(RAMC_OBJS)
	$(LD) -o $@ $< $(RAMC_OBJS) $(RVM_LIB) $(LINCLUDES) $(RAMC_LIBS)

tests/rvm_test_free_rc: tests/rvm_test_free_rc.o $(STATIC_LIB) $(RAMC_OBJS)
	$(LD) -o $@ $< $(RAMC_OBJS) $(RVM_LIB) $(LINCLUDES) $(RAMC_LIBS)

clean:
	rm -f data/*.o tests/*.o backends/*.o *.o $(STATIC_LIB) $(TARGETS)

