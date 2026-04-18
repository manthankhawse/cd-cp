/**
 * mlir_bridge.c — AST → CloudPol MLIR Dialect IR
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 5: Converts the ProgramNode AST (output of Phase 3 parser) into the
 * CloudPolModule IR (input to Phase 5 passes and Phase 6 codegen).
 *
 * Lowering strategy:
 *   - Each unique role becomes one CloudPolBlock.
 *   - Each StatementNode becomes one CloudPolOp.
 *   - Condition trees are serialised to a canonical parenthesised string.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/mlir.h"

/* ─────────────────────────────────────────────
   Internal: condition serialiser
───────────────────────────────────────────── */
static const char *relop_sym(RelOp op) {
    switch (op) {
        case REL_EQ:  return "==";
        case REL_NEQ: return "!=";
        case REL_LT:  return "<";
        case REL_GT:  return ">";
        case REL_LEQ: return "<=";
        case REL_GEQ: return ">=";
        default:      return "?";
    }
}

/* Recursively build a canonical condition string into buf (max buf_sz chars). */
static void cond_to_str(const CondNode *c, char *buf, size_t buf_sz) {
    if (!c) { snprintf(buf, buf_sz, "(none)"); return; }

    if (c->kind == COND_SIMPLE) {
        snprintf(buf, buf_sz, "(%s %s %s)",
                 c->attribute, relop_sym(c->rel_op), c->value);
    } else {
        char left_buf[256]  = "";
        char right_buf[256] = "";
        cond_to_str(c->left,  left_buf,  sizeof(left_buf));
        if (c->log_op == LOG_NOT) {
            snprintf(buf, buf_sz, "(NOT %s)", left_buf);
        } else {
            cond_to_str(c->right, right_buf, sizeof(right_buf));
            snprintf(buf, buf_sz, "(%s %s %s)",
                     left_buf,
                     c->log_op == LOG_AND ? "AND" : "OR",
                     right_buf);
        }
    }
}

/* ─────────────────────────────────────────────
   CloudPolModule constructor helpers
───────────────────────────────────────────── */
CloudPolModule *mlir_new_module(const char *source_file) {
    CloudPolModule *m = (CloudPolModule *)calloc(1, sizeof(CloudPolModule));
    if (source_file) m->source_file = strdup(source_file);
    return m;
}

CloudPolBlock *mlir_find_or_create_block(CloudPolModule *mod,
                                         const char *role) {
    /* Search existing blocks */
    CloudPolBlock *b = mod->blocks;
    while (b) {
        if (strcmp(b->role, role) == 0) return b;
        b = b->next;
    }
    /* Create new block and prepend */
    CloudPolBlock *nb = (CloudPolBlock *)calloc(1, sizeof(CloudPolBlock));
    nb->role  = strdup(role);
    nb->next  = mod->blocks;
    mod->blocks = nb;
    mod->block_count++;
    return nb;
}

CloudPolOp *mlir_new_op(Effect effect,
                        const char *action,
                        const char *resource,
                        const char *cond_str,
                        int source_line) {
    CloudPolOp *op    = (CloudPolOp *)calloc(1, sizeof(CloudPolOp));
    op->effect        = effect;
    op->action        = strdup(action);
    op->resource      = strdup(resource);
    op->cond_str      = strdup(cond_str);
    op->source_line   = source_line;
    op->flags         = OP_FLAG_NONE;
    return op;
}

void mlir_append_op(CloudPolBlock *blk, CloudPolOp *op) {
    /* Append to tail to preserve source order */
    if (!blk->ops) {
        blk->ops = op;
    } else {
        CloudPolOp *cur = blk->ops;
        while (cur->next) cur = cur->next;
        cur->next = op;
    }
    blk->op_count++;
}

/* ─────────────────────────────────────────────
   build_mlir_module
   Main bridge: ProgramNode AST → CloudPolModule IR
───────────────────────────────────────────── */
CloudPolModule *build_mlir_module(const ProgramNode *prog,
                                  const char *source_file) {
    CloudPolModule *mod = mlir_new_module(source_file);
    if (!prog || prog->count == 0) return mod;

    /* First pass: collect blocks in source order by reversing the approach —
       we iterate forward through the linked list (already reversed in main). */
    const StatementNode *s = prog->statements;
    while (s) {
        char cond_buf[512];
        cond_to_str(s->condition, cond_buf, sizeof(cond_buf));

        CloudPolBlock *blk = mlir_find_or_create_block(mod, s->role);
        CloudPolOp   *op  = mlir_new_op(s->effect, s->action, s->resource,
                                        cond_buf, s->line_number);
        mlir_append_op(blk, op);
        mod->total_ops++;
        s = s->next;
    }

    /* Reverse block list to restore source order (blocks were prepended) */
    CloudPolBlock *prev = NULL, *cur = mod->blocks, *nxt;
    while (cur) { nxt = cur->next; cur->next = prev; prev = cur; cur = nxt; }
    mod->blocks = prev;

    return mod;
}

/* ─────────────────────────────────────────────
   emit_mlir_text
   Human-readable CloudPol dialect IR printer.
───────────────────────────────────────────── */
void emit_mlir_text(const CloudPolModule *mod, FILE *out) {
    if (!mod) return;

    fprintf(out, "// CloudPol MLIR Dialect IR\n");
    if (mod->source_file)
        fprintf(out, "// Source: %s\n", mod->source_file);
    fprintf(out, "// Blocks: %d   Ops: %d\n\n",
            mod->block_count, mod->total_ops);

    fprintf(out, "module @cloudpol_policy {\n\n");

    const CloudPolBlock *b = mod->blocks;
    while (b) {
        fprintf(out, "  cloudpol.policy_block @%s {\n",
                b->role);

        const CloudPolOp *op = b->ops;
        while (op) {
            const char *effect_s = op->effect == EFFECT_ALLOW ? "ALLOW" : "DENY";
            const char *dead_s   = (op->flags & OP_FLAG_DEAD)
                                   ? "  // [DEAD — eliminated by pass]" : "";
            const char *cfold_s  = (op->flags & OP_FLAG_COND_DEAD)
                                   ? "  // [COND-DEAD — condition always false]" : "";

            fprintf(out,
                "    cloudpol.op %s action=\"%s\"\n"
                "               resource=\"%s\"\n"
                "               cond=%s\n"
                "               line=%d%s%s\n",
                effect_s,
                op->action,
                op->resource,
                op->cond_str,
                op->source_line,
                dead_s,
                cfold_s);

            op = op->next;
        }

        fprintf(out, "  }\n\n");
        b = b->next;
    }

    fprintf(out, "} // end module\n");
}

/* ─────────────────────────────────────────────
   mlir_free_module
───────────────────────────────────────────── */
void mlir_free_module(CloudPolModule *mod) {
    if (!mod) return;
    free(mod->source_file);
    CloudPolBlock *b = mod->blocks;
    while (b) {
        CloudPolBlock *bnxt = b->next;
        free(b->role);
        CloudPolOp *op = b->ops;
        while (op) {
            CloudPolOp *onxt = op->next;
            free(op->action);
            free(op->resource);
            free(op->cond_str);
            free(op);
            op = onxt;
        }
        free(b);
        b = bnxt;
    }
    free(mod);
}
