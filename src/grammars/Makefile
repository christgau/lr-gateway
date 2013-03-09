ANTLR_LIB=$(HOME)/opt/antlr-3.4/lib
ANTLR=CLASSPATH=$(ANTLR_LIB)/antlr-3.4-complete.jar java org.antlr.Tool
GRAMMAR=oris_automation

all: $(GRAMMAR).tokens gtest

$(GRAMMAR).tokens: $(GRAMMAR).g
	$(ANTLR) $<

gtest: $(GRAMMAR)Lexer.o $(GRAMMAR)Parser.o gtest.c
	gcc -Wall -I. -I$(HOME)/include -L$(HOME)/lib -lantlr3c $^ -o $@

%.o: %.c $(GRAMMAR).tokens
	gcc -c -I. -I$(HOME)/include $< -o $@

tags: *.c *.h
	ctags -R --verbose *.h *.c $(HOME)/include/antl*

.PHONY: clean

clean:
	rm -rf $(GRAMMAR).tokens
	rm -rf $(GRAMMAR)*.h
	rm -rf $(GRAMMAR)*.c
	rm -rf *.o
	rm -rf gtest
