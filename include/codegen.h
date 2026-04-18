/**
 * codegen.h — CloudPol Backend Code Generation API
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 6: Defines the output format selector and the single entry-point
 * function that drives all code generation backends.
 *
 * Supported output formats:
 *   FMT_IAM_JSON   — AWS IAM policy document (JSON)
 *   FMT_REPORT     — Human-readable Markdown policy report
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "mlir.h"

/* ─────────────────────────────────────────────
   Output format selector
───────────────────────────────────────────── */
typedef enum {
    FMT_IAM_JSON = 0,
    FMT_REPORT
} CodegenFormat;

/* ─────────────────────────────────────────────
   Public API
───────────────────────────────────────────── */

/**
 * codegen_emit
 * Lower the CloudPol IR to the requested output format, writing
 * to `out`.  Returns 0 on success, non-zero on error.
 */
int codegen_emit(const CloudPolModule *mod, CodegenFormat fmt, FILE *out);

#endif /* CODEGEN_H */
