/**
 * cli.h — CloudPol CLI Options
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 7: Defines the CliOptions struct parsed from argv and the
 * entry-point function that drives the full compiler pipeline.
 */

#ifndef CLI_H
#define CLI_H

#include "codegen.h"

/* ─────────────────────────────────────────────
   Emit-mode flags (bitmask — multiple allowed)
───────────────────────────────────────────── */
#define EMIT_AST    0x01
#define EMIT_MLIR   0x02
#define EMIT_IAM    0x04
#define EMIT_REPORT 0x08

/* ─────────────────────────────────────────────
   Parsed CLI options
───────────────────────────────────────────── */
typedef struct {
    const char *input_file;   /* NULL = stdin */
    const char *output_file;  /* NULL = stdout */
    int         emit_flags;   /* bitmask of EMIT_* */
    int         no_semantic;  /* skip semantic pass */
    int         no_passes;    /* skip MLIR passes */
} CliOptions;

/* ─────────────────────────────────────────────
   Public API
───────────────────────────────────────────── */

/** Parse argv into a CliOptions struct. Exits on --help / --version / error. */
CliOptions parse_cli(int argc, char *argv[]);

/** Run the full compiler pipeline according to opts. Returns process exit code. */
int run_compiler(CliOptions opts);

#endif /* CLI_H */
