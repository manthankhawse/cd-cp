/**
 * semantic.h — Semantic Analysis Header
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 4: Defines the semantic error taxonomy, the SemanticError record,
 * and the public API for the AST traversal pass.
 *
 * Checks performed:
 *   SC-01  Unknown AWS action (not in mock dictionary)
 *   SC-02  Malformed ARN resource string
 *   SC-03  Duplicate policy statement (same role + action + resource)
 *   SC-04  Conflicting ALLOW/DENY for identical (role, action, resource)
 *   SC-05  Condition attribute type mismatch (e.g. mfa == "yes" is invalid)
 *   SC-06  Empty policy (zero statements)
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

/* ─────────────────────────────────────────────
   Semantic error codes
───────────────────────────────────────────── */
typedef enum {
    SEM_OK               = 0,
    SEM_ERR_UNKNOWN_ACTION,     /* SC-01 */
    SEM_ERR_BAD_ARN,            /* SC-02 */
    SEM_ERR_DUPLICATE_STMT,     /* SC-03 */
    SEM_ERR_CONFLICT_EFFECT,    /* SC-04 */
    SEM_ERR_TYPE_MISMATCH,      /* SC-05 */
    SEM_ERR_EMPTY_POLICY        /* SC-06 */
} SemErrCode;

/* ─────────────────────────────────────────────
   A single semantic diagnostic
───────────────────────────────────────────── */
typedef struct SemanticError {
    SemErrCode           code;
    int                  line;        /* source line of the offending statement */
    char                 message[256];
    struct SemanticError *next;
} SemanticError;

/* ─────────────────────────────────────────────
   Semantic analysis result
───────────────────────────────────────────── */
typedef struct {
    SemanticError *errors;   /* linked list (NULL = success) */
    int            count;    /* total diagnostic count       */
} SemanticResult;

/* ─────────────────────────────────────────────
   Public API
───────────────────────────────────────────── */

/**
 * analyze_program
 *
 * Walk the full AST and perform all semantic checks.
 * Returns a heap-allocated SemanticResult; caller owns it.
 * Print results with print_semantic_result(), free with free_semantic_result().
 */
SemanticResult *analyze_program(const ProgramNode *prog);

/** Pretty-print diagnostics to stderr. */
void print_semantic_result(const SemanticResult *result);

/** Release all memory owned by the SemanticResult. */
void free_semantic_result(SemanticResult *result);

#endif /* SEMANTIC_H */
