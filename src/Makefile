CC=gcc
AR=ar
RM=rm -rf
ANTLR=antlr3

INCLUDEPATHS=-I$(HOME)/include -I. -I./deps/mempool -I./grammars
WARNFLAGS=-Wall -Wextra
CFLAGS=-g -O0 $(INCLUDEPATHS)

ANTLR_LIB=$(HOME)/opt/antlr-3.4/lib
LDFLAGS=-g -levent -L$(HOME)/lib -L$(ANTLR) -lantlr3c

TARGET=gateway
MAINFILE=oris_gateway.c

ANTLR_CP=/usr/share/antlr-3/lib/antlr3.jar

SOURCES=\
	oris_app_info.c \
	oris_automation.c \
	oris_configuration.c \
	oris_connection.c \
	oris_kvpair.c \
	oris_log.c \
	oris_protocol.c \
	oris_protocol_ctrl.c \
	oris_protocol_data.c \
	oris_socket_connection.c \
	oris_table.c \
	oris_util.c \
	deps/mempool/mem_pool.c \
	grammars/oris_interpret_tools.c 

OBJECTS=$(patsubst %.c,%.o,$(SOURCES))
HEADERS=$(patsubst %.c,%.h,$(SOURCES))

GRAMMARS_DIR=./grammars
GRAMMAR_SRC=\
	$(GRAMMARS_DIR)/configLexer.c \
	$(GRAMMARS_DIR)/configParser.c \
	$(GRAMMARS_DIR)/configTree.c

GRAMMAR_ARCHIVE=$(GRAMMARS_DIR)/config.ar

.PHONY: clean grammar

all: $(GRAMMAR_ARCHIVE) $(TARGET) 

%.o: %.c %.h
	@echo "CC    $<"
	@$(CC) $(WARNFLAGS) $(CFLAGS) -c $< -o $@ #>/dev/null

$(TARGET): $(MAINFILE) $(OBJECTS) $(HEADERS) $(GRAMMAR_ARCHIVE)
	$(CC) $(WARNFLAGS) $(CFLAGS) $(LDFLAGS) $< $(OBJECTS) $(GRAMMAR_ARCHIVE) -o $@

$(GRAMMAR_ARCHIVE): $(GRAMMARS_DIR)/*.g $(GRAMMAR_SRC)
	$(MAKE) -C $(GRAMMARS_DIR) grammars
	for FILE in $(GRAMMAR_SRC); do $(CC) $(CFLAGS) -c $$FILE -o `echo $$FILE | sed s/\.c$$/.o/`; done
	$(AR) cr $@ $(patsubst %.c,%.o,$^)

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJECTS) 
	$(RM) $(GRAMMAR_ARCHIVE)
	$(RM) tags

tags: $(SOURCES)
	ctags -RV *
