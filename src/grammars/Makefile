ANTLR_LIB=$(HOME)/src/antlr-3.4/lib
ANTLR=CLASSPATH=$(ANTLR_LIB)/antlr-3.4-complete.jar java org.antlr.Tool
GRAMMAR=config
TREE_GRAMMAR=configTree

INCLUDES=-I../deps/mempool -I$(HOME)/include -I..

all: $(GRAMMAR).tokens gtest

$(GRAMMAR).tokens: $(GRAMMAR).g
	$(ANTLR) $<

$(TREE_GRAMMAR).tokens: $(TREE_GRAMMAR).g
	$(ANTLR) $<

gtest: $(GRAMMAR)Lexer.o $(GRAMMAR)Parser.o gtest.c ../deps/mempool/mem_pool.c \
	$(TREE_GRAMMAR).o oris_interpret_tools.c ../oris_util.c ../oris_kvpair.c 
	# ../oris_configuration.c
	gcc -g -Wall -I. $(INCLUDES) -L$(HOME)/lib -lantlr3c $^ -o $@

$(TREE_GRAMMAR).o: $(TREE_GRAMMAR).c $(TREE_GRAMMAR).tokens
	gcc -c -g -w -I. $(INCLUDES) -L$(HOME)/lib -lantlr3c $< -o $@

%.o: %.c $(GRAMMAR).tokens
	gcc -w -c -I. -I$(HOME)/include $< -o $@

tags: *.c *.h
	ctags -R --verbose *.h *.c $(HOME)/include/antl*

.PHONY: clean grammars

grammars: $(GRAMMAR).tokens $(TREE_GRAMMAR).tokens

clean:
	rm -rf $(GRAMMAR).tokens
	rm -rf $(GRAMMAR)*.h
	rm -rf $(GRAMMAR)*.c
	rm -rf *.o
	rm -rf gtest
