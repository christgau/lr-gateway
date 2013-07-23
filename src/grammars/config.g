/* vim: set filetype=antlrv3 */
grammar configuration;


options {
	language=C;
	output=AST;
	ASTLabelType=pANTLR3_BASE_TREE;
}


tokens {
	CONFIG;
	RECORD;
	FUNCTION;
	PARAMS;
	ITERATE;
	VALUE;

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
//	: connections targets (statement)* (template_definition)* (request_definition)? -> ^(CONFIG connections targets statement* template_definition* request_definition?)
	: connections targets (event)* (template_definition)* (requests_definition)?-> ^(CONFIG connections targets event* template_definition* requests_definition?) 
	;

connections
	: CONNECTIONS^ COLON! kv_list
	;

targets
	: TARGETS^ COLON! kv_list
	;

event
	: ON object COLON (action)* -> ^(object action*)
	;

object
	: connection
	| table_ref
	| command
	;

connection
	: CONNECTION connection_state -> ^(CONNECTION connection_state)
	;
	
connection_state
	: ESTABLISHED
	| CLOSED
	;

table_ref
	: TABLE^ IDENTIFIER
	;

command
	: COMMAND^ STRING
	;

action
	: HTTP http_method url=expr 'with' kind=('value' expr | 'template' IDENTIFIER) SEMICOLON -> ^(HTTP http_method $url $kind)
	| REQUEST IDENTIFIER ('for' 'each' 'entry') SEMICOLON -> ^(ITERATE IDENTIFIER)
	| REQUEST IDENTIFIER SEMICOLON -> ^(REQUEST IDENTIFIER)
	;

http_method
	: 'get'
	| 'put'
	| 'post'
	| 'delete'
	;
	
template_definition
	: TEMPLATE^ IDENTIFIER COLON! kv_list? SEMICOLON!
	;

kv_list
	: kv_pair (COMMA! kv_pair)*
	;
	
kv_pair
	: IDENTIFIER COLON! expr
	;

requests_definition
	: REQUESTS^ COLON! request_definition*
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

INTEGER		: (DIGIT)+ ;
IDENTIFIER	: (LETTER|UNDERSCORE)(LETTER|UNDERSCORE|DIGIT)* ;
STRING		: ('"' ~('"')* '"')  { SETTEXT(GETTEXT()->subString(GETTEXT(), 1, GETTEXT()->len-2));} 
		| ('\'' ~('\'')* '\'') { SETTEXT(GETTEXT()->subString(GETTEXT(), 1, GETTEXT()->len-2));} ;

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
