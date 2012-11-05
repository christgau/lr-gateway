CC=gcc
RM=rm -rf

INCLUDEPATHS=-I$(HOME)/include -I.
LDFLAGS=-g -levent -lyaml -L$(HOME)/lib
CFLAGS=-g -Wall -pedantic -O0 $(INCLUDEPATHS)

TARGET=gateway
MAINFILE=oris_gateway.c
HEADERS=*.h

SOURCES=\
	oris_connection.c \
	oris_configuration.c \
	oris_protocol.c \
	oris_protocol_ctrl.c \
	oris_protocol_data.c \
	oris_socket_connection.c \
	oris_log.c \
	oris_util.c

OBJECTS=$(patsubst %.c,%.o,$(SOURCES))


.PHONY: clean

$(TARGET): $(MAINFILE) $(OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(OBJECTS) -o $@

%.o: %.c %.h
	@echo "CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@ #>/dev/null

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJECTS) 
	$(RM) tags

tags: $(SOURCES)
	ctags -RV *
