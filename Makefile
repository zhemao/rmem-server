.PHONY: clean

CFLAGS  := -Wall -g
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread

APPS    := rmem-server rmem-client

all: ${APPS}

rmem-server: common.o rmem_table.o rmem-server.o
	${LD} -o $@ $^ ${LDFLAGS}

rmem-client: common.o rmem_table.o rmem-client.o
	${LD} -o $@ $^ ${LDFLAGS}

rmem-test: common.o rmem_table.o rmem-test.o
	${LD} -o $@ $^ ${LDFLAGS}

clean:
	rm -f *.o ${APPS}

