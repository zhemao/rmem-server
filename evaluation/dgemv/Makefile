RVM_DIR = ../../
CC = gcc

#CFLAGS 	= -O3
CFLAGS 	= -g -std=gnu11
LIBS	= -lblas -lrdmacm -libverbs -lpthread
TARGETS	= dgemv dgemv_rvm dgemv_file
RVM_LIB = $(RVM_DIR)librvm.a
INCLUDE = -I$(RVM_DIR)

all: 	$(TARGETS)

dgemv: dgemv.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

dgemv_rvm: dgemv_rvm.o
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $^ $(RVM_LIB) $(LIBS)
	
dgemv_file: dgemv_file.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

#C Objects (Some false deps on headers, oh well)
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

clean :
	rm -f *.o
	rm -rf $(TARGETS)
	rm *.rec
