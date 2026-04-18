/**
 * mlir_passes.h — CloudPol MLIR Pass Manager API
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 5: Declares the pass manager that runs a fixed pipeline of
 * transformation passes over the CloudPol IR.
 *
 * Pass Pipeline (in order):
 *   Pass 1 — Dead Policy Elimination
 *   Pass 2 — Effect Normalization (DENY-before-ALLOW ordering)
 *   Pass 3 — Condition Constant Folding (tautology / impossibility detection)
 */

#ifndef MLIR_PASSES_H
#define MLIR_PASSES_H

#include "mlir.h"

/* ─────────────────────────────────────────────
   Per-pass statistics
───────────────────────────────────────────── */
typedef struct {
    int dead_ops_removed;      /* Pass 1 */
    int blocks_reordered;      /* Pass 2 */
    int cond_folded;           /* Pass 3 */
} PassStats;

/* ─────────────────────────────────────────────
   Public API
───────────────────────────────────────────── */

/**
 * run_passes
 * Execute the full pass pipeline over `mod` in-place.
 * Returns aggregated statistics.
 */
PassStats run_passes(CloudPolModule *mod);

/** Print pass statistics to stdout. */
void print_pass_stats(const PassStats *stats);

#endif /* MLIR_PASSES_H */
