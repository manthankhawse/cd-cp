%{
/*
 * policy_lexer.l -- Flex Lexical Analyzer for CloudPol DSL
 * Cloud Policy as Code DSL Compiler
 *
 * Step 2: Lexical Analysis
 *
 * This Flex specification scans raw CloudPol source text and emits a
 * typed token stream for the Bison parser (Step 3).
 *
 * Features:
 *   - Full keyword recognition (ALLOW, DENY, ROLE, ACTION, ON,
 *     RESOURCE, WHERE, AND, OR, NOT, TRUE, FALSE)
 *   - Attribute keywords (ip, time, mfa, region, tag)
 *   - Quoted string literals with value capture
 *   - Numeric literals (integer and decimal)
 *   - Multi-char operators (==, !=, <=, >=) and single-char (<, >)
 *   - Statement terminator (;)
 *   - Single-line comments  // ...
 *   - Block comments        scanned with input() loop
 *   - Column tracking for precise error reporting
 *   - Graceful error recovery on illegal characters
 *
 * Build (standalone token printer):
 *   flex -o build/lex.standalone.c src/policy_lexer.l
 *   gcc  -Wall -g -I./include -DLEXER_STANDALONE \
 *        -o build/policy_lexer build/lex.standalone.c -lfl
 *
 * Build (full compiler):
 *   make all
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Token definitions:
 *   Standalone mode  -> use self-contained include/tokens.h (no Bison needed)
 *   Full compiler    -> use Bison-generated policy_parser.tab.h
 */
#ifdef LEXER_STANDALONE
#  include "tokens.h"
   /* yylval definition for standalone mode */
   YYSTYPE yylval;
#else
#  include "policy_parser.tab.h"
#endif

/*
 * Column tracking.
 * yylineno is managed by Flex (%option yylineno).
 * yycolno is updated via YY_USER_ACTION before every action.
 */
int yycolno = 1;

#define YY_USER_ACTION  yycolno += yyleng;

/* Forward declaration */
static void strip_quotes(const char *src, char *dst, size_t dst_len);

%}

/* Flex options */
%option noyywrap
%option nounput
%option yylineno

/* Named character classes */
DIGIT       [0-9]
WS          [ \t\r]
STRING_BODY [^"\n]*

%%

 /* ================================================================
    1. WHITESPACE -- skip silently
    ================================================================ */

{WS}+       { /* skip horizontal whitespace */ }
\n          { yycolno = 1; }


 /* ================================================================
    2. COMMENTS
    ================================================================ */

"//"[^\n]*      { /* single-line comment -- consume to end of line */ }

"/*"            {
                    /* block comment -- consume until closing */
                    int c, prev = 0;
                    while ((c = input()) != EOF) {
                        if (c == '\n') { yycolno = 1; }
                        else           { yycolno++; }
                        if (prev == '*' && c == '/') break;
                        prev = c;
                    }
                    if (c == EOF) {
                        fprintf(stderr,
                            "[CloudPol Lexer] ERROR line %d:%d: "
                            "unterminated block comment\n",
                            yylineno, yycolno);
                    }
                }


 /* ================================================================
    3. EFFECT KEYWORDS
    ================================================================ */

"ALLOW"     { return KW_ALLOW; }
"DENY"      { return KW_DENY;  }


 /* ================================================================
    4. STRUCTURAL KEYWORDS
    ================================================================ */

"ROLE"      { return KW_ROLE;     }
"ACTION"    { return KW_ACTION;   }
"ON"        { return KW_ON;       }
"RESOURCE"  { return KW_RESOURCE; }
"WHERE"     { return KW_WHERE;    }


 /* ================================================================
    5. LOGICAL OPERATOR KEYWORDS
    ================================================================ */

"AND"       { return KW_AND; }
"OR"        { return KW_OR;  }
"NOT"       { return KW_NOT; }


 /* ================================================================
    6. BOOLEAN LITERALS
    ================================================================ */

"TRUE"      { yylval.bval = 1; return KW_TRUE;  }
"FALSE"     { yylval.bval = 0; return KW_FALSE; }


 /* ================================================================
    7. ATTRIBUTE KEYWORDS (WHERE clause LHS -- all lowercase)
       Lowercase keeps attributes visually distinct from keywords
       and makes the grammar LLM-friendly (rigid, unambiguous set).
    ================================================================ */

"ip"        { return ATTR_IP;     }
"time"      { return ATTR_TIME;   }
"mfa"       { return ATTR_MFA;    }
"region"    { return ATTR_REGION; }
"tag"       { return ATTR_TAG;    }


 /* ================================================================
    8. RELATIONAL OPERATORS (longest-match rules listed first)
    ================================================================ */

"=="        { return OP_EQ;  }
"!="        { return OP_NEQ; }
"<="        { return OP_LEQ; }
">="        { return OP_GEQ; }
"<"         { return OP_LT;  }
">"         { return OP_GT;  }


 /* ================================================================
    9. STATEMENT TERMINATOR
    ================================================================ */

";"         { return SEMICOLON; }


 /* ================================================================
    10. STRING LITERALS -- capture body, strip surrounding quotes
    ================================================================ */

\"{STRING_BODY}\"   {
                        size_t len = strlen(yytext);
                        /* len includes both quote chars; body = len-2 chars */
                        yylval.sval = (char *)malloc(len - 1);
                        strip_quotes(yytext, yylval.sval, len - 1);
                        return STRING;
                    }

\"[^\"\n]*          {
                        /* Unterminated string literal */
                        fprintf(stderr,
                            "[CloudPol Lexer] ERROR line %d:%d: "
                            "unterminated string literal: %s\n",
                            yylineno, yycolno, yytext);
                    }


 /* ================================================================
    11. NUMERIC LITERALS (integer and decimal)
    ================================================================ */

{DIGIT}+("."{DIGIT}+)?  {
                            yylval.sval = strdup(yytext);
                            return NUMBER;
                        }


 /* ================================================================
    12. CATCH-ALL -- report and continue (error recovery)
    ================================================================ */

.           {
                fprintf(stderr,
                    "[CloudPol Lexer] ERROR line %d:%d: "
                    "unexpected character '%s'\n",
                    yylineno, yycolno, yytext);
            }

%%

/*
 * strip_quotes
 * Copies the string body from src (which includes leading and trailing
 * double-quote characters) into dst, NUL-terminating the result.
 *   src = "some text"  -->  dst = some text
 */
static void strip_quotes(const char *src, char *dst, size_t dst_len) {
    size_t src_len = strlen(src);
    if (src_len < 2) { dst[0] = '\0'; return; }
    size_t body_len = src_len - 2;            /* skip leading and trailing " */
    if (body_len >= dst_len) body_len = dst_len - 1;
    memcpy(dst, src + 1, body_len);
    dst[body_len] = '\0';
}


/*
 * STANDALONE TEST ENTRY POINT
 *
 * When compiled with -DLEXER_STANDALONE, main() drives the lexer and
 * pretty-prints every token it produces. Used for Step 2 verification.
 *
 * Compile:
 *   make lexer-test
 * Run:
 *   cat samples/valid_policy.pol | ./build/policy_lexer
 */
#ifdef LEXER_STANDALONE

static const char *token_name(int tok) {
    switch (tok) {
        case KW_ALLOW:    return "KW_ALLOW";
        case KW_DENY:     return "KW_DENY";
        case KW_ROLE:     return "KW_ROLE";
        case KW_ACTION:   return "KW_ACTION";
        case KW_ON:       return "KW_ON";
        case KW_RESOURCE: return "KW_RESOURCE";
        case KW_WHERE:    return "KW_WHERE";
        case KW_AND:      return "KW_AND";
        case KW_OR:       return "KW_OR";
        case KW_NOT:      return "KW_NOT";
        case KW_TRUE:     return "KW_TRUE";
        case KW_FALSE:    return "KW_FALSE";
        case ATTR_IP:     return "ATTR_IP";
        case ATTR_TIME:   return "ATTR_TIME";
        case ATTR_MFA:    return "ATTR_MFA";
        case ATTR_REGION: return "ATTR_REGION";
        case ATTR_TAG:    return "ATTR_TAG";
        case OP_EQ:       return "OP_EQ";
        case OP_NEQ:      return "OP_NEQ";
        case OP_LT:       return "OP_LT";
        case OP_GT:       return "OP_GT";
        case OP_LEQ:      return "OP_LEQ";
        case OP_GEQ:      return "OP_GEQ";
        case SEMICOLON:   return "SEMICOLON";
        case STRING:      return "STRING";
        case NUMBER:      return "NUMBER";
        default:          return "UNKNOWN";
    }
}

int main(void) {
    int tok;
    printf("%-16s %-12s %s\n", "TOKEN", "LINE:COL", "VALUE");
    printf("%-16s %-12s %s\n",
           "----------------", "------------", "-----");
    while ((tok = yylex()) != 0) {
        char loc[32];
        snprintf(loc, sizeof(loc), "%d:%d", yylineno,
                 yycolno > yyleng ? yycolno - yyleng : 1);
        switch (tok) {
            case STRING:
            case NUMBER:
                printf("%-16s %-12s \"%s\"\n",
                       token_name(tok), loc, yylval.sval);
                free(yylval.sval);
                yylval.sval = NULL;
                break;
            case KW_TRUE:
            case KW_FALSE:
                printf("%-16s %-12s %s\n",
                       token_name(tok), loc,
                       yylval.bval ? "true" : "false");
                break;
            default:
                printf("%-16s %-12s\n", token_name(tok), loc);
        }
    }
    return 0;
}

#endif /* LEXER_STANDALONE */
