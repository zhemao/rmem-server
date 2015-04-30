INCLUDES = -I/nscratch/joao/ramcloud/src/ -I/nscratch/joao/ramcloud/ -I/nscratch/joao/ramcloud/obj.master 
LINCLUDES = -L/nscratch/joao/ramcloud/obj.master
CFLAGS = -std=c++0x
LIBS = -lramcloud -lboost_system -lboost_program_options
OBJ_PATH = /nscratch/joao/ramcloud/obj.master
RC_OBJS = ${OBJ_PATH}/OptionParser.o
CXX ?= g++
TARGETS = test ramc_transactions.o

all:$(TARGETS)

test: test.cpp ${RC_OBJS}
	${CXX} -o $@ ${CFLAGS} ${INCLUDES} ${LINCLUDES} ${LIBS} $^

ramc_transactions.o: ramc_transactions.cpp
	${CXX} -c -o $@ ${CFLAGS} ${INCLUDES} ${LINCLUDES} ${LIBS} $^

run:
	LD_LIBRARY_PATH=/nscratch/joao/ramcloud/obj/ ./test -C infrc:host=f1,port=11100

clean:
	rm -rf $(TARGETS)
