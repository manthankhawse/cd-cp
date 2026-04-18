/**
 * ast.c — AST Node Implementation
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 3: Implementations of AST free helpers and pretty-print helpers
 * declared in include/ast.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ast.h"

/* ─────────────────────────────────────────────
   String helpers
───────────────────────────────────────────── */
static const char *effect_str(Effect e) {
    return e == EFFECT_ALLOW ? "ALLOW" : "DENY";
}

static const char *relop_str(RelOp op) {
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

static const char *logop_str(LogOp op) {
    switch (op) {
        case LOG_AND: return "AND";
        case LOG_OR:  return "OR";
        case LOG_NOT: return "NOT";
        default:      return "?";
    }
}

/* ─────────────────────────────────────────────
   free_cond_node
   Recursively frees a CondNode tree.
───────────────────────────────────────────── */
void free_cond_node(CondNode *node) {
    if (!node) return;
    if (node->kind == COND_SIMPLE) {
        free(node->attribute);
        free(node->value);
    } else {
        /* COND_COMPOUND */
        free_cond_node(node->left);
        free_cond_node(node->right);  /* NULL-safe */
    }
    free(node);
}

/* ─────────────────────────────────────────────
   free_statement_node
   Frees a single StatementNode (not the linked list).
───────────────────────────────────────────── */
void free_statement_node(StatementNode *node) {
    if (!node) return;
    free(node->role);
    free(node->action);
    free(node->resource);
    free_cond_node(node->condition);
    free(node);
}

/* ─────────────────────────────────────────────
   free_program_node
   Frees the entire AST.
───────────────────────────────────────────── */
void free_program_node(ProgramNode *node) {
    if (!node) return;
    StatementNode *cur = node->statements;
    while (cur) {
        StatementNode *next = cur->next;
        free_statement_node(cur);
        cur = next;
    }
    free(node);
}

/* ─────────────────────────────────────────────
   print_cond_node
   Recursively pretty-prints a condition subtree.
───────────────────────────────────────────── */
void print_cond_node(const CondNode *node, int indent) {
    if (!node) return;
    /* Build indent prefix */
    char pad[64] = "";
    int i;
    for (i = 0; i < indent && i < 62; i++) pad[i] = ' ';
    pad[i] = '\0';

    if (node->kind == COND_SIMPLE) {
        printf("%sSIMPLE_COND: %s %s %s\n",
               pad, node->attribute, relop_str(node->rel_op), node->value);
    } else {
        /* COND_COMPOUND */
        if (node->log_op == LOG_NOT) {
            printf("%sNOT\n", pad);
            print_cond_node(node->left, indent + 4);
        } else {
            printf("%s%s\n", pad, logop_str(node->log_op));
            print_cond_node(node->left,  indent + 4);
            print_cond_node(node->right, indent + 4);
        }
    }
}

/* ─────────────────────────────────────────────
   print_program_node
   Prints the entire AST to stdout.
───────────────────────────────────────────── */
void print_program_node(const ProgramNode *prog) {
    if (!prog) return;
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║         CloudPol AST — %2d statement(s)   ║\n", prog->count);
    printf("╚══════════════════════════════════════════╝\n");

    StatementNode *s = prog->statements;
    int idx = 1;
    while (s) {
        printf("\n[Statement %d] line %d\n", idx++, s->line_number);
        printf("  Effect   : %s\n",   effect_str(s->effect));
        printf("  Role     : \"%s\"\n", s->role);
        printf("  Action   : \"%s\"\n", s->action);
        printf("  Resource : \"%s\"\n", s->resource);
        if (s->condition) {
            printf("  WHERE->\n");
            print_cond_node(s->condition, 4);
        } else {
            printf("  (no WHERE clause)\n");
        }
        s = s->next;
    }
    printf("\n");
}
