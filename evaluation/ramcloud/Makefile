INCLUDES = -I/nscratch/joao/ramcloud/src/ -I/nscratch/joao/ramcloud/ -I/nscratch/joao/ramcloud/obj.master 
LINCLUDES = -L/nscratch/joao/ramcloud/obj.master
CFLAGS = -std=c++0x
LIBS = -lramcloud -lboost_system -lboost_program_options
OBJ_PATH = /nscratch/joao/ramcloud/obj.master
RC_OBJS = ${OBJ_PATH}/OptionParser.o
CXX ?= g++

all:test

test: test.cpp ${RC_OBJS}
	${CXX} -o test ${CFLAGS} ${INCLUDES} ${LINCLUDES} ${LIBS} $^

run:
	LD_LIBRARY_PATH=/nscratch/joao/ramcloud/obj/ ./test -C infrc:host=f1,port=11100

clean:
	rm -rf test
