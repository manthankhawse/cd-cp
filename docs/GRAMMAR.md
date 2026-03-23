# CloudPol — Formal Grammar Specification

> Developer reference for the Context-Free Grammar (CFG) of the CloudPol DSL.
> This document is the authoritative source for the Flex/Bison implementation.

---

## 1. Grammar Notation

This document uses an extended **Backus-Naur Form (BNF)**:

| Notation | Meaning |
|---|---|
| `<name>` | Non-terminal symbol |
| `'KEYWORD'` | Terminal keyword token |
| `\|` | Alternation (OR) |
| `[ x ]` | Optional occurrence of x |
| `x+` | One or more occurrences of x |
| `x*` | Zero or more occurrences of x |

---

## 2. Top-Level Structure

```bnf
<program>   ::= <statement>+
```

A CloudPol source file is a non-empty sequence of policy statements.

---

## 3. Statement

```bnf
<statement> ::= <effect> 'ROLE' STRING 'ACTION' STRING
                          'ON' 'RESOURCE' STRING
                          [ 'WHERE' <condition> ]
                          ';'
```

**Terminals:**

| Terminal | Description |
|---|---|
| `ROLE`     | Keyword |
| `ACTION`   | Keyword |
| `ON`       | Preposition keyword |
| `RESOURCE` | Keyword |
| `WHERE`    | Clause introducer |
| `;`        | Statement terminator (SEMICOLON) |
| `STRING`   | Double-quoted string literal |

---

## 4. Effect

```bnf
<effect>    ::= 'ALLOW'
              | 'DENY'
```

---

## 5. Condition Expression

```bnf
<condition> ::= <simple_cond>
              | <condition> 'AND' <condition>
              | <condition> 'OR'  <condition>
              | 'NOT' <simple_cond>
```

**Operator precedence** (handled by Bison `%left` / `%right` declarations):

| Precedence | Operator | Associativity |
|---|---|---|
| 3 (highest) | `NOT` | Right |
| 2 | `AND` | Left |
| 1 (lowest)  | `OR`  | Left |

---

## 6. Simple Condition

```bnf
<simple_cond>   ::= <attribute> <rel_op> <value>
```

---

## 7. Attributes

```bnf
<attribute>     ::= 'ip'
                  | 'time'
                  | 'mfa'
                  | 'region'
                  | 'tag'
```

> **Design rationale:** The attribute set is intentionally minimal and rigid.
> Research (ModernBERT, F-Measure 0.32 on `condition` entities) shows AI models
> perform poorly on open-ended conditional generation. Restricting the attribute
> vocabulary to exactly five lowercase tokens dramatically reduces the LLM's
> margin for error.

---

## 8. Relational Operators

```bnf
<rel_op>        ::= '=='
                  | '!='
                  | '<'
                  | '>'
                  | '<='
                  | '>='
```

---

## 9. Values

```bnf
<value>         ::= STRING
                  | NUMBER
                  | 'TRUE'
                  | 'FALSE'
```

---

## 10. Token Definitions (Regular Expressions)

The following table documents the Flex patterns for every terminal.

| Token | Flex Pattern | Bison Name | Carries Value? |
|---|---|---|---|
| `ALLOW` | `"ALLOW"` | `KW_ALLOW` | No |
| `DENY` | `"DENY"` | `KW_DENY` | No |
| `ROLE` | `"ROLE"` | `KW_ROLE` | No |
| `ACTION` | `"ACTION"` | `KW_ACTION` | No |
| `ON` | `"ON"` | `KW_ON` | No |
| `RESOURCE` | `"RESOURCE"` | `KW_RESOURCE` | No |
| `WHERE` | `"WHERE"` | `KW_WHERE` | No |
| `AND` | `"AND"` | `KW_AND` | No |
| `OR` | `"OR"` | `KW_OR` | No |
| `NOT` | `"NOT"` | `KW_NOT` | No |
| `TRUE` | `"TRUE"` | `KW_TRUE` | `int bval` |
| `FALSE` | `"FALSE"` | `KW_FALSE` | `int bval` |
| `ip` | `"ip"` | `ATTR_IP` | No |
| `time` | `"time"` | `ATTR_TIME` | No |
| `mfa` | `"mfa"` | `ATTR_MFA` | No |
| `region` | `"region"` | `ATTR_REGION` | No |
| `tag` | `"tag"` | `ATTR_TAG` | No |
| `==` | `"=="` | `OP_EQ` | No |
| `!=` | `"!="` | `OP_NEQ` | No |
| `<` | `"<"` | `OP_LT` | No |
| `>` | `">"` | `OP_GT` | No |
| `<=` | `"<="` | `OP_LEQ` | No |
| `>=` | `">="` | `OP_GEQ` | No |
| `;` | `";"` | `SEMICOLON` | No |
| String | `\"[^"\n]*\"` | `STRING` | `char* sval` |
| Number | `[0-9]+(\.[0-9]+)?` | `NUMBER` | `char* sval` |

> [!NOTE]
> String values are heap-allocated (`strdup`) inside the lexer rules and
> **must be freed** by parser grammar actions after use to avoid memory leaks.

---

## 11. Yylval Union

The semantic value union declared in `policy_parser.y`:

```c
%union {
    char  *sval;   /* STRING, NUMBER literals — heap allocated */
    int    bval;   /* TRUE / FALSE → 1 / 0                    */
    /* Step 3 additions:                                        */
    /* CondNode      *cond;                                     */
    /* StatementNode *stmt;                                     */
    /* ProgramNode   *prog;                                     */
}
```

---

## 12. Source Location Tracking

The lexer maintains two global counters:

```c
int yylineno = 1;   /* current line number (1-indexed) */
int yycolno  = 1;   /* current column number (1-indexed) */
```

`yycolno` is updated via the `YY_USER_ACTION` macro (runs before every rule action):

```c
#define YY_USER_ACTION  yycolno += yyleng;
```

`yylineno` is incremented and `yycolno` reset to `1` on every `\n` match.

---

## 13. Grammar Conflict Analysis

| Potential Conflict | Resolution |
|---|---|
| `AND` vs `OR` precedence | `%left KW_OR` declared before `%left KW_AND` → AND binds tighter |
| `NOT` right-associativity | `%right KW_NOT` declared at highest precedence |
| Optional `WHERE` clause | Handled by two separate production rules (with/without WHERE) |
| Token lookahead | Grammar is **LALR(1)** — Bison default, no conflicts expected |

---

## 14. Context-Free Grammar — Quick Reference Card

```
program      → statement+

statement    → effect ROLE STRING ACTION STRING ON RESOURCE STRING ;
             | effect ROLE STRING ACTION STRING ON RESOURCE STRING WHERE condition ;

effect       → ALLOW | DENY

condition    → simple_cond
             | condition AND condition
             | condition OR  condition
             | NOT simple_cond

simple_cond  → attribute rel_op value

attribute    → ip | time | mfa | region | tag

rel_op       → == | != | < | > | <= | >=

value        → STRING | NUMBER | TRUE | FALSE
```

---

## 15. AST Node Types (Defined in `include/ast.h`)

| Node Type | C Struct | Used In |
|---|---|---|
| Program root | `ProgramNode` | Step 3 parser, Step 4 SMT |
| Policy statement | `StatementNode` | Step 3 parser, Step 5 codegen |
| Condition | `CondNode` | Step 3 parser, Step 4 SMT |
| Effect enum | `Effect` | `StatementNode.effect` |
| RelOp enum | `RelOp` | `CondNode.rel_op` |
| LogOp enum | `LogOp` | `CondNode.log_op` |

---

## 16. Future Grammar Extensions (Post-Step 5)

The following constructs are planned for later compiler phases:

| Feature | Proposed Syntax |
|---|---|
| Policy groups | `GROUP "<name>" { statements }` |
| Parenthesized conditions | `WHERE (cond1 AND cond2) OR cond3` |
| Duration conditions | `WHERE time BETWEEN 9 AND 17` |
| Tag key-value split | `WHERE tag.key == "env" AND tag.value == "prod"` |
