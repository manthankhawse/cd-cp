/**
 * cli.c — CloudPol Compiler CLI Driver
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 7: Parses CLI arguments and drives the full compiler pipeline:
 *
 *   1. Lex + Parse (Bison/Flex)         → ProgramNode AST
 *   2. Semantic Analysis                → SemanticResult
 *   3. MLIR Bridge                      → CloudPolModule IR
 *   4. MLIR Passes                      → optimized IR
 *   5. Code Generation (--emit-*)       → IAM JSON / MLIR text / report
 *
 * Usage:
 *   cloudpol [options] [input.pol]
 *
 * Options:
 *   --emit-ast      Print typed AST (default: always printed)
 *   --emit-mlir     Print CloudPol MLIR dialect IR
 *   --emit-iam      Emit AWS IAM policy JSON
 *   --emit-report   Emit human-readable Markdown report
 *   -o <file>       Write primary codegen output to <file> (default: stdout)
 *   --no-semantic   Skip semantic analysis
 *   --no-passes     Skip MLIR optimization passes
 *   --help          Show this message
 *   --version       Print version
 *
 * Exit codes:
 *   0   Success
 *   1   Syntax / parse failure
 *   2   Semantic failure
 *   3   Codegen / I/O failure
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cli.h"
#include "../include/ast.h"
#include "../include/semantic.h"
#include "../include/mlir.h"
#include "../include/mlir_passes.h"
#include "../include/codegen.h"

/* build_mlir_module is implemented in mlir_bridge.c */
CloudPolModule *build_mlir_module(const ProgramNode *prog,
                                  const char *source_file);

#define CLOUDPOL_VERSION "1.0.0"

/* ─────────────────────────────────────────────
   External symbols from Bison/Flex
───────────────────────────────────────────── */
extern int   yyparse(void);
extern int   yylineno;
extern FILE *yyin;

/* Global parser state (defined in policy_parser.y via bison) */
extern ProgramNode *g_program;
extern int          g_errors;

/* ─────────────────────────────────────────────
   print_help
───────────────────────────────────────────── */
static void print_help(const char *argv0) {
    printf("CloudPol Compiler v%s — Cloud Policy as Code DSL\n\n", CLOUDPOL_VERSION);
    printf("Usage:\n");
    printf("  %s [options] [input.pol]\n\n", argv0);
    printf("Options:\n");
    printf("  --emit-ast      Print Abstract Syntax Tree (default on)\n");
    printf("  --emit-mlir     Print MLIR dialect IR after bridge + passes\n");
    printf("  --emit-iam      Emit AWS IAM policy JSON to output\n");
    printf("  --emit-report   Emit Markdown policy report to output\n");
    printf("  -o <file>       Write --emit-iam / --emit-report / --emit-mlir output to file\n");
    printf("  --no-semantic   Skip semantic analysis (Phase 3)\n");
    printf("  --no-passes     Skip MLIR optimization passes (Phase 5)\n");
    printf("  --help          Show this message and exit\n");
    printf("  --version       Print version and exit\n\n");
    printf("Examples:\n");
    printf("  %s samples/valid_policy.pol\n", argv0);
    printf("  %s --emit-mlir samples/valid_policy.pol\n", argv0);
    printf("  %s --emit-iam -o policy.json samples/valid_policy.pol\n", argv0);
    printf("  %s --emit-report samples/valid_policy.pol\n", argv0);
    printf("\nExit codes: 0=ok  1=syntax error  2=semantic error  3=codegen error\n");
}

/* ─────────────────────────────────────────────
   parse_cli
───────────────────────────────────────────── */
CliOptions parse_cli(int argc, char *argv[]) {
    CliOptions opts = {
        .input_file  = NULL,
        .output_file = NULL,
        .emit_flags  = EMIT_AST,  /* AST always shown unless overridden */
        .no_semantic = 0,
        .no_passes   = 0
    };

    int any_emit_explicit = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0 ||
                   strcmp(argv[i], "-v") == 0) {
            printf("cloudpol %s\n", CLOUDPOL_VERSION);
            exit(0);
        } else if (strcmp(argv[i], "--emit-ast") == 0) {
            opts.emit_flags |= EMIT_AST;
            any_emit_explicit = 1;
        } else if (strcmp(argv[i], "--emit-mlir") == 0) {
            opts.emit_flags |= EMIT_MLIR;
            any_emit_explicit = 1;
        } else if (strcmp(argv[i], "--emit-iam") == 0) {
            opts.emit_flags |= EMIT_IAM;
            any_emit_explicit = 1;
        } else if (strcmp(argv[i], "--emit-report") == 0) {
            opts.emit_flags |= EMIT_REPORT;
            any_emit_explicit = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[CloudPol] Error: -o requires a filename.\n");
                exit(3);
            }
            opts.output_file = argv[++i];
        } else if (strcmp(argv[i], "--no-semantic") == 0) {
            opts.no_semantic = 1;
        } else if (strcmp(argv[i], "--no-passes") == 0) {
            opts.no_passes = 1;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "[CloudPol] Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
            exit(1);
        } else {
            if (opts.input_file) {
                fprintf(stderr,
                    "[CloudPol] Error: multiple input files specified.\n");
                exit(1);
            }
            opts.input_file = argv[i];
        }
    }
    (void)any_emit_explicit;
    return opts;
}

/* ─────────────────────────────────────────────
   run_compiler — full pipeline
───────────────────────────────────────────── */
int run_compiler(CliOptions opts) {

    /* ── Phase 1: Open input ─────────────────────────────────── */
    if (opts.input_file) {
        yyin = fopen(opts.input_file, "r");
        if (!yyin) {
            fprintf(stderr, "[CloudPol] Error: cannot open '%s'\n",
                    opts.input_file);
            return 1;
        }
    }

    /* ── Phase 1: Lex + Parse ────────────────────────────────── */
    printf("[CloudPol] Phase 1: Lexical & Syntax Analysis...\n");
    int rc = yyparse();
    if (opts.input_file) fclose(yyin);

    if (g_errors > 0 || rc != 0) {
        fprintf(stderr,
            "[CloudPol] Parse FAILED — %d error(s).\n", g_errors);
        if (g_program) free_program_node(g_program);
        return 1;
    }

    /* Reverse statement list to source order */
    if (g_program && g_program->statements) {
        StatementNode *prev = NULL, *cur = g_program->statements, *nxt;
        while (cur) { nxt = cur->next; cur->next = prev; prev = cur; cur = nxt; }
        g_program->statements = prev;
    }

    printf("[CloudPol] Parse SUCCESSFUL — %d statement(s).\n",
           g_program ? g_program->count : 0);

    /* Emit AST if requested */
    if (opts.emit_flags & EMIT_AST)
        if (g_program) print_program_node(g_program);

    /* ── Phase 2: Semantic Analysis ─────────────────────────── */
    if (!opts.no_semantic) {
        printf("[CloudPol] Phase 2: Semantic Analysis...\n");
        SemanticResult *sem = analyze_program(g_program);
        print_semantic_result(sem);
        int sem_errors = sem ? sem->count : 0;
        free_semantic_result(sem);
        if (sem_errors > 0) {
            fprintf(stderr,
                "[CloudPol] FAILED — %d semantic error(s).\n", sem_errors);
            if (g_program) free_program_node(g_program);
            return 2;
        }
    } else {
        printf("[CloudPol] Phase 2: Semantic Analysis SKIPPED (--no-semantic).\n");
    }

    /* ── Phase 3: MLIR Bridge ───────────────────────────────── */
    printf("[CloudPol] Phase 3: MLIR Bridge (AST → CloudPol IR)...\n");
    CloudPolModule *mod = build_mlir_module(g_program, opts.input_file);
    printf("[CloudPol] IR: %d block(s), %d op(s) generated.\n",
           mod->block_count, mod->total_ops);

    /* ── Phase 4: MLIR Passes ───────────────────────────────── */
    if (!opts.no_passes) {
        printf("[CloudPol] Phase 4: Running MLIR Optimization Passes...\n");
        PassStats ps = run_passes(mod);
        print_pass_stats(&ps);
    } else {
        printf("[CloudPol] Phase 4: MLIR Passes SKIPPED (--no-passes).\n");
    }

    /* ── Phase 5: Open output file (if -o) ──────────────────── */
    FILE *out = stdout;
    int   opened_out = 0;
    if (opts.output_file &&
        (opts.emit_flags & (EMIT_IAM | EMIT_REPORT | EMIT_MLIR))) {
        out = fopen(opts.output_file, "w");
        if (!out) {
            fprintf(stderr, "[CloudPol] Error: cannot write to '%s'\n",
                    opts.output_file);
            mlir_free_module(mod);
            if (g_program) free_program_node(g_program);
            return 3;
        }
        opened_out = 1;
    }

    /* ── Phase 5: Code Generation ───────────────────────────── */
    int cg_rc = 0;

    if (opts.emit_flags & EMIT_MLIR) {
        printf("[CloudPol] Phase 5: Emitting MLIR dialect IR...\n");
        emit_mlir_text(mod, out);
    }

    if (opts.emit_flags & EMIT_IAM) {
        printf("[CloudPol] Phase 5: Emitting AWS IAM JSON...\n");
        cg_rc |= codegen_emit(mod, FMT_IAM_JSON, out);
    }

    if (opts.emit_flags & EMIT_REPORT) {
        printf("[CloudPol] Phase 5: Emitting Markdown policy report...\n");
        cg_rc |= codegen_emit(mod, FMT_REPORT, out);
    }

    if (opened_out) {
        fclose(out);
        printf("[CloudPol] Output written to: %s\n", opts.output_file);
    }

    /* ── Cleanup ─────────────────────────────────────────────── */
    mlir_free_module(mod);
    if (g_program) free_program_node(g_program);

    if (cg_rc != 0) {
        fprintf(stderr, "[CloudPol] Code generation FAILED.\n");
        return 3;
    }

    printf("[CloudPol] Compilation SUCCESSFUL.\n");
    return 0;
}
