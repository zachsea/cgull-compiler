grammar cgull;

program: top_level_statement+ EOF ;

/* ----- expressions ----- */

expression
    : base_expression
    | if_expression
    | when_expression
    ;

base_expression
    : '(' expression ')'
    | index_expression
    | dereference_expression
    | reference_expression
    | literal
    | function_call
    | allocate_expression
    | cast_expression
    | unary_expression
    | tuple_expression
    | array_expression
    | variable
    | function_expression
    | base_expression (MULT_OP | DIV_OP | MOD_OP) base_expression
    | base_expression (PLUS_OP | MINUS_OP) base_expression
    | base_expression (BITWISE_LEFT_SHIFT_OP | BITWISE_RIGHT_SHIFT_OP) base_expression
    | base_expression (LESS_OP | GREATER_OP | LESS_EQUAL_OP | GREATER_EQUAL_OP) base_expression
    | base_expression (EQUAL_OP | NOT_EQUAL_OP) base_expression
    | base_expression BITWISE_AND_OP base_expression
    | base_expression BITWISE_XOR_OP base_expression
    | base_expression BITWISE_OR_OP base_expression
    | base_expression AND_OP base_expression
    | base_expression OR_OP base_expression
    ;

expression_list
    : expression (',' expression)*
    ;

variable
    : IDENTIFIER
    | field_access
    ;

variable_list
    : variable (',' variable)+
    ;

index_expression
    : indexable '[' expression ']'
    ;

indexable
    : IDENTIFIER
    | '(' expression ')'
    | allocate_expression
    | indexable '[' expression ']'
    | indexable '(' expression_list? ')'
    | indexable ('.' | '->') IDENTIFIER
    ;

dereference_expression
    : '*' dereferenceable
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
    | CHAR_LITERAL
    | STRING_LITERAL
    | HEX_LITERAL
    | BINARY_LITERAL
    | BOOLEAN_TRUE
    | BOOLEAN_FALSE
    ;

function_call
    : ((FN_SPECIAL? IDENTIFIER) | index_expression | generic_identifier) '(' expression_list? ')'
    ;

allocate_expression
    : ALLOCATE (allocate_primitive | allocate_array | allocate_struct | allocate_function)
    ;

allocate_primitive
    : primitive_type ('(' expression ')')?
    ;

allocate_array
    : type '[' expression ']'
    ;

allocate_struct
    : (IDENTIFIER | generic_identifier) '(' expression_list? ')'
    ;

allocate_function
    : function_type function_expression
    ;

function_expression
    : '(' parameter_list? ')' function_block
    ;

parameter
    : type IDENTIFIER
    ;

parameter_list
    : parameter (',' parameter)*
    ;

field_access
    : field (('.' | '->') field)+
    ;

field_function_call
    : field (('.' | '->') field)* ('.' | '->') function_call
    ;

field
    : (function_call | IDENTIFIER | index_expression)
    ;

cast_expression
    : IDENTIFIER (BITS_AS_CAST | AS_CAST) primitive_type
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

array_expression
    : '{' expression_list '}'
    ;

if_expression
    : base_expression IF base_expression ELSE base_expression
    ;

when_expression
    : WHEN '(' expression ')' when_block
    ;

when_block
    : '{' when_entry* '}'
    ;

when_entry
    : when_pattern WHEN_ARROW (expression | when_block_expr) ','?
    ;

when_pattern
    : literal
    | when_negation
    | when_range
    | when_default
    ;

when_range
    : literal (INCLUSIVE_RANGE | EXCLUSIVE_RANGE) literal
    ;

when_negation:
    NOT_OP (literal | when_range | when_range)
    ;

when_default
    : '_'
    ;

when_block_expr
    : '{' function_level_statement* return_statement? '}'
    ;

/* ----- statements ----- */

top_level_statement
    : interface_definition
    | struct_definition
    | function_definition
    | assignment_statement
    | variable_declaration
    | destructuring_statement
    ;

interface_definition
    : INTERFACE (IDENTIFIER | generic_identifier) '{' interface_body '}'
    ;

interface_body
    : interface_statement+
    ;

interface_statement
    : FN IDENTIFIER '(' parameter_list? ')' ( '->' '(' type_list ')' | '->' type_list )? SEMICOLON
    ;

struct_definition
    : STRUCT (IDENTIFIER | generic_definition) (':' interface_list)? '{' struct_body '}'
    ;

struct_statement
    : variable_declaration | special_function_definition | function_definition
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

interface_list
    : (IDENTIFIER | generic_identifier | generic_definition) (',' (IDENTIFIER | generic_identifier | generic_definition))*
    ;

function_definition
    : FN IDENTIFIER '(' parameter_list? ')' ( '->' '(' type_list ')' | '->' type_list )? function_block
    ;

special_function_definition
    : FN FN_SPECIAL IDENTIFIER overridable_operators? '(' parameter_list? ')' ( '->' '(' type_list ')' | '->' type_list )? function_block
    ;

overridable_operators
    : (PLUS_OP | MINUS_OP | MULT_OP | DIV_OP | MOD_OP | INCREMENT_OP | DECREMENT_OP | EQUAL_OP | NOT_EQUAL_OP | LESS_OP | GREATER_OP | LESS_EQUAL_OP | GREATER_EQUAL_OP
      | AND_OP | OR_OP | BITWISE_AND_OP | BITWISE_OR_OP | BITWISE_XOR_OP | BITWISE_NOT_OP | BITWISE_LEFT_SHIFT_OP | BITWISE_RIGHT_SHIFT_OP | ARRAY_OP)
    ;

assignment_statement
    : (dereference_expression | index_expression | variable) (ASSIGN | op_assign) expression SEMICOLON
    ;

op_assign
    : (PLUS_OP | MINUS_OP | MULT_OP | DIV_OP | MOD_OP | BITWISE_AND_OP | BITWISE_OR_OP | BITWISE_XOR_OP | BITWISE_NOT_OP | BITWISE_LEFT_SHIFT_OP | BITWISE_RIGHT_SHIFT_OP) ASSIGN
    ;

variable_declaration
    : CONST? type IDENTIFIER (ASSIGN expression)? SEMICOLON
    ;

variable_declaration_list
    : type IDENTIFIER (',' type IDENTIFIER)+
    ;

destructuring_statement
    : destructuring_declaration
    | destructuring_assignment
    ;
destructuring_assignment
    : variable_list ASSIGN expression SEMICOLON
    ;
destructuring_declaration
    : variable_declaration_list ASSIGN expression SEMICOLON
    ;

function_level_statement
    : struct_definition
    | loop_statement
    | assignment_statement
    | variable_declaration
    | return_statement
    | deallocate_statement
    | function_call_statement
    | throw_statement
    | try_statement
    | if_statement
    | unary_statement
    | break_statement
    | destructuring_statement
    | when_statement
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
    : FOR '(' for_init SEMICOLON expression SEMICOLON expression ')' branch_block
    ;

for_init
    : CONST? type IDENTIFIER (ASSIGN expression)?
    | for_declaration
    ;

for_declaration
    : variable_declaration | for_function_declaration
    ;

for_function_declaration
    : function_type IDENTIFIER (ASSIGN function_expression)?
    ;

infinite_loop_statement
    : FOR branch_block ;

return_statement
    : RETURN expression_list? SEMICOLON
    ;

deallocate_statement
    : DEALLOCATE ARRAY_OP? IDENTIFIER SEMICOLON
    ;

function_call_statement
    : (function_call | field_function_call) SEMICOLON
    ;

throw_statement
    : THROW EXCEPTION '<' IDENTIFIER '>' '(' expression ')' SEMICOLON
    ;

try_statement
    : TRY branch_block (handle_block)* (ELSE_IF '(' expression ')' branch_block)* (ELSE branch_block)? (FINALLY branch_block)?
    ;

handle_block
    : HANDLE '(' exception_type IDENTIFIER ')' branch_block
    ;

exception_type
    : EXCEPTION '<' IDENTIFIER '>' | EXCEPTION
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

when_statement
    : when_expression
    ;

/* ----- types ----- */

type
    : base_type array_suffix? '*'?
    ;

array_suffix
    : ARRAY_OP
    ;

base_type
    : primitive_type | user_defined_type | function_type
    ;

type_list
    : type (',' type)*
    ;

primitive_type
    : (SIGNED_TYPE | UNSIGNED_TYPE)? (INT_TYPE | SHORT_TYPE | LONG_TYPE | CHAR_TYPE)
    | FLOAT_TYPE | DOUBLE_TYPE | STRING_TYPE | BOOLEAN_TYPE | VOID_TYPE
    ;

user_defined_type
    : IDENTIFIER
    | generic_identifier
    ;

function_type
    : FN '<' type_list '->' type_list '>'
    ;

generic_identifier
    : IDENTIFIER '<' type_list '>'
    ;

generic_definition
    : IDENTIFIER '<' identifier_list '>'
    ;

identifier_list
    : IDENTIFIER (',' IDENTIFIER)*
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

THROW: 'throw' ;
TRY: 'try' ;
HANDLE: 'handle' ;
FINALLY: 'finally' ;
EXCEPTION: 'exception' ;

WHEN: 'when' ;
WHEN_ARROW: '=>' ;
INCLUSIVE_RANGE: '..=' ;
EXCLUSIVE_RANGE: '..' ;

INT_TYPE: 'int' ;
SHORT_TYPE: 'short' ;
LONG_TYPE: 'long' ;
FLOAT_TYPE: 'float' ;
CHAR_TYPE: 'char' ;
DOUBLE_TYPE: 'double' ;
STRING_TYPE: 'string' ;
BOOLEAN_TYPE: 'bool' ;
VOID_TYPE: 'void' ;
UNSIGNED_TYPE: 'unsigned' ;
SIGNED_TYPE: 'signed' ;

STRUCT: 'struct' ;
INTERFACE: 'interface' ;
STATIC: 'static' ;
CONST: 'const' ;
PUBLIC: 'public' ;
PRIVATE: 'private' ;
ALLOCATE: 'allocate' ;
DEALLOCATE: 'deallocate' ;

NUMBER_LITERAL: [0-9]+ ;
DECIMAL_LITERAL: [0-9]+ '.' [0-9]+ ;
FLOAT_POSINF_LITERAL: '\'+inf\'' ;
FLOAT_NEGINF_LITERAL: '\'-inf\'' ;
FLOAT_NAN_LITERAL: 'nan' ;
CHAR_LITERAL: '\'' . '\'' ;
// fragments for escaping characters
STRING_LITERAL: '"' (ESC | ~["\\\r\n])* '"' ;
fragment ESC: '\\' (["\\/bfnrt] | UNICODE) ;
fragment UNICODE: 'u' HEX HEX HEX HEX ;
fragment HEX: [0-9a-fA-F] ;
HEX_LITERAL: '0x' [0-9a-fA-F]+ ;
BINARY_LITERAL: '0b' [01]+ ;
BOOLEAN_TRUE: 'true' ;
BOOLEAN_FALSE: 'false' ;

WS: [ \t\r\n]+ -> skip ;
COMMENT: '//' ~[\r\n]* -> skip ;
MULTILINE_COMMENT: '/*' .*? '*/' -> skip ;

IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]* ;
