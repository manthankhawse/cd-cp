%{
/**
 * policy_parser.y — Bison Parser Specification for CloudPol DSL
 * Cloud Policy as Code DSL Compiler
 *
 * Step 2 STUB — Token declarations only.
 * Full grammar rules and AST construction actions are added in Step 3.
 *
 * This stub is required so that `flex` can compile `policy_lexer.l`:
 * Flex references the token codes and yylval union declared here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ast.h"

/* Error handler called by Bison on syntax errors */
void yyerror(const char *msg) {
    extern int yylineno;
    extern int yycolno;
    fprintf(stderr,
        "[CloudPol Parser] SYNTAX ERROR line %d:%d — %s\n",
        yylineno, yycolno, msg);
}

/* Entry point declared by Bison */
extern int yylex(void);

%}

/* ─────────────────────────────────────────────────────────────────
   Semantic value union — types that tokens carry through the parse
───────────────────────────────────────────────────────────────────*/
%union {
    char     *sval;    /* STRING and NUMBER literals       */
    int       bval;    /* TRUE / FALSE boolean literals    */
    /* The fields below will be populated in Step 3 */
    /* CondNode      *cond; */
    /* StatementNode *stmt; */
    /* ProgramNode   *prog; */
}

/* ─────────────────────────────────────────────────────────────────
   Token declarations  (must match exactly what policy_lexer.l emits)
───────────────────────────────────────────────────────────────────*/

/* Effect keywords */
%token KW_ALLOW
%token KW_DENY

/* Structural keywords */
%token KW_ROLE
%token KW_ACTION
%token KW_ON
%token KW_RESOURCE
%token KW_WHERE

/* Logical operators */
%token KW_AND
%token KW_OR
%token KW_NOT

/* Boolean literals — carry an int value */
%token <bval> KW_TRUE
%token <bval> KW_FALSE

/* Attribute keywords (WHERE clause LHS) */
%token ATTR_IP
%token ATTR_TIME
%token ATTR_MFA
%token ATTR_REGION
%token ATTR_TAG

/* Relational operators */
%token OP_EQ
%token OP_NEQ
%token OP_LT
%token OP_GT
%token OP_LEQ
%token OP_GEQ

/* Punctuation */
%token SEMICOLON

/* Value tokens — carry a heap-allocated string */
%token <sval> STRING
%token <sval> NUMBER

/* ─────────────────────────────────────────────────────────────────
   Operator precedence (for condition expressions)
   Lower declarations bind tighter.
───────────────────────────────────────────────────────────────────*/
%left  KW_OR
%left  KW_AND
%right KW_NOT

%%

/* ═════════════════════════════════════════════════════════════════
   GRAMMAR RULES  (Step 3 — to be filled in)
   Placeholder rule keeps Bison happy during Step 2 testing.
═════════════════════════════════════════════════════════════════ */

program
    : /* empty */
    | program statement
    ;

statement
    : KW_ALLOW KW_ROLE STRING KW_ACTION STRING KW_ON KW_RESOURCE STRING SEMICOLON
        {
            /* Step 3: build StatementNode and attach to program */
            printf("[Parser stub] Parsed ALLOW statement for role '%s'\n", $3);
            free($3); free($5); free($8);
        }
    | KW_DENY KW_ROLE STRING KW_ACTION STRING KW_ON KW_RESOURCE STRING SEMICOLON
        {
            printf("[Parser stub] Parsed DENY statement for role '%s'\n", $3);
            free($3); free($5); free($8);
        }
    | KW_ALLOW KW_ROLE STRING KW_ACTION STRING KW_ON KW_RESOURCE STRING KW_WHERE condition SEMICOLON
        {
            printf("[Parser stub] Parsed ALLOW+WHERE statement for role '%s'\n", $3);
            free($3); free($5); free($8);
        }
    | KW_DENY KW_ROLE STRING KW_ACTION STRING KW_ON KW_RESOURCE STRING KW_WHERE condition SEMICOLON
        {
            printf("[Parser stub] Parsed DENY+WHERE statement for role '%s'\n", $3);
            free($3); free($5); free($8);
        }
    ;

condition
    : simple_cond                         { /* Step 3 */ }
    | condition KW_AND condition          { /* Step 3 */ }
    | condition KW_OR  condition          { /* Step 3 */ }
    | KW_NOT simple_cond                  { /* Step 3 */ }
    ;

simple_cond
    : attribute rel_op value              { /* Step 3 */ }
    ;

attribute
    : ATTR_IP     { /* Step 3 */ }
    | ATTR_TIME   { /* Step 3 */ }
    | ATTR_MFA    { /* Step 3 */ }
    | ATTR_REGION { /* Step 3 */ }
    | ATTR_TAG    { /* Step 3 */ }
    ;

rel_op
    : OP_EQ   { /* Step 3 */ }
    | OP_NEQ  { /* Step 3 */ }
    | OP_LT   { /* Step 3 */ }
    | OP_GT   { /* Step 3 */ }
    | OP_LEQ  { /* Step 3 */ }
    | OP_GEQ  { /* Step 3 */ }
    ;

value
    : STRING    { /* Step 3 */ }
    | NUMBER    { /* Step 3 */ }
    | KW_TRUE   { /* Step 3 */ }
    | KW_FALSE  { /* Step 3 */ }
    ;

%%
