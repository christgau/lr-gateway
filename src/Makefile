CC=gcc
RM=rm -rf
ANTLR=antlr3

INCLUDEPATHS=-I$(HOME)/include -I.
LDFLAGS=-g -levent -lyaml -L$(HOME)/lib
CFLAGS=-g -Wall -pedantic -O0 $(INCLUDEPATHS)

TARGET=gateway
MAINFILE=oris_gateway.c
HEADERS=*.h

ANTLR_CP=/usr/share/antlr-3/lib/antlr3.jar

SOURCES=\
	oris_connection.c \
	oris_configuration.c \
	oris_protocol.c \
	oris_protocol_ctrl.c \
	oris_protocol_data.c \
	oris_socket_connection.c \
	oris_table.c \
	oris_log.c \
	oris_util.c

OBJECTS=$(patsubst %.c,%.o,$(SOURCES))


.PHONY: clean grammar

all: $(TARGET) 
#grammar

$(TARGET): $(MAINFILE) $(OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(OBJECTS) -o $@

%.o: %.c %.h
	@echo "CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@ #>/dev/null

grammar: grammars/oris_automation.g
	CLASSPATH=$(ANTLR_CP) $(ANTLR) $< 


clean:
	$(RM) $(TARGET)
	$(RM) $(OBJECTS) 
	$(RM) tags

tags: $(SOURCES)
	ctags -RV *
