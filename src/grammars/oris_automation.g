/* vim: set filetype=antlrv3 */
grammar oris_automation;


options {
	language=C;
	output=AST;
	ASTLabelType=pANTLR3_BASE_TREE;
}


tokens {
	RECORD;
	FUNCTION;
	PARAMS;

	AND='and';
	OR='or';

	ON='on';
	TABLE='table';
	COMMAND='command';
	CONNECTION='connection';
	CONNECTIONS='connections';
	ESTABLISHED='established';
	CLOSED='closed';
	TEMPLATE='template';
	REQUEST='request';
	TARGETS='targets';
	HTTP='http';
	
	COLON=':';
	SEMICOLON=';' ;	
	COMMA=',';
	DOT='.';
	BRACKET_LEFT='(';
	BRACKET_RIGHT=')';
	CURLY_LEFT='{';
	CURLY_RIGHT='}';
}

configuration
	: (connections)? (targets)? (statement)*
	;

targets
	: ^TARGETS COLON! (kv_list)?
	;

connections
	: ^CONNECTIONS COLON! (kv_list)?
	;

statement
	: event
/*	| template_definition
	| request_definition*/
	;

event
	: ON object COLON (action)*
	;

object
	: connection
	| table_ref
	| command
	;

connection
	: CONNECTION connection_state
	;
	
connection_state
	: ESTABLISHED
	| CLOSED
	;

table_ref
	: TABLE IDENTIFIER
	;

command
	: COMMAND STRING
	;

action
	: HTTP ( 'get' | 'put' | 'post' | 'delete' ) expr 'with' ( 'value' expr | 'template' IDENTIFIER ) SEMICOLON
	| REQUEST IDENTIFIER ('for' 'each' 'entry')?
	;

template_definition:
	: TEMPLATE^ IDENTIFIER COLON! kv_list SEMICOLON!
	;

kv_list
	: kv_pair (COMMA! kv_pair)*
	|
	;
	
kv_pair
	: ^IDENTIFIER COLON! expr
	;

request_definition
	: REQUEST^ IDENTIFIER COLON! expr SEMICOLON!
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
	: name=IDENTIFIER BRACKET_LEFT BRACKET_RIGHT -> ^(FUNCTION $name ^(PARAMS))
	| name=IDENTIFIER BRACKET_LEFT expr (COMMA expr)* BRACKET_RIGHT -> ^(FUNCTION $name ^(PARAMS expr+))
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

INTEGER		: (DIGIT)+ ;
IDENTIFIER	: (LETTER|UNDERSCORE)(LETTER|UNDERSCORE|DIGIT)* ;
STRING		: ('"' ~('"')* '"') ;

fragment DIGIT
	: '0'..'9';

fragment LETTER
	: 'A'..'Z' | 'a'..'z';

fragment UNDERSCORE
	: '_';

PLUS            : '+'   ;
MINUS           : '-'   ;
MUL             : '*'   ;
MOD		: '%'   ;
DIV             : '/'   ;
EQUAL           : '='   
		| '=='	;
NOT_EQUAL       : '<>'  
		| '!='	;
LTH             : '<'   ;
LE              : '<='  ;
GE              : '>='  ;
GT              : '>'   ;

fragment NEWLINE: '\n' | '\r';
WS		: (' '|'\t'|NEWLINE)+ { SKIP(); } ;
