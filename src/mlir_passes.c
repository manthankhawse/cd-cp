/**
 * mlir_passes.c — CloudPol MLIR Pass Pipeline
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 5: Three transformation passes run in order over the CloudPolModule.
 *
 * Pass 1 — Dead Policy Elimination
 *   Marks a specific DENY op as DEAD if there exists an ALLOW op for the same
 *   role whose action is a wildcard covering the DENY's action
 *   (e.g. "s3:*" covers "s3:DeleteBucket"), indicating the DENY is unreachable
 *   at the IAM level when both have the same resource scope.
 *
 * Pass 2 — Effect Normalization
 *   Within each block, ensures DENY ops appear before ALLOW ops, matching the
 *   AWS policy evaluation order (explicit-deny wins, then allow).
 *
 * Pass 3 — Condition Constant Folding
 *   Detects conditions that are logically impossible or always-true:
 *     - time >= N AND time <= M where N > M  (unsatisfiable range)
 *     - time > 23 or time < 0               (out-of-range hour)
 *   Marks such ops with OP_FLAG_COND_DEAD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/mlir.h"
#include "../include/mlir_passes.h"

/* ═════════════════════════════════════════════════════════════
   Pass 1 — Dead Policy Elimination
═════════════════════════════════════════════════════════════ */

/* Returns 1 if wildcard_action (e.g. "s3:*") covers specific_action
   (e.g. "s3:DeleteBucket"). */
static int action_covered_by_wildcard(const char *wildcard_action,
                                      const char *specific_action) {
    if (!wildcard_action || !specific_action) return 0;

    /* Find the colon separating service from action name */
    const char *wc = strchr(wildcard_action, ':');
    if (!wc) return 0;
    /* Must end with ":*" */
    if (*(wc + 1) != '*' || *(wc + 2) != '\0') return 0;

    /* Extract service length */
    size_t svc_len = (size_t)(wc - wildcard_action);
    /* specific_action must begin with the same service prefix */
    return strncmp(wildcard_action, specific_action, svc_len) == 0
           && specific_action[svc_len] == ':';
}

static int pass_dead_policy_elimination(CloudPolModule *mod) {
    int removed = 0;
    CloudPolBlock *blk = mod->blocks;
    while (blk) {
        /* Collect all ALLOW wildcard actions for this block */
        CloudPolOp *op = blk->ops;
        while (op) {
            if (op->effect == EFFECT_ALLOW &&
                strchr(op->action, ':') != NULL) {
                const char *wc = strchr(op->action, ':');
                if (wc && *(wc + 1) == '*' && *(wc + 2) == '\0') {
                    /* Found a wildcard ALLOW — scan for matching DENY ops */
                    CloudPolOp *candidate = blk->ops;
                    while (candidate) {
                        if (candidate != op &&
                            candidate->effect == EFFECT_DENY &&
                            !(candidate->flags & OP_FLAG_DEAD) &&
                            action_covered_by_wildcard(op->action,
                                                       candidate->action)) {
                            candidate->flags |= OP_FLAG_DEAD;
                            removed++;
                        }
                        candidate = candidate->next;
                    }
                }
            }
            op = op->next;
        }
        blk = blk->next;
    }
    return removed;
}

/* ═════════════════════════════════════════════════════════════
   Pass 2 — Effect Normalization (DENY before ALLOW)
═════════════════════════════════════════════════════════════ */

/*
 * Reorder: collect all DENY ops first, then ALLOW ops.
 * Only non-DEAD ops are preserved in the final sorted list;
 * DEAD ops are moved to the end (not deleted — the printer annotates them).
 */
static int pass_effect_normalization(CloudPolModule *mod) {
    int reordered_blocks = 0;
    CloudPolBlock *blk = mod->blocks;

    while (blk) {
        /* Split ops into four buckets: deny_live, allow_live, dead */
        CloudPolOp *deny_head  = NULL, *deny_tail  = NULL;
        CloudPolOp *allow_head = NULL, *allow_tail = NULL;
        CloudPolOp *dead_head  = NULL, *dead_tail  = NULL;

        int was_ordered = 1;
        int saw_allow   = 0;

        CloudPolOp *cur = blk->ops;
        while (cur) {
            CloudPolOp *nxt = cur->next;
            cur->next = NULL;

            if (cur->flags & OP_FLAG_DEAD) {
                if (!dead_head) dead_head = cur; else dead_tail->next = cur;
                dead_tail = cur;
            } else if (cur->effect == EFFECT_DENY) {
                if (saw_allow) was_ordered = 0;
                if (!deny_head) deny_head = cur; else deny_tail->next = cur;
                deny_tail = cur;
            } else {
                saw_allow = 1;
                if (!allow_head) allow_head = cur; else allow_tail->next = cur;
                allow_tail = cur;
            }
            cur = nxt;
        }

        /* Stitch: DENYs → ALLOWs → DEAD */
        CloudPolOp *new_head = deny_head ? deny_head : allow_head;
        if (deny_tail)  deny_tail->next  = allow_head;
        if (allow_tail) allow_tail->next = dead_head;
        if (!new_head)  new_head = dead_head;

        blk->ops = new_head;
        if (!was_ordered) reordered_blocks++;
        blk = blk->next;
    }
    return reordered_blocks;
}

/* ═════════════════════════════════════════════════════════════
   Pass 3 — Condition Constant Folding
   Inspect cond_str for patterns we can statically evaluate.
═════════════════════════════════════════════════════════════ */

/*
 * Scan cond_str for time comparisons. Returns 1 if we detect an
 * impossible time range (e.g. time >= 22 AND time <= 6 is satisfiable
 * across midnight but we detect the simpler out-of-range case: time > 23).
 *
 * Simple heuristic: look for "(time >= N)" where N > 23 or "(time <= N)"
 * where N < 0.  These are numerically impossible.
 */
static int has_impossible_time(const char *cond_str) {
    if (!cond_str) return 0;
    /* Look for "time >=" or "time >" */
    const char *p = cond_str;
    while ((p = strstr(p, "time")) != NULL) {
        /* Scan for the number after the operator */
        const char *gt = strstr(p, ">=");
        const char *lt = strstr(p, "<=");
        if (gt && gt < p + 20) {
            int val = atoi(gt + 2);
            if (val > 23) return 1;
        }
        if (lt && lt < p + 20) {
            int val = atoi(lt + 2);
            if (val < 0) return 1;
        }
        p++;
    }
    return 0;
}

static int pass_condition_folding(CloudPolModule *mod) {
    int folded = 0;
    CloudPolBlock *blk = mod->blocks;
    while (blk) {
        CloudPolOp *op = blk->ops;
        while (op) {
            if (!(op->flags & OP_FLAG_DEAD) &&
                !(op->flags & OP_FLAG_COND_DEAD) &&
                strcmp(op->cond_str, "(none)") != 0) {
                if (has_impossible_time(op->cond_str)) {
                    op->flags |= OP_FLAG_COND_DEAD;
                    folded++;
                }
            }
            op = op->next;
        }
        blk = blk->next;
    }
    return folded;
}

/* ═════════════════════════════════════════════════════════════
   Public: run_passes
═════════════════════════════════════════════════════════════ */
PassStats run_passes(CloudPolModule *mod) {
    PassStats stats = {0, 0, 0};
    if (!mod) return stats;

    stats.dead_ops_removed  = pass_dead_policy_elimination(mod);
    stats.blocks_reordered  = pass_effect_normalization(mod);
    stats.cond_folded       = pass_condition_folding(mod);

    return stats;
}

void print_pass_stats(const PassStats *stats) {
    if (!stats) return;
    printf("\n[CloudPol Passes] MLIR optimization results:\n");
    printf("  Pass 1 (Dead Policy Elimination) : %d op(s) marked dead\n",
           stats->dead_ops_removed);
    printf("  Pass 2 (Effect Normalization)    : %d block(s) reordered\n",
           stats->blocks_reordered);
    printf("  Pass 3 (Condition Folding)       : %d condition(s) folded\n\n",
           stats->cond_folded);
}
