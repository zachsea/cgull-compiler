grammar cgull;

program: top_level_statement+ EOF ;

/* ----- expressions ----- */

expression
    : base_expression
    ;

base_expression
    : '(' expression ')'
    | allocate_expression
    | index_expression
    | dereference_expression
    | reference_expression
    | literal
    | function_call
    | cast_expression
    | unary_expression
    | tuple_expression
    | variable
    | base_expression (MULT_OP | DIV_OP | MOD_OP) base_expression
    | base_expression (PLUS_OP | MINUS_OP) base_expression
    | base_expression (BITWISE_LEFT_SHIFT_OP | BITWISE_RIGHT_SHIFT_OP) base_expression
    | base_expression BITWISE_AND_OP base_expression
    | base_expression BITWISE_XOR_OP base_expression
    | base_expression BITWISE_OR_OP base_expression
    | base_expression (LESS_OP | GREATER_OP | LESS_EQUAL_OP | GREATER_EQUAL_OP) base_expression
    | base_expression (EQUAL_OP | NOT_EQUAL_OP) base_expression
    | base_expression AND_OP base_expression
    | base_expression OR_OP base_expression
    | if_expression
    ;

expression_list
    : expression (',' expression)*
    ;

variable
    : IDENTIFIER
    | field_access
    ;

index_expression
    : indexable '[' expression ']'
    ;

indexable
    : IDENTIFIER
    | '(' expression ')'
    | indexable '[' expression ']'
    | indexable '(' expression_list? ')'
    ;

dereference_expression
    : '*'+ dereferenceable
    ;

dereferenceable
    : IDENTIFIER
    | function_call
    | allocate_expression
    | field_access
    | index_expression
    ;

reference_expression
    : '&' expression
    ;

literal
    : NUMBER_LITERAL
    | DECIMAL_LITERAL
    | FLOAT_POSINF_LITERAL
    | FLOAT_NEGINF_LITERAL
    | FLOAT_NAN_LITERAL
    | STRING_LITERAL
    | HEX_LITERAL
    | BINARY_LITERAL
    | BOOLEAN_TRUE
    | BOOLEAN_FALSE
    | NULLPTR_LITERAL
    ;

function_call
    : (FN_SPECIAL? IDENTIFIER | index_expression) '(' expression_list? ')'
    ;

allocate_expression
    : ALLOCATE (allocate_primitive | allocate_array | allocate_struct)
    ;

allocate_primitive
    : primitive_type ('(' expression ')')?
    ;

allocate_array
    : type '[' expression ']'
    ;

allocate_struct
    : IDENTIFIER '(' expression_list? ')'
    ;

parameter
    : CONST? type IDENTIFIER
    ;

parameter_list
    : parameter (',' parameter)*
    ;

// this should be a normal operator later, too late to change now...
access_operator
    : '.' | '->'
    ;

field_access
    : field (access_operator field)+
    ;

field
    : (function_call | IDENTIFIER | index_expression | '(' expression ')')
    ;

cast_expression
    : (IDENTIFIER | '(' expression ')') (BITS_AS_CAST | AS_CAST) primitive_type
    ;

unary_expression
    : postfix_expression
    | (PLUS_OP | MINUS_OP | NOT_OP) expression
    | (INCREMENT_OP | DECREMENT_OP) expression
    | (BITWISE_NOT_OP) expression
    ;

postfix_expression
    : (IDENTIFIER | function_call | field_access | '(' expression ')') (INCREMENT_OP | DECREMENT_OP)
    ;

tuple_expression
    : '(' expression_list ')'
    ;

if_expression
    : IF base_expression base_expression ELSE base_expression
    ;

/* ----- statements ----- */

top_level_statement
    : struct_definition
    | function_definition
    | assignment_statement
    | variable_declaration_statement
    | destructuring_statement
    ;

struct_definition
    : STRUCT IDENTIFIER '{' struct_body '}'
    ;

struct_statement
    : variable_declaration_statement | function_definition
    ;

struct_body
    : (access_block | top_level_struct_statement)*
    ;

top_level_struct_statement
    : (PUBLIC | PRIVATE)? struct_statement
    ;

access_block
    : (PUBLIC | PRIVATE) '{' struct_statement* '}'
    ;

function_definition
    : FN (IDENTIFIER | FN_SPECIAL IDENTIFIER) '(' parameter_list? ')' ( '->' '(' type_list ')' | '->' type )? function_block
    ;

assignment_statement
    : (dereference_expression | index_expression | variable) ASSIGN expression SEMICOLON
    ;

variable_declaration
    : CONST? type IDENTIFIER (ASSIGN expression)?
    ;

variable_declaration_statement
    : variable_declaration SEMICOLON
    ;

variable_declaration_list
    : variable_declaration (',' variable_declaration)+
    ;

destructuring_statement
    : destructuring_list ASSIGN expression SEMICOLON
    ;

destructuring_list
    : destructuring_item (',' destructuring_item)*
    ;

destructuring_item
    : CONST? type IDENTIFIER
    | variable
    ;

function_level_statement
    : loop_statement
    | assignment_statement
    | variable_declaration_statement
    | return_statement
    | deallocate_statement
    | function_call_statement
    | if_statement
    | unary_statement
    | break_statement
    | destructuring_statement
    ;

function_block
    : '{' function_level_statement* '}'
    ;

branch_block
    : '{' (function_level_statement | BREAK SEMICOLON)* '}'
    ;

loop_statement
    : until_statement
    | while_statement
    | for_statement
    | infinite_loop_statement
    ;

until_statement
    : FOR branch_block UNTIL '(' expression ')' SEMICOLON
    ;

while_statement
    : FOR '(' expression ')' branch_block
    ;

for_statement
    : FOR '(' variable_declaration? SEMICOLON expression SEMICOLON expression ')' branch_block
    ;

infinite_loop_statement
    : FOR branch_block ;

return_statement
    : RETURN expression? SEMICOLON
    ;

deallocate_statement
    : DEALLOCATE ARRAY_OP? IDENTIFIER SEMICOLON
    ;

function_call_statement
    : (function_call | field_access) SEMICOLON
    ;

if_statement
    : IF '(' expression ')' branch_block (ELSE_IF '(' expression ')' branch_block)* (ELSE branch_block)?
    ;

unary_statement
    : (INCREMENT_OP | DECREMENT_OP) expression SEMICOLON
    | postfix_expression SEMICOLON
    ;

break_statement
    : BREAK SEMICOLON
    ;

/* ----- types ----- */

type
    : (primitive_type | user_defined_type | tuple_type) array_suffix? '*'*
    ;

array_suffix
    : ARRAY_OP
    ;

type_list
    : type (',' type)*
    ;

primitive_type
    : INT_TYPE | LONG_TYPE
    | FLOAT_TYPE | STRING_TYPE | BOOLEAN_TYPE | VOID_TYPE
    ;

user_defined_type
    : IDENTIFIER
    ;

tuple_type
    : TUPLE '<' type_list '>'
    ;

/* ----- lexer ----- */

FN: 'fn' ;
FN_SPECIAL: '$' ;
RETURN: 'return' ;

ASSIGN: '=' ;
SEMICOLON: ';' ;

PLUS_OP: '+' ;
MINUS_OP: '-' ;
MULT_OP: '*' ;
DIV_OP: '/' ;
MOD_OP: '%' ;
INCREMENT_OP: '++' ;
DECREMENT_OP: '--' ;
EQUAL_OP: '==' ;
NOT_EQUAL_OP: '!=' ;
LESS_OP: '<' ;
GREATER_OP: '>' ;
LESS_EQUAL_OP: '<=' ;
GREATER_EQUAL_OP: '>=' ;
AND_OP: '&&' ;
OR_OP: '||' ;
NOT_OP: '!' ;
BITWISE_AND_OP: '&' ;
BITWISE_OR_OP: '|' ;
BITWISE_XOR_OP: '^' ;
BITWISE_NOT_OP: '~' ;
BITWISE_LEFT_SHIFT_OP: '<<' ;
BITWISE_RIGHT_SHIFT_OP: '>>' ;

ARRAY_OP: '[]' ;

BITS_AS_CAST: 'bits as' ;
AS_CAST: 'as' ;

IF: 'if' ;
ELSE_IF: 'else if' ;
ELSE: 'else' ;

FOR: 'for' ;
UNTIL: 'until' ;
BREAK: 'break' ;

INCLUSIVE_RANGE: '..=' ;
EXCLUSIVE_RANGE: '..' ;

INT_TYPE: 'int' ;
LONG_TYPE: 'long' ;
FLOAT_TYPE: 'float' ;
STRING_TYPE: 'string' ;
BOOLEAN_TYPE: 'bool' ;
VOID_TYPE: 'void' ;

STRUCT: 'struct' ;
TUPLE: 'tuple' ;
CONST: 'const' ;
PUBLIC: 'public' ;
PRIVATE: 'private' ;
ALLOCATE: 'allocate' ;
DEALLOCATE: 'deallocate' ;

NUMBER_LITERAL: [0-9]+ 'L'? ;
DECIMAL_LITERAL: [0-9]+ '.' [0-9]+ ;
FLOAT_POSINF_LITERAL: '\'+inf\'' ;
FLOAT_NEGINF_LITERAL: '\'-inf\'' ;
FLOAT_NAN_LITERAL: 'nan' ;
// fragments for escaping characters
STRING_LITERAL: '"' (ESC | ~["\\\r\n])* '"' ;
fragment ESC: '\\' (["\\/bfnrt] | UNICODE) ;
fragment UNICODE: 'u' HEX HEX HEX HEX ;
fragment HEX: [0-9a-fA-F] ;
HEX_LITERAL: '0x' [0-9a-fA-F]+ ;
BINARY_LITERAL: '0b' [01]+ ;
BOOLEAN_TRUE: 'true' ;
BOOLEAN_FALSE: 'false' ;
NULLPTR_LITERAL: 'nullptr' ;

WS: [ \t\r\n]+ -> skip ;
COMMENT: '//' ~[\r\n]* -> skip ;
MULTILINE_COMMENT: '/*' .*? '*/' -> skip ;

IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]* ;
