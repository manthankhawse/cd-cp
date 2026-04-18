# ─────────────────────────────────────────────────────────────────────────────
# Makefile — CloudPol DSL Compiler Build System
# Cloud Policy as Code DSL Compiler
#
# Targets:
#   make all              Build the full compiler (all phases)
#   make clean            Remove all generated files
#   make lexer-test       Build standalone lexer token printer
#   make run              Run compiler on samples/valid_policy.pol
#   make emit-mlir        Emit MLIR dialect IR for valid_policy.pol
#   make emit-iam         Emit AWS IAM JSON for valid_policy.pol
#   make emit-report      Emit Markdown report for valid_policy.pol
#   make test-semantic    Run semantic error test
#   make help             Show this list
# ─────────────────────────────────────────────────────────────────────────────

CC      = gcc
FLEX    = flex
BISON   = bison

CFLAGS  = -Wall -Wextra -g -I./include
LDFLAGS = -lfl

SRC_DIR     = src
BUILD_DIR   = build
INCLUDE_DIR = include

# ── Generated sources ─────────────────────────────────────────────────────────
LEXER_C     = $(BUILD_DIR)/lex.yy.c
PARSER_C    = $(BUILD_DIR)/policy_parser.tab.c
PARSER_H    = $(BUILD_DIR)/policy_parser.tab.h

# ── Hand-written sources ──────────────────────────────────────────────────────
AST_C         = $(SRC_DIR)/ast.c
SEMANTIC_C    = $(SRC_DIR)/semantic.c
MLIR_BRIDGE_C = $(SRC_DIR)/mlir_bridge.c
MLIR_PASSES_C = $(SRC_DIR)/mlir_passes.c
CODEGEN_C     = $(SRC_DIR)/codegen.c
CLI_C         = $(SRC_DIR)/cli.c

ALL_HAND_C = $(AST_C) $(SEMANTIC_C) \
             $(MLIR_BRIDGE_C) $(MLIR_PASSES_C) \
             $(CODEGEN_C) $(CLI_C)

# ── Source specs ──────────────────────────────────────────────────────────────
LEXER_L  = $(SRC_DIR)/policy_lexer.l
PARSER_Y = $(SRC_DIR)/policy_parser.y

# ── Final binaries ────────────────────────────────────────────────────────────
COMPILER  = $(BUILD_DIR)/cloudpol
LEXER_BIN = $(BUILD_DIR)/policy_lexer

# Sample policy
VALID_POL = samples/valid_policy.pol
SEM_POL   = samples/semantic_errors.pol

# ─────────────────────────────────────────────────────────────────────────────
.PHONY: all lexer lexer-test clean dirs help \
        run emit-mlir emit-iam emit-report test-semantic
# ─────────────────────────────────────────────────────────────────────────────

all: dirs $(COMPILER)
	@echo ""
	@echo "✓  CloudPol compiler → $(COMPILER)"
	@echo "   Try: ./$(COMPILER) --help"

# ── Full compiler (all 7 phases) ──────────────────────────────────────────────
$(COMPILER): dirs $(PARSER_C) $(LEXER_C) $(ALL_HAND_C)
	$(CC) $(CFLAGS) -o $@ $(PARSER_C) $(LEXER_C) $(ALL_HAND_C) $(LDFLAGS)
	@echo "✓  Build complete → $@"

# ── Generate C from Bison ─────────────────────────────────────────────────────
$(PARSER_C): dirs $(PARSER_Y)
	$(BISON) -d -o $(PARSER_C) $(PARSER_Y)
	cp $(PARSER_H) $(SRC_DIR)/policy_parser.tab.h
	@echo "✓  Bison generated $(PARSER_C)"

# ── Generate C from Flex ──────────────────────────────────────────────────────
$(LEXER_C): dirs $(PARSER_H) $(LEXER_L)
	$(FLEX) -o $(LEXER_C) $(LEXER_L)
	@echo "✓  Flex generated $(LEXER_C)"

$(PARSER_H): $(PARSER_C)

# ── Standalone lexer (Step 2 token printer) ───────────────────────────────────
LEXER_STANDALONE_C = $(BUILD_DIR)/lex.standalone.c

lexer-test: dirs
	$(FLEX) -o $(LEXER_STANDALONE_C) $(LEXER_L)
	$(CC) -Wall -g -I./include -DLEXER_STANDALONE \
	      -o $(LEXER_BIN) $(LEXER_STANDALONE_C) $(LDFLAGS)
	@echo "✓  Lexer test binary → $(LEXER_BIN)"
	@echo "   Usage: cat $(VALID_POL) | $(LEXER_BIN)"

lexer: lexer-test

# ── Create build directory ────────────────────────────────────────────────────
dirs:
	@mkdir -p $(BUILD_DIR)

# ── Cleanup ───────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(SRC_DIR)/policy_parser.tab.h
	@echo "✓  Cleaned build artifacts"

# ─────────────────────────────────────────────────────────────────────────────
# Convenience run targets
# ─────────────────────────────────────────────────────────────────────────────

run: all
	@echo ""
	./$(COMPILER) $(VALID_POL)

emit-mlir: all
	@echo ""
	./$(COMPILER) --emit-mlir $(VALID_POL)

emit-iam: all
	@echo ""
	./$(COMPILER) --emit-iam $(VALID_POL)

emit-report: all
	@echo ""
	./$(COMPILER) --emit-report $(VALID_POL)

test-semantic: all
	@echo ""
	./$(COMPILER) $(SEM_POL) 2>&1 || true

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo "CloudPol Compiler — Makefile targets"
	@echo ""
	@echo "  make all            Build the full compiler"
	@echo "  make clean          Remove build artifacts"
	@echo "  make lexer-test     Build standalone lexer token printer"
	@echo "  make run            Run compiler on samples/valid_policy.pol"
	@echo "  make emit-mlir      Print MLIR dialect IR"
	@echo "  make emit-iam       Print AWS IAM JSON"
	@echo "  make emit-report    Print Markdown policy report"
	@echo "  make test-semantic  Run semantic error test"
	@echo ""
	@echo "  ./build/cloudpol --help   Full CLI reference"
