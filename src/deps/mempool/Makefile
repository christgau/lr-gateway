GCC = gcc
FLAG = -g
OBJ = test.o mem_pool.o

all: test

test: $(OBJ)
	$(GCC) $(OBJ) -o $@ $(FLAG)

test.o: test.c
	$(GCC) $< -c -o $@ $(FLAG)

mem_pool.o: mem_pool.c mem_pool.h
	$(GCC) $< -c -o $@ $(FLAG)

clean:
	rm -f test $(OBJ)
