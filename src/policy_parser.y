%{
/**
 * policy_parser.y — Bison Parser Specification for CloudPol DSL
 * Cloud Policy as Code DSL Compiler
 *
 * Step 3: Complete Syntax Analysis + AST Construction
 *
 * Grammar:
 *   program    := statement*
 *   statement  := effect ROLE STRING ACTION STRING ON RESOURCE STRING [WHERE condition] ;
 *   condition  := simple_cond
 *              |  condition AND condition
 *              |  condition OR  condition
 *              |  NOT simple_cond
 *   simple_cond := attribute rel_op value
 *   attribute  := ip | time | mfa | region | tag
 *   rel_op     := == | != | < | > | <= | >=
 *   value      := STRING | NUMBER | TRUE | FALSE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ast.h"
#include "../include/semantic.h"

extern int yylex(void);
extern int yylineno;
extern int yycolno;

/* ── Parser state ────────────────────────────────────────────── */
static ProgramNode *g_program = NULL;
static int          g_errors  = 0;

void yyerror(const char *msg) {
    fprintf(stderr,
        "[CloudPol Parser] SYNTAX ERROR line %d:%d — %s\n",
        yylineno, yycolno, msg);
    g_errors++;
}

%}

/* ─────────────────────────────────────────────────────────────
   %code requires — emitted into the generated header BEFORE the
   YYSTYPE union definition, so that both the parser and the lexer
   (which #includes policy_parser.tab.h) see CondNode/StatementNode.
───────────────────────────────────────────────────────────────*/
%code requires {
    #include "../include/ast.h"
}

/* ─────────────────────────────────────────────────────────────
   Semantic value union
───────────────────────────────────────────────────────────────*/
%union {
    char          *sval;    /* STRING / NUMBER literals (heap-alloc)  */
    int            bval;    /* TRUE / FALSE                           */
    int            rval;    /* relational operator (RelOp cast)       */
    int            eval;    /* effect (Effect cast)                   */
    CondNode      *cond;    /* condition sub-tree                     */
    StatementNode *stmt;    /* single policy statement                */
}

/* ─────────────────────────────────────────────────────────────
   Token declarations
─────────────────────────────────────────────────────────────*/
%token KW_ALLOW
%token KW_DENY
%token KW_ROLE
%token KW_ACTION
%token KW_ON
%token KW_RESOURCE
%token KW_WHERE
%token KW_AND
%token KW_OR
%token KW_NOT
%token <bval> KW_TRUE
%token <bval> KW_FALSE
%token ATTR_IP
%token ATTR_TIME
%token ATTR_MFA
%token ATTR_REGION
%token ATTR_TAG
%token OP_EQ
%token OP_NEQ
%token OP_LT
%token OP_GT
%token OP_LEQ
%token OP_GEQ
%token SEMICOLON
%token <sval> STRING
%token <sval> NUMBER

/* ─────────────────────────────────────────────────────────────
   Non-terminal types
─────────────────────────────────────────────────────────────*/
%type <eval>  effect
%type <rval>  rel_op
%type <cond>  condition simple_cond
%type <stmt>  statement
%type <sval>  value attr_str

/* ─────────────────────────────────────────────────────────────
   Operator precedence — lowest to highest binding
   OR < AND < NOT
─────────────────────────────────────────────────────────────*/
%left  KW_OR
%left  KW_AND
%right KW_NOT

%%

/* ═════════════════════════════════════════════════════════════
   GRAMMAR RULES
═════════════════════════════════════════════════════════════ */

program
    : /* empty */
        { g_program = (ProgramNode *)calloc(1, sizeof(ProgramNode)); }

    | program statement
        {
            if ($2) {
                $2->next              = g_program->statements;
                g_program->statements = $2;
                g_program->count++;
            }
        }

    | program error SEMICOLON
        { yyerrok; }
    ;

statement
    : effect KW_ROLE STRING KW_ACTION STRING KW_ON KW_RESOURCE STRING SEMICOLON
        {
            $$ = make_statement((Effect)$1, $3, $5, $8, NULL, yylineno);
            free($3); free($5); free($8);
        }

    | effect KW_ROLE STRING KW_ACTION STRING KW_ON KW_RESOURCE STRING KW_WHERE condition SEMICOLON
        {
            $$ = make_statement((Effect)$1, $3, $5, $8, $10, yylineno);
            free($3); free($5); free($8);
        }
    ;

effect
    : KW_ALLOW  { $$ = (int)EFFECT_ALLOW; }
    | KW_DENY   { $$ = (int)EFFECT_DENY;  }
    ;

condition
    : simple_cond
        { $$ = $1; }

    | condition KW_AND condition
        { $$ = make_compound_cond(LOG_AND, $1, $3); }

    | condition KW_OR  condition
        { $$ = make_compound_cond(LOG_OR,  $1, $3); }

    | KW_NOT simple_cond
        { $$ = make_compound_cond(LOG_NOT, $2, NULL); }
    ;

simple_cond
    : attr_str rel_op value
        {
            $$ = make_simple_cond($1, (RelOp)$2, $3);
            free($3);
        }
    ;

attr_str
    : ATTR_IP     { $$ = "ip";     }
    | ATTR_TIME   { $$ = "time";   }
    | ATTR_MFA    { $$ = "mfa";    }
    | ATTR_REGION { $$ = "region"; }
    | ATTR_TAG    { $$ = "tag";    }
    ;

rel_op
    : OP_EQ   { $$ = (int)REL_EQ;  }
    | OP_NEQ  { $$ = (int)REL_NEQ; }
    | OP_LT   { $$ = (int)REL_LT;  }
    | OP_GT   { $$ = (int)REL_GT;  }
    | OP_LEQ  { $$ = (int)REL_LEQ; }
    | OP_GEQ  { $$ = (int)REL_GEQ; }
    ;

value
    : STRING    { $$ = $1;              }
    | NUMBER    { $$ = $1;              }
    | KW_TRUE   { $$ = strdup("TRUE");  }
    | KW_FALSE  { $$ = strdup("FALSE"); }
    ;

%%

/* ═════════════════════════════════════════════════════════════
   ENTRY POINT
═════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    extern FILE *yyin;

    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            fprintf(stderr, "[CloudPol] Error: cannot open '%s'\n", argv[1]);
            return 1;
        }
    }

    /* ── Phase 1: Syntax Analysis ─────────────────────────────────── */
    printf("[CloudPol] Phase 1: Syntax analysis…\n");
    int rc = yyparse();
    if (argc > 1) fclose(yyin);

    if (g_errors > 0 || rc != 0) {
        fprintf(stderr,
            "[CloudPol Parser] Parse FAILED — %d error(s) encountered.\n",
            g_errors);
        if (g_program) free_program_node(g_program);
        return 1;
    }

    /* Reverse statement list to restore source order */
    if (g_program) {
        StatementNode *prev = NULL, *cur = g_program->statements, *nxt;
        while (cur) { nxt = cur->next; cur->next = prev; prev = cur; cur = nxt; }
        g_program->statements = prev;
    }

    printf("[CloudPol Parser] Parse SUCCESSFUL — %d statement(s) parsed.\n",
           g_program ? g_program->count : 0);

    /* Print AST */
    if (g_program) print_program_node(g_program);

    /* ── Phase 2: Semantic Analysis ───────────────────────────────── */
    printf("[CloudPol] Phase 2: Semantic analysis…\n");
    SemanticResult *sem = analyze_program(g_program);
    print_semantic_result(sem);
    int sem_errors = sem ? sem->count : 0;
    free_semantic_result(sem);

    if (g_program) free_program_node(g_program);

    if (sem_errors > 0) {
        fprintf(stderr,
            "[CloudPol] Compilation FAILED — %d semantic error(s).\n",
            sem_errors);
        return 2;
    }

    printf("[CloudPol] Compilation SUCCESSFUL — policy is syntactically and semantically valid.\n");
    return 0;
}
