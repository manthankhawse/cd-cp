/**
 * semantic.c — Semantic Analysis Pass
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 4: Traverses the fully-constructed AST produced by the Bison parser
 * and enforces semantic rules that cannot be expressed in a context-free
 * grammar alone.
 *
 * Checks implemented:
 *   SC-01  Unknown AWS action (not in mock dictionary)
 *   SC-02  Malformed ARN resource string (must match arn:… or be "*")
 *   SC-03  Duplicate policy statement (same role + action + resource)
 *   SC-04  Conflicting ALLOW/DENY for identical (role, action, resource)
 *   SC-05  Condition attribute type mismatch (mfa must be bool, time numeric)
 *   SC-06  Empty policy (zero statements after a successful parse)
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../include/ast.h"
#include "../include/semantic.h"
#include "aws_dict.h"   /* is_known_aws_action(), is_valid_arn() */

/* ─────────────────────────────────────────────
   Internal helpers — error list management
───────────────────────────────────────────── */

static SemanticError *make_error(SemErrCode code, int line, const char *fmt, ...) {
    SemanticError *e = (SemanticError *)calloc(1, sizeof(SemanticError));
    e->code = code;
    e->line = line;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);

    return e;
}

static void append_error(SemanticResult *r, SemanticError *e) {
    e->next  = r->errors;
    r->errors = e;
    r->count++;
}

/* ─────────────────────────────────────────────
   SC-01: Validate AWS action
───────────────────────────────────────────── */
static void check_action(const StatementNode *s, SemanticResult *r) {
    if (!is_known_aws_action(s->action)) {
        append_error(r, make_error(
            SEM_ERR_UNKNOWN_ACTION, s->line_number,
            "SC-01: Unknown AWS action \"%s\" (role: \"%s\")",
            s->action, s->role));
    }
}

/* ─────────────────────────────────────────────
   SC-02: Validate ARN format
───────────────────────────────────────────── */
static void check_arn(const StatementNode *s, SemanticResult *r) {
    if (!is_valid_arn(s->resource)) {
        append_error(r, make_error(
            SEM_ERR_BAD_ARN, s->line_number,
            "SC-02: Malformed ARN resource \"%s\" for action \"%s\" "
            "(expected arn:<partition>:<service>:<region>:<account>:<resource> or \"*\")",
            s->resource, s->action));
    }
}

/* ─────────────────────────────────────────────
   SC-05: Condition attribute type checks
   Rules:
     mfa    — value must be "TRUE" or "FALSE"
     time   — value must be numeric (all-digit or decimal)
     ip     — value must look like a dotted-quad or CIDR (basic check)
     region — any non-empty string is acceptable
     tag    — any non-empty string is acceptable
───────────────────────────────────────────── */

/* Returns 1 if s looks like a number (integer or decimal). */
static int looks_numeric(const char *s) {
    if (!s || !*s) return 0;
    const char *p = s;
    while (*p == ' ') p++;          /* skip leading spaces */
    int has_dot = 0, has_digit = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { has_digit = 1; }
        else if (*p == '.' && !has_dot) { has_dot = 1; }
        else return 0;
    }
    return has_digit;
}

/* Returns 1 if s looks like an IPv4 address or CIDR. */
static int looks_like_ip(const char *s) {
    if (!s) return 0;
    int dots = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '.' || *p == '/') dots++;
        else if (*p < '0' || *p > '9') return 0;
    }
    return dots >= 3;   /* at least 3 dots for a.b.c.d */
}

static void check_cond_types(const CondNode *c, const StatementNode *s,
                              SemanticResult *r) {
    if (!c) return;
    if (c->kind == COND_SIMPLE) {
        const char *attr = c->attribute;
        const char *val  = c->value;

        if (strcmp(attr, "mfa") == 0) {
            /* mfa must compare to a boolean literal */
            if (strcmp(val, "TRUE") != 0 && strcmp(val, "FALSE") != 0) {
                append_error(r, make_error(
                    SEM_ERR_TYPE_MISMATCH, s->line_number,
                    "SC-05: Type mismatch — 'mfa' attribute expects TRUE/FALSE, got \"%s\"",
                    val));
            }
        } else if (strcmp(attr, "time") == 0) {
            /* time must compare to a numeric literal */
            if (!looks_numeric(val)) {
                append_error(r, make_error(
                    SEM_ERR_TYPE_MISMATCH, s->line_number,
                    "SC-05: Type mismatch — 'time' attribute expects a numeric value, got \"%s\"",
                    val));
            }
        } else if (strcmp(attr, "ip") == 0) {
            /* ip must look like an IPv4 address or CIDR */
            if (!looks_like_ip(val)) {
                append_error(r, make_error(
                    SEM_ERR_TYPE_MISMATCH, s->line_number,
                    "SC-05: Type mismatch — 'ip' attribute expects an IPv4 address/CIDR, got \"%s\"",
                    val));
            }
        }
        /* region and tag accept any string — no further constraint */
    } else {
        /* Recurse into compound condition */
        check_cond_types(c->left,  s, r);
        check_cond_types(c->right, s, r);
    }
}

/* ─────────────────────────────────────────────
   SC-03 / SC-04: Duplicate and conflict detection
   We keep a small seen-list; O(n²) is fine for policy files.
───────────────────────────────────────────── */
typedef struct Seen {
    const char    *role;
    const char    *action;
    const char    *resource;
    Effect         effect;
    int            line;
    struct Seen   *next;
} Seen;

static void check_duplicates(const ProgramNode *prog, SemanticResult *r) {
    Seen *head = NULL;

    const StatementNode *s = prog->statements;
    while (s) {
        /* Search previous statements */
        for (Seen *p = head; p; p = p->next) {
            if (strcmp(p->role,     s->role)     == 0 &&
                strcmp(p->action,   s->action)   == 0 &&
                strcmp(p->resource, s->resource) == 0) {

                if (p->effect == s->effect) {
                    /* SC-03 exact duplicate */
                    append_error(r, make_error(
                        SEM_ERR_DUPLICATE_STMT, s->line_number,
                        "SC-03: Duplicate statement — %s ROLE \"%s\" ACTION \"%s\" "
                        "ON RESOURCE \"%s\" (first seen at line %d)",
                        s->effect == EFFECT_ALLOW ? "ALLOW" : "DENY",
                        s->role, s->action, s->resource, p->line));
                } else {
                    /* SC-04 conflicting ALLOW/DENY */
                    append_error(r, make_error(
                        SEM_ERR_CONFLICT_EFFECT, s->line_number,
                        "SC-04: Conflicting effects — %s at line %d conflicts with "
                        "%s at line %d for ROLE \"%s\" ACTION \"%s\" RESOURCE \"%s\"",
                        s->effect == EFFECT_ALLOW ? "ALLOW" : "DENY",
                        s->line_number,
                        p->effect == EFFECT_ALLOW ? "ALLOW" : "DENY",
                        p->line,
                        s->role, s->action, s->resource));
                }
            }
        }

        /* Record this statement */
        Seen *entry       = (Seen *)calloc(1, sizeof(Seen));
        entry->role       = s->role;
        entry->action     = s->action;
        entry->resource   = s->resource;
        entry->effect     = s->effect;
        entry->line       = s->line_number;
        entry->next       = head;
        head              = entry;

        s = s->next;
    }

    /* Free seen-list */
    while (head) { Seen *tmp = head->next; free(head); head = tmp; }
}

/* ─────────────────────────────────────────────
   Public API — analyze_program
───────────────────────────────────────────── */
SemanticResult *analyze_program(const ProgramNode *prog) {
    SemanticResult *r = (SemanticResult *)calloc(1, sizeof(SemanticResult));
    if (!prog) {
        append_error(r, make_error(SEM_ERR_EMPTY_POLICY, 0,
            "SC-06: Empty policy — no statements provided"));
        return r;
    }

    /* SC-06: Empty policy */
    if (prog->count == 0) {
        append_error(r, make_error(SEM_ERR_EMPTY_POLICY, 0,
            "SC-06: Empty policy — no statements to analyze"));
        return r;
    }

    /* Per-statement checks */
    const StatementNode *s = prog->statements;
    while (s) {
        check_action(s, r);              /* SC-01 */
        check_arn(s, r);                 /* SC-02 */
        check_cond_types(s->condition, s, r);  /* SC-05 */
        s = s->next;
    }

    /* Cross-statement checks */
    check_duplicates(prog, r);           /* SC-03, SC-04 */

    /* Reverse error list to restore source order */
    SemanticError *prev = NULL, *cur = r->errors, *nxt;
    while (cur) { nxt = cur->next; cur->next = prev; prev = cur; cur = nxt; }
    r->errors = prev;

    return r;
}

/* ─────────────────────────────────────────────
   Public API — print_semantic_result
───────────────────────────────────────────── */
static const char *sem_code_str(SemErrCode c) {
    switch (c) {
        case SEM_ERR_UNKNOWN_ACTION:  return "UNKNOWN_ACTION";
        case SEM_ERR_BAD_ARN:         return "BAD_ARN";
        case SEM_ERR_DUPLICATE_STMT:  return "DUPLICATE_STMT";
        case SEM_ERR_CONFLICT_EFFECT: return "CONFLICT_EFFECT";
        case SEM_ERR_TYPE_MISMATCH:   return "TYPE_MISMATCH";
        case SEM_ERR_EMPTY_POLICY:    return "EMPTY_POLICY";
        default:                      return "UNKNOWN";
    }
}

void print_semantic_result(const SemanticResult *result) {
    if (!result) return;

    if (result->count == 0) {
        fprintf(stdout,
            "\n[CloudPol Semantic] ✓  All semantic checks PASSED — no issues found.\n\n");
        return;
    }

    fprintf(stderr,
        "\n╔══════════════════════════════════════════╗\n");
    fprintf(stderr,
        "║   CloudPol Semantic Analysis — ERRORS   ║\n");
    fprintf(stderr,
        "╚══════════════════════════════════════════╝\n");

    int idx = 1;
    const SemanticError *e = result->errors;
    while (e) {
        fprintf(stderr,
            "\n[Error %d] line %-4d  [%s]\n  %s\n",
            idx++, e->line, sem_code_str(e->code), e->message);
        e = e->next;
    }

    fprintf(stderr,
        "\n[CloudPol Semantic] %d semantic error(s) detected.\n\n",
        result->count);
}

/* ─────────────────────────────────────────────
   Public API — free_semantic_result
───────────────────────────────────────────── */
void free_semantic_result(SemanticResult *result) {
    if (!result) return;
    SemanticError *cur = result->errors;
    while (cur) {
        SemanticError *nxt = cur->next;
        free(cur);
        cur = nxt;
    }
    free(result);
}
