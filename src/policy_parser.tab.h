/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_BUILD_POLICY_PARSER_TAB_H_INCLUDED
# define YY_YY_BUILD_POLICY_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    KW_ALLOW = 258,                /* KW_ALLOW  */
    KW_DENY = 259,                 /* KW_DENY  */
    KW_ROLE = 260,                 /* KW_ROLE  */
    KW_ACTION = 261,               /* KW_ACTION  */
    KW_ON = 262,                   /* KW_ON  */
    KW_RESOURCE = 263,             /* KW_RESOURCE  */
    KW_WHERE = 264,                /* KW_WHERE  */
    KW_AND = 265,                  /* KW_AND  */
    KW_OR = 266,                   /* KW_OR  */
    KW_NOT = 267,                  /* KW_NOT  */
    KW_TRUE = 268,                 /* KW_TRUE  */
    KW_FALSE = 269,                /* KW_FALSE  */
    ATTR_IP = 270,                 /* ATTR_IP  */
    ATTR_TIME = 271,               /* ATTR_TIME  */
    ATTR_MFA = 272,                /* ATTR_MFA  */
    ATTR_REGION = 273,             /* ATTR_REGION  */
    ATTR_TAG = 274,                /* ATTR_TAG  */
    OP_EQ = 275,                   /* OP_EQ  */
    OP_NEQ = 276,                  /* OP_NEQ  */
    OP_LT = 277,                   /* OP_LT  */
    OP_GT = 278,                   /* OP_GT  */
    OP_LEQ = 279,                  /* OP_LEQ  */
    OP_GEQ = 280,                  /* OP_GEQ  */
    SEMICOLON = 281,               /* SEMICOLON  */
    STRING = 282,                  /* STRING  */
    NUMBER = 283                   /* NUMBER  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 35 "src/policy_parser.y"

    char     *sval;    /* STRING and NUMBER literals       */
    int       bval;    /* TRUE / FALSE boolean literals    */
    /* The fields below will be populated in Step 3 */
    /* CondNode      *cond; */
    /* StatementNode *stmt; */
    /* ProgramNode   *prog; */

#line 101 "build/policy_parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_BUILD_POLICY_PARSER_TAB_H_INCLUDED  */
