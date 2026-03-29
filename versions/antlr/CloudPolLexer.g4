lexer grammar CloudPolLexer;

/* Effect keywords */
KW_ALLOW : 'ALLOW';
KW_DENY  : 'DENY';

/* Structural keywords */
KW_ROLE     : 'ROLE';
KW_ACTION   : 'ACTION';
KW_ON       : 'ON';
KW_RESOURCE : 'RESOURCE';
KW_WHERE    : 'WHERE';

/* Logical operators */
KW_AND : 'AND';
KW_OR  : 'OR';
KW_NOT : 'NOT';

/* Boolean literals */
KW_TRUE  : 'TRUE';
KW_FALSE : 'FALSE';

/* Attribute keywords */
ATTR_IP     : 'ip';
ATTR_TIME   : 'time';
ATTR_MFA    : 'mfa';
ATTR_REGION : 'region';
ATTR_TAG    : 'tag';

/* Relational operators */
OP_EQ  : '==';
OP_NEQ : '!=';
OP_LEQ : '<=';
OP_GEQ : '>=';
OP_LT  : '<';
OP_GT  : '>';

/* Punctuation */
SEMICOLON : ';';

/* Literals */
STRING : '"' ~('"'|'\n')* '"';
NUMBER : [0-9]+ ('.' [0-9]+)?;

/* Whitespace and Comments */
WS : [ \t\r\n]+ -> skip;
LINE_COMMENT  : '//' ~[\r\n]* -> skip;
BLOCK_COMMENT : '/*' .*? '*/' -> skip;

/* Catch-all for error reporting in ANTLR */
UNKNOWN : . ;
