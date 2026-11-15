// Stella IR v1 syntax, corresponding to StellaIR.cf; see ../ir/README.md.
// Target-independent ANTLR 4 grammar: no embedded actions or predicates.
// Decoding checks (version, ranges, JSON, form) are separate; see README.md.
grammar StellaIR;
unit: 'stella-ir' DECIMAL form JSON_STRING ';' declaration* EOF;
form: 'cfg' | 'ssa' | 'functional';
declaration
 : 'export' CALLABLE (',' CALLABLE)* ';'
 | 'layout' LAYOUT 'fields' types 'capacity' DECIMAL 'updatable' booleanLiteral 'entry' (CALLABLE | 'none') ';'
 | 'frame' FRAME 'saved' types 'result' valueType 'target' CALLABLE ';'
 | 'function' CALLABLE parameters '->' valueType '{' body '}'
 | 'routine' CALLABLE parameters '{' body '}'
 ;
body: graph | expression;
graph: ('locals' parameters ';')? 'entry' DESTINATION ';' block+;
block: DESTINATION ':' instruction* graphExit;
instruction
 : results '=' rhs ';'
 | binding '=' 'phi' '{' (incoming (',' incoming)*)? '}' ';'
 ;
incoming: DESTINATION ':' atom;
rhs
 : scalarName arguments
 | 'call' CALLABLE arguments
 | 'run' CALLABLE arguments
 | 'runtime' JSON_STRING arguments attributes
 ;
scalarName: 'copy' | 'add' | 'sub' | 'mul' | 'div' | 'rem'
          | 'eq' | 'ne' | 'lt' | 'le' | 'gt' | 'ge' | 'not' | 'and' | 'or' | 'xor';
graphExit
 : 'jump' DESTINATION ';'
 | 'branch' atom ',' DESTINATION ',' DESTINATION ';'
 | commonExit
 ;
commonExit
 : 'return' atom ';'
 | 'tail' CALLABLE arguments ';'
 | 'tail-entry' types arguments ';'
 | 'resume' atom ';'
 | 'fail' JSON_STRING ';'
 ;
expression
 : 'let' results '=' rhs ';' expression
 | 'do' rhs ';' expression
 | 'if' atom '{' expression '}' 'else' '{' expression '}'
 | 'letrec' '{' localFunction+ '}' 'in' '{' expression '}'
 | 'jump' DESTINATION arguments ';'
 | commonExit
 ;
localFunction: 'local' DESTINATION parameters '{' expression '}';
parameters: '(' (binding (',' binding)*)? ')';
results: '[' (binding (',' binding)*)? ']';
binding: VALUE ':' valueType;
arguments: '(' (atom (',' atom)*)? ')';
types: '(' (valueType (',' valueType)*)? ')';
valueType: 'i64' | 'bool' | 'ref';
atom: VALUE | DECIMAL | booleanLiteral;
booleanLiteral: 'true' | 'false';
attributes: '{' (attribute (',' attribute)*)? '}';
attribute: JSON_STRING ':' staticValue;
staticValue: JSON_STRING | booleanLiteral | attributes | '[' (staticValue (',' staticValue)*)? ']';
VALUE: '%' NAME;
CALLABLE: '@' NAME;
LAYOUT: '$' NAME;
FRAME: '!' NAME;
DESTINATION: '^' NAME;
fragment NAME: [A-Za-z_] [A-Za-z_0-9]*;
DECIMAL: '0' | '-'? [1-9] [0-9]*;
JSON_STRING: '"' ( ~["\\\r\n] | '\\' (["\\/bfnrt] | 'u' HEX HEX HEX HEX) )* '"';
fragment HEX: [0-9a-fA-F];
LINE_COMMENT: '//' ~[\r\n]* -> channel(HIDDEN);
SPACE: [ \t\r\n]+ -> channel(HIDDEN);
