/**
 * tokens.h -- Standalone Token Definitions for CloudPol Lexer
 *
 * When compiling the lexer in LEXER_STANDALONE mode (without Bison),
 * this header provides the token enum and YYSTYPE union so policy_lexer.l
 * compiles independently for testing.
 *
 * In full compiler mode the Bison-generated policy_parser.tab.h is used
 * instead -- it contains the same definitions.
 *
 * NOTE: Token codes start at 258 to avoid collisions with ASCII literals
 * (Bison convention: 0-255 are single character tokens, 256+ are named).
 */

#ifndef TOKENS_H
#define TOKENS_H

/* Token codes -- must match %token declarations in policy_parser.y exactly */
enum TokenCode {
    /* Effect keywords */
    KW_ALLOW    = 258,
    KW_DENY     = 259,

    /* Structural keywords */
    KW_ROLE     = 260,
    KW_ACTION   = 261,
    KW_ON       = 262,
    KW_RESOURCE = 263,
    KW_WHERE    = 264,

    /* Logical operators */
    KW_AND      = 265,
    KW_OR       = 266,
    KW_NOT      = 267,

    /* Boolean literals */
    KW_TRUE     = 268,
    KW_FALSE    = 269,

    /* Attribute keywords (WHERE clause LHS) */
    ATTR_IP     = 270,
    ATTR_TIME   = 271,
    ATTR_MFA    = 272,
    ATTR_REGION = 273,
    ATTR_TAG    = 274,

    /* Relational operators */
    OP_EQ       = 275,
    OP_NEQ      = 276,
    OP_LT       = 277,
    OP_GT       = 278,
    OP_LEQ      = 279,
    OP_GEQ      = 280,

    /* Punctuation */
    SEMICOLON   = 281,

    /* Literal value tokens */
    STRING      = 282,
    NUMBER      = 283
};

/* Semantic value union -- mirrors the %union in policy_parser.y */
typedef union {
    char *sval;   /* STRING and NUMBER literals (heap-allocated) */
    int   bval;   /* TRUE (1) or FALSE (0)                       */
} YYSTYPE;

/* yylval is defined once in the standalone compilation unit (lex.standalone.c) */
extern YYSTYPE yylval;

#endif /* TOKENS_H */
