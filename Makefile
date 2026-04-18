# ─────────────────────────────────────────────────────────────────────────────
# Makefile — CloudPol DSL Compiler Build System
# Cloud Policy as Code DSL Compiler
#
# Targets:
#   make all          Build the full compiler (lexer + parser + AST)
#   make lexer        Build only the lexer (standalone test mode)
#   make lexer-test   Build lexer in standalone mode for token pretty-printing
#   make parser       Build lexer + parser (Step 3 — full AST)
#   make clean        Remove all generated files
# ─────────────────────────────────────────────────────────────────────────────

CC      = gcc
FLEX    = flex
BISON   = bison

CFLAGS  = -Wall -Wextra -g -I./include
LDFLAGS = -lfl

SRC_DIR     = src
BUILD_DIR   = build
INCLUDE_DIR = include

# Generated sources
LEXER_C     = $(BUILD_DIR)/lex.yy.c
PARSER_C    = $(BUILD_DIR)/policy_parser.tab.c
PARSER_H    = $(BUILD_DIR)/policy_parser.tab.h

# Hand-written sources
AST_C       = $(SRC_DIR)/ast.c
SEMANTIC_C  = $(SRC_DIR)/semantic.c

# Source specs
LEXER_L     = $(SRC_DIR)/policy_lexer.l
PARSER_Y    = $(SRC_DIR)/policy_parser.y

# Final binaries
COMPILER    = $(BUILD_DIR)/cloudpol
LEXER_BIN   = $(BUILD_DIR)/policy_lexer

# ─────────────────────────────────────────────────────────────────────────────
.PHONY: all lexer lexer-test parser clean dirs help
# ─────────────────────────────────────────────────────────────────────────────

all: dirs parser
	@echo ""
	@echo "✓  CloudPol compiler binary: $(COMPILER)"
	@echo "   Usage: ./$(COMPILER) samples/valid_policy.pol"

# ── Step 4: Build full compiler (parser + lexer + AST + semantic) ─────────────
parser: dirs $(PARSER_C) $(LEXER_C)
	$(CC) $(CFLAGS) -o $(COMPILER) $(PARSER_C) $(LEXER_C) $(AST_C) $(SEMANTIC_C) $(LDFLAGS)
	@echo "✓  Parser build complete → $(COMPILER)"

# ── Step 2: Standalone lexer (pretty-prints tokens, no parser needed) ─────────
# Flex is run separately here — no dependency on Bison's .tab.h
LEXER_STANDALONE_C = $(BUILD_DIR)/lex.standalone.c

lexer-test: dirs
	$(FLEX) -o $(LEXER_STANDALONE_C) $(LEXER_L)
	$(CC) -Wall -g -I./include -DLEXER_STANDALONE -o $(LEXER_BIN) $(LEXER_STANDALONE_C) $(LDFLAGS)
	@echo "Lexer test binary --> $(LEXER_BIN)"
	@echo "   Usage:  cat samples/valid_policy.pol | $(LEXER_BIN)"

lexer: lexer-test

# ── Generate C from Bison ──────────────────────────────────────────────────────
$(PARSER_C): dirs $(PARSER_Y)
	$(BISON) -d -o $(PARSER_C) $(PARSER_Y)
	@echo "✓  Bison generated $(PARSER_C) and $(PARSER_H)"
	@# Make generated header visible in src/ directory for Flex include
	cp $(PARSER_H) $(SRC_DIR)/policy_parser.tab.h

# ── Generate C from Flex ───────────────────────────────────────────────────────
$(LEXER_C): dirs $(PARSER_H) $(LEXER_L)
	$(FLEX) -o $(LEXER_C) $(LEXER_L)
	@echo "✓  Flex generated $(LEXER_C)"

# Flex needs the .tab.h in src/ to include it
$(PARSER_H): $(PARSER_C)

# ── Create build directory ─────────────────────────────────────────────────────
dirs:
	@mkdir -p $(BUILD_DIR)

# ── Cleanup ───────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(SRC_DIR)/policy_parser.tab.h
	@echo "✓  Cleaned build artifacts"

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo "CloudPol Compiler — Makefile targets"
	@echo "  make all          Build the full compiler (Step 3)"
	@echo "  make lexer-test   Build standalone lexer token printer (Step 2)"
	@echo "  make parser       Build lexer + full parser with AST (Step 3)"
	@echo "  make clean        Remove build artifacts"
