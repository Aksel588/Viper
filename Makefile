CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS =

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

COMMON_SRC = src/support/arena.c \
             src/support/diagnostics.c \
             src/support/discovery.c \
             src/support/builtins.c \
             src/support/module.c \
             src/support/paths.c \
             src/support/source.c \
             src/frontend/lexer.c \
             src/frontend/parser.c \
             src/frontend/token.c \
             src/frontend/ast.c \
             src/analysis/types.c \
             src/analysis/symbol_table.c \
             src/analysis/semantic.c \
             src/codegen/bytecode.c \
             src/vm/runtime.c \
             src/vm/vm.c

VIPER_SRC = src/driver/main.c $(COMMON_SRC)
VIPERRUN_SRC = src/driver/viperrun.c $(COMMON_SRC)

COMMON_OBJ = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
VIPER_OBJ = $(OBJ_DIR)/driver/main.o $(COMMON_OBJ)
VIPERRUN_OBJ = $(OBJ_DIR)/driver/viperrun.o $(COMMON_OBJ)

TEST_BIN = $(BIN_DIR)/test_symbol_table \
           $(BIN_DIR)/test_lexer \
           $(BIN_DIR)/test_semantic \
           $(BIN_DIR)/test_bytecode \
           $(BIN_DIR)/test_vm \
           $(BIN_DIR)/test_module

.PHONY: all clean test examples verify install install-user uninstall dirs

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
LIBDIR  ?= $(PREFIX)/lib/viper
DESTDIR ?=

all: dirs $(BIN_DIR)/viper $(BIN_DIR)/viperrun $(TEST_BIN)

dirs:
	@mkdir -p $(OBJ_DIR)/driver $(OBJ_DIR)/frontend $(OBJ_DIR)/analysis \
	          $(OBJ_DIR)/codegen $(OBJ_DIR)/vm $(OBJ_DIR)/support $(BIN_DIR)

$(BIN_DIR)/viper: $(VIPER_OBJ) | dirs
	$(CC) $(CFLAGS) -o $@ $(VIPER_OBJ) $(LDFLAGS)

$(BIN_DIR)/viperrun: $(VIPERRUN_OBJ) | dirs
	$(CC) $(CFLAGS) -o $@ $(VIPERRUN_OBJ) $(LDFLAGS)

$(OBJ_DIR)/%.o: src/%.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN_DIR)/test_symbol_table: tests/test_symbol_table.c src/analysis/symbol_table.c src/analysis/types.c | dirs
	$(CC) $(CFLAGS) -o $@ tests/test_symbol_table.c src/analysis/symbol_table.c src/analysis/types.c

$(BIN_DIR)/test_lexer: tests/test_lexer.c src/frontend/lexer.c src/frontend/token.c src/support/diagnostics.c src/support/source.c | dirs
	$(CC) $(CFLAGS) -o $@ tests/test_lexer.c src/frontend/lexer.c src/frontend/token.c src/support/diagnostics.c src/support/source.c

$(BIN_DIR)/test_semantic: tests/test_semantic.c \
	src/analysis/semantic.c src/analysis/symbol_table.c src/analysis/types.c \
	src/support/builtins.c src/support/arena.c src/support/discovery.c src/support/module.c \
	src/frontend/ast.c src/support/diagnostics.c src/support/source.c \
	src/frontend/lexer.c src/frontend/parser.c src/frontend/token.c | dirs
	$(CC) $(CFLAGS) -o $@ tests/test_semantic.c \
		src/analysis/semantic.c src/analysis/symbol_table.c src/analysis/types.c \
		src/support/builtins.c src/support/arena.c src/support/discovery.c src/support/module.c \
		src/frontend/ast.c src/support/diagnostics.c src/support/source.c \
		src/frontend/lexer.c src/frontend/parser.c src/frontend/token.c

$(BIN_DIR)/test_bytecode: tests/test_bytecode.c $(COMMON_SRC) | dirs
	$(CC) $(CFLAGS) -o $@ tests/test_bytecode.c $(COMMON_SRC)

$(BIN_DIR)/test_vm: tests/test_vm.c $(COMMON_SRC) | dirs
	$(CC) $(CFLAGS) -o $@ tests/test_vm.c $(COMMON_SRC)

$(BIN_DIR)/test_module: tests/test_module.c $(COMMON_SRC) | dirs
	$(CC) $(CFLAGS) -o $@ tests/test_module.c $(COMMON_SRC)

test: $(TEST_BIN)
	$(BIN_DIR)/test_symbol_table
	$(BIN_DIR)/test_lexer
	$(BIN_DIR)/test_semantic
	$(BIN_DIR)/test_bytecode
	$(BIN_DIR)/test_vm
	$(BIN_DIR)/test_module

examples: $(BIN_DIR)/viper
	$(BIN_DIR)/viper examples

verify: $(BIN_DIR)/viper
	$(BIN_DIR)/viper -p examples --run examples/globals.vp
	! $(BIN_DIR)/viper -p examples --run examples/bad.vp
	$(BIN_DIR)/viper -p examples -r --run examples/main.vp
	$(BIN_DIR)/viper --run examples/while.vp
	! $(BIN_DIR)/viper examples/typo.vp

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN_DIR)/viper $(DESTDIR)$(BINDIR)/viper
	install -m 755 $(BIN_DIR)/viperrun $(DESTDIR)$(BINDIR)/viperrun
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 lib/viper/*.vp $(DESTDIR)$(LIBDIR)/

install-user: all
	$(MAKE) install PREFIX=$(HOME)/.local

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/viper $(DESTDIR)$(BINDIR)/viperrun
	rm -rf $(DESTDIR)$(LIBDIR)

clean:
	rm -rf $(BUILD_DIR)
	rm -f viper viperrun tests/test_symbol_table tests/test_lexer tests/test_semantic tests/test_bytecode tests/test_vm

# Convenience symlinks at repo root (optional targets)
viper: $(BIN_DIR)/viper
	@ln -sf $(BIN_DIR)/viper viper

viperrun: $(BIN_DIR)/viperrun
	@ln -sf $(BIN_DIR)/viperrun viperrun
