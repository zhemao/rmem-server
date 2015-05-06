RVM_DIR = ../../
CC = gcc

#CFLAGS 	= -O3
CFLAGS 	= -g -std=gnu11
LIBS	= -lblas
TARGETS	= dgemv dgemv_rvm dgemv_file
RVM_LIB = $(RVM_DIR)librvm.a

all: 	$(TARGETS)

dgemv: dgemv.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

dgemv_rvm: dgemv_rvm.o
	$(CC) $(CFLAGS) $(LIBS) $(RVM_LIB) -I$(RVM_DIR) -o $@ $^
	
dgemv_file: dgemv_file.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

#C Objects (Some false deps on headers, oh well)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f *.o
	rm -rf $(TARGETS)
