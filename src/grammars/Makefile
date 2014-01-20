ANTLR_LIB=$(HOME)/opt/antlr-3.4/lib
ANTLR=CLASSPATH=$(ANTLR_LIB)/antlr-3.4-complete.jar java org.antlr.Tool
GRAMMAR=config
TREE_GRAMMAR=configTree

INCLUDES=-I../deps/mempool -I$(HOME)/include -I..

grammars: $(GRAMMAR).tokens $(TREE_GRAMMAR).tokens

$(GRAMMAR).tokens: $(GRAMMAR).g
	$(ANTLR) $<

$(TREE_GRAMMAR).tokens: $(TREE_GRAMMAR).g $(GRAMMAR).tokens
	$(ANTLR) $<

$(TREE_GRAMMAR).o: $(TREE_GRAMMAR).c $(TREE_GRAMMAR).tokens
	gcc -c -g -w -I. $(INCLUDES) -L$(HOME)/lib -lantlr3c $< -o $@

%.o: %.c $(GRAMMAR).tokens
	gcc -w -c -I. -I$(HOME)/include $< -o $@

tags: *.c *.h
	ctags -R --verbose *.h *.c $(HOME)/include/antl*

.PHONY: clean grammars

clean:
	rm -rf $(GRAMMAR).tokens
	rm -rf $(GRAMMAR)*.h
	rm -rf $(GRAMMAR)*.c
	rm -rf *.o
	rm -rf gtest
