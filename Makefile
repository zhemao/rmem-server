.PHONY: clean

CFLAGS  := -Wall -Werror -g
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread

APPS    := rmem-server

all: ${APPS}

rmem-server: common.o rmem-server.o
	${LD} -o $@ $^ ${LDFLAGS}

clean:
	rm -f *.o ${APPS}

