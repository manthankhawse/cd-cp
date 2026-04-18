/**
 * ast.h — Abstract Syntax Tree (AST) Node Definitions
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Defines in-memory representation of every node in the AST.
 * Used in Step 3 (Bison parser actions) and Step 4 (Semantic Analysis).
 */

#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────
   Effect type (ALLOW / DENY)
───────────────────────────────────────────── */
typedef enum {
    EFFECT_ALLOW,
    EFFECT_DENY
} Effect;

/* ─────────────────────────────────────────────
   Relational operators for WHERE conditions
───────────────────────────────────────────── */
typedef enum {
    REL_EQ,   /* == */
    REL_NEQ,  /* != */
    REL_LT,   /* <  */
    REL_GT,   /* >  */
    REL_LEQ,  /* <= */
    REL_GEQ   /* >= */
} RelOp;

/* ─────────────────────────────────────────────
   Logical operators for compound conditions
───────────────────────────────────────────── */
typedef enum {
    LOG_AND,
    LOG_OR,
    LOG_NOT
} LogOp;

/* ─────────────────────────────────────────────
   Condition node
───────────────────────────────────────────── */
typedef enum {
    COND_SIMPLE,    /* attribute op value */
    COND_COMPOUND   /* cond (AND|OR) cond  OR  NOT cond */
} CondKind;

typedef struct CondNode CondNode;
struct CondNode {
    CondKind kind;
    /* COND_SIMPLE fields */
    char   *attribute;     /* e.g. "ip", "mfa", "region" */
    RelOp   rel_op;
    char   *value;         /* string or number literal value */
    /* COND_COMPOUND fields */
    LogOp      log_op;
    CondNode  *left;
    CondNode  *right;      /* NULL when LOG_NOT */
};

/* ─────────────────────────────────────────────
   Statement node  (one ALLOW/DENY directive)
───────────────────────────────────────────── */
typedef struct StatementNode {
    Effect    effect;
    char     *role;        /* e.g. "DevOps" */
    char     *action;      /* e.g. "s3:PutObject" */
    char     *resource;    /* e.g. "arn:aws:s3:::my-bucket" */
    CondNode *condition;   /* NULL when there is no WHERE clause */
    int       line_number; /* source line for error reporting */
    struct StatementNode *next;
} StatementNode;

/* ─────────────────────────────────────────────
   Program node  (root of the AST)
───────────────────────────────────────────── */
typedef struct {
    StatementNode *statements; /* linked list of statements */
    int            count;
} ProgramNode;

/* ─────────────────────────────────────────────
   Constructor helpers
───────────────────────────────────────────── */
static inline CondNode *make_simple_cond(const char *attr,
                                         RelOp op,
                                         const char *val) {
    CondNode *n = (CondNode *)calloc(1, sizeof(CondNode));
    n->kind      = COND_SIMPLE;
    n->attribute = strdup(attr);
    n->rel_op    = op;
    n->value     = strdup(val);
    return n;
}

static inline CondNode *make_compound_cond(LogOp op,
                                            CondNode *left,
                                            CondNode *right) {
    CondNode *n = (CondNode *)calloc(1, sizeof(CondNode));
    n->kind   = COND_COMPOUND;
    n->log_op = op;
    n->left   = left;
    n->right  = right;
    return n;
}

static inline StatementNode *make_statement(Effect effect,
                                             const char *role,
                                             const char *action,
                                             const char *resource,
                                             CondNode   *condition,
                                             int         line_no) {
    StatementNode *s = (StatementNode *)calloc(1, sizeof(StatementNode));
    s->effect      = effect;
    s->role        = strdup(role);
    s->action      = strdup(action);
    s->resource    = strdup(resource);
    s->condition   = condition;
    s->line_number = line_no;
    return s;
}

/* ─────────────────────────────────────────────
   AST free helpers (forward decl, implemented
   in ast.c in Step 3)
───────────────────────────────────────────── */
void free_cond_node(CondNode *node);
void free_statement_node(StatementNode *node);
void free_program_node(ProgramNode *node);

/* ─────────────────────────────────────────────
   AST print helpers (for debugging)
───────────────────────────────────────────── */
void print_cond_node(const CondNode *node, int indent);
void print_program_node(const ProgramNode *prog);

#endif /* AST_H */
