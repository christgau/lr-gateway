RM=rm -rf
PREFIX?=$(HOME)

INCLUDEPATHS=-I$(PREFIX)/include -I. -I./deps/mempool -I./grammars
WARNFLAGS=-Wall -Wextra
CFLAGS=-g -O0 -std=c99 -D_GNU_SOURCE $(INCLUDEPATHS) $(WARNFLAGS)

LDFLAGS=-g -lz -levent -levent_openssl -lantlr3c -L$(PREFIX)/lib -lssl -lcrypto

TARGET=gateway
MAINFILE=oris_gateway.c

SOURCES=\
	oris_app_info.c \
	oris_automation.c \
	oris_configuration.c \
	oris_connection.c \
	oris_http.c \
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

OBJECTS=$(SOURCES:.c=.o)
HEADERS=$(SOURCES:.c=.h)

GRAMMARS_DIR=./grammars
GRAMMAR_SRC=\
	$(GRAMMARS_DIR)/configLexer.c \
	$(GRAMMARS_DIR)/configParser.c \
	$(GRAMMARS_DIR)/configTree.c

GRAMMAR_ARCHIVE=$(GRAMMARS_DIR)/config.ar

.PHONY: clean check check-clean memcheck install uninstall

all: $(GRAMMAR_ARCHIVE) $(TARGET)

%.o: %.c %.h
	@echo "CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@ #>/dev/null

$(TARGET): $(MAINFILE) $(OBJECTS) $(HEADERS) $(GRAMMAR_ARCHIVE)
	@echo "CCLD  $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $(MAINFILE) $(OBJECTS) $(GRAMMAR_ARCHIVE) -o $@

grammars: $(GRAMMARS_DIR)/*.g
	$(MAKE) -C $(GRAMMARS_DIR) grammars

$(GRAMMAR_ARCHIVE): $(GRAMMAR_SRC)
	# $(MAKE) $(patsubst %.c,%.o,$(GRAMMAR_SRC))
	#for FILE in $(GRAMMAR_SRC); do $(MAKE) `echo $$FILE | sed s/\.c$$/.o/`; done
	for FILE in $(GRAMMAR_SRC); do $(CC) $(CFLAGS) -c $$FILE -o `echo $$FILE | sed s/\.c$$/.o/`; done
	$(AR) cr $@ $(GRAMMAR_SRC:.c=.o)

check:
	find . -name '*.c' -and -not -name 'config*' -print0 | xargs -0 -I% clang-check -analyze % -- $(INCLUDEPATHS)

check-clean:
	rm *.plist

memcheck: $(TARGET)
	valgrind --tool=memcheck --leak-check=full --show-reachable=yes --log-file=$@ ./$(TARGET) --verbose; true
	less -S $@

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJECTS)
	$(RM) $(GRAMMAR_ARCHIVE)
	$(RM) tags

install: $(TARGET)
	install -m 0755 $(TARGET) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

tags: $(SOURCES)
	ctags -RV *
