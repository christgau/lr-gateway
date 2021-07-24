/* vim: set filetype=antlr3 */
grammar config;


options {
    language=C;
    output=AST;
    ASTLabelType=pANTLR3_BASE_TREE;
}


tokens {
    CONFIG;
    AUTOMATION;
    RECORD;
    EVENT;
    OBJECT;
    ACTIONLIST;
    FUNCTION;
    PARAMS;
    FOREACH;
    VALUE;
    TEMPLATES;
    COND_ACTION;
    OPERATIONS;


    AND='and';
    OR='or';

    ON='on';
    TABLE='table';
    COMMAND='command';
    CONNECTION='connection';
    CONNECTIONS='connections';
    ESTABLISHED='established';
    CLOSED='closed';
    TARGETS='targets';
    TEMPLATE='template';
    REQUESTS='requests';
    REQUEST='request';
    HTTP='http';
    ITERATE='iterate';
    END = 'end';
    UPDATE = 'update';
    COPY = 'copy';
    INTERVAL = 'every';
    SECONDS = 'seconds';

    COLON=':';
    SEMICOLON=';' ;
    COMMA=',';
    DOT='.';
    BRACKET_LEFT='(';
    BRACKET_RIGHT=')';
    CURLY_RIGHT='}';
    CURLY_LEFT='{';
}

configuration
    : connections? targets? (event)* (template_definition)* (requests_definition)? -> ^(CONFIG connections? targets?) ^(AUTOMATION event*) ^(TEMPLATES template_definition*) requests_definition?
    ;

connections
    : CONNECTIONS^ COLON! kv_list SEMICOLON!
    ;

targets
    : TARGETS^ COLON! kv_list SEMICOLON!
    ;

event
    : ON object COLON operations -> ^(EVENT object ^(OPERATIONS operations))
    ;

object
    : CONNECTION connection_state -> ^(CONNECTION connection_state)
    | TABLE^ IDENTIFIER
    | COMMAND^ STRING
    | INTERVAL^ INTEGER SECONDS
    ;

connection_state
    : ESTABLISHED
    | CLOSED
    ;

operations
    : (conditional_action|iterate)*
    ;

conditional_action
    : action
    | 'if' expr
	(
		action-> ^(COND_ACTION expr action)
		|
		ITERATE IDENTIFIER COLON conditional_action* END SEMICOLON -> ^(ITERATE IDENTIFIER ^(OPERATIONS (conditional_action)*) expr)
	)
    ;

iterate
    : ITERATE IDENTIFIER COLON conditional_action* END SEMICOLON -> ^(ITERATE IDENTIFIER ^(OPERATIONS (conditional_action)*))
    ;

action
    : HTTP^ http_method url=expr ('using'! IDENTIFIER ('for'! ('table' | ('each'! 'record' 'of'!)) IDENTIFIER)? | 'with'! 'value'! expr)? SEMICOLON!
    | REQUEST req=IDENTIFIER ('for' 'each' 'record' 'in') tbl=IDENTIFIER SEMICOLON -> ^(FOREACH $req $tbl)
    | REQUEST^ IDENTIFIER SEMICOLON!
	| UPDATE^ IDENTIFIER 'set'! 'field'! expr '='! expr SEMICOLON!
	| COPY^ 'table'! expr 'to'! expr SEMICOLON!
    ;

http_method
    : 'get'
    | 'put'
    | 'post'
    | 'delete'
    ;

template_definition
    : TEMPLATE^ IDENTIFIER COLON! kv_list
    ;

requests_definition
    : REQUESTS^ COLON! kv_list
    ;

kv_list
    : (kv_pair)*
    ;

kv_pair
    : IDENTIFIER COLON! expr
    ;

expr
    : simpleExpr ( (EQUAL^ | NOT_EQUAL^ | LTH^ | LE^ | GE^ | GT^ ) simpleExpr )*
    ;

simpleExpr
    : term ( (PLUS^ | MINUS^ | OR^) term )*
    ;

term
    : signedFactor ( (MUL^ | DIV^ | MOD^ | AND^) signedFactor )*
    ;

signedFactor
    : (PLUS^|MINUS^)? factor
    ;

factor
    : BRACKET_LEFT! expr BRACKET_RIGHT!
    | record
    | function
    | constant
    ;

record
    : CURLY_LEFT table=IDENTIFIER DOT column=IDENTIFIER CURLY_RIGHT -> ^(RECORD $table $column)
    | CURLY_LEFT table=IDENTIFIER DOT column=INTEGER CURLY_RIGHT -> ^(RECORD $table $column)
    ;

function
    : name=IDENTIFIER BRACKET_LEFT parameters BRACKET_RIGHT -> ^(FUNCTION $name parameters)
    ;

parameters
    : expr (COMMA expr)* -> ^(PARAMS expr+)
    ;

constant
    : INTEGER
    | STRING
    ;

ML_COMMENT
    : '/*' (options {greedy=false;} : .)* '*/' { SKIP(); }
    ;

SL_COMMENT
    : '#' ~(NEWLINE)* { SKIP(); } ;

INTEGER     : (DIGIT)+ ;
IDENTIFIER  : (LETTER|UNDERSCORE)(LETTER|UNDERSCORE|DIGIT)*(EXLAM)?;
STRING      : ('"' ~('"')* '"')  { SETTEXT(GETTEXT()->subString(GETTEXT(), 1, GETTEXT()->len-2));}
            | ('\'' ~('\'')* '\'') { SETTEXT(GETTEXT()->subString(GETTEXT(), 1, GETTEXT()->len-2));} ;

fragment DIGIT
    : '0'..'9';

fragment LETTER
    : 'A'..'Z' | 'a'..'z';

fragment UNDERSCORE
    : '_';

fragment EXLAM
    : '!';

PLUS            : '+'   ;
MINUS           : '-'   ;
MUL             : '*'   ;
MOD             : '%'   ;
DIV             : '/'   ;
EQUAL           : '='
                | '=='  ;
NOT_EQUAL       : '<>'
                | '!='  ;
LTH             : '<'   ;
LE              : '<='  ;
GE              : '>='  ;
GT              : '>'   ;

fragment NEWLINE: '\n' | '\r';
WS      : (' '|'\t'|NEWLINE)+ { SKIP(); } ;
