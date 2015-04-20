.PHONY: clean

CFLAGS   := -g -O3 -std=c11
CC       := gcc
INCLUDES := -Idata
LDFLAGS  := ${LDFLAGS} 

all: log

log:log.o data/list.o data/hash.o
	${CC} ${INCLUDES} -o $@ $^ ${CFLAGS} 

#%.o:%.c data/list.c data/hash.c
#	${CC} -o $@ $^ ${CFLAGS} ${INCLUDES}

clean:
	rm -f *.o ${APPS}

