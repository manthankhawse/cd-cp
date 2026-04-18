/**
 * mlir.h — CloudPol MLIR Dialect IR Types
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 5: Defines the in-memory CloudPol MLIR dialect:
 *
 *   CloudPolModule
 *     └─ CloudPolBlock[]   (one per unique role)
 *           └─ CloudPolOp[]   (one per policy statement)
 *
 * This mirrors LLVM MLIR's Module/Block/Operation hierarchy using plain C.
 * The serialised textual form is printed as a `.mlir`-style file with a
 * custom `cloudpol` dialect namespace.
 *
 * Textual IR example:
 *   module @cloudpol_policy {
 *     cloudpol.policy_block @DevOps {
 *       cloudpol.op ALLOW action="s3:PutObject"
 *                         resource="arn:aws:s3:::prod-artifacts"
 *                         cond="(none)" line=9
 *     }
 *   }
 */

#ifndef MLIR_H
#define MLIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* ─────────────────────────────────────────────
   Op flags (set/cleared by passes)
───────────────────────────────────────────── */
#define OP_FLAG_NONE        0x00
#define OP_FLAG_DEAD        0x01   /* dead-policy-elimination marked */
#define OP_FLAG_COND_DEAD   0x02   /* condition always-false */

/* ─────────────────────────────────────────────
   CloudPolOp — a single policy operation
───────────────────────────────────────────── */
typedef struct CloudPolOp {
    Effect  effect;
    char   *action;        /* heap-owned */
    char   *resource;      /* heap-owned */
    char   *cond_str;      /* canonical condition string, heap-owned */
    int     source_line;
    int     flags;         /* OP_FLAG_* bitmask */
    struct CloudPolOp *next;
} CloudPolOp;

/* ─────────────────────────────────────────────
   CloudPolBlock — ops for one role
───────────────────────────────────────────── */
typedef struct CloudPolBlock {
    char        *role;     /* heap-owned */
    CloudPolOp  *ops;      /* linked list */
    int          op_count;
    struct CloudPolBlock *next;
} CloudPolBlock;

/* ─────────────────────────────────────────────
   CloudPolModule — root of the IR
───────────────────────────────────────────── */
typedef struct {
    char          *source_file;   /* heap-owned, may be NULL */
    CloudPolBlock *blocks;        /* linked list */
    int            block_count;
    int            total_ops;
} CloudPolModule;

/* ─────────────────────────────────────────────
   Constructor / free helpers
───────────────────────────────────────────── */
CloudPolModule *mlir_new_module(const char *source_file);
CloudPolBlock  *mlir_find_or_create_block(CloudPolModule *mod,
                                          const char *role);
CloudPolOp     *mlir_new_op(Effect effect,
                            const char *action,
                            const char *resource,
                            const char *cond_str,
                            int source_line);
void mlir_append_op(CloudPolBlock *blk, CloudPolOp *op);
void mlir_free_module(CloudPolModule *mod);

/** Bridge: lower the ProgramNode AST into a CloudPolModule IR. */
CloudPolModule *build_mlir_module(const ProgramNode *prog,
                                  const char *source_file);

/* ─────────────────────────────────────────────
   Textual IR printer
───────────────────────────────────────────── */
void emit_mlir_text(const CloudPolModule *mod, FILE *out);

#endif /* MLIR_H */
