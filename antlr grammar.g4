// ANTLR4 grammar for the RANE bootstrap language as implemented in this repository.
//
// IMPORTANT:
// - This grammar is intended to match the *implemented* parser in `rane_parser.cpp`.
// - The lexer in this repo tokenizes many additional keywords (reserved/planned). They are
//   included here as tokens so editors/highlighters can recognize them, but most are NOT
//   reachable from `stmt` (same as the current C++ parser).
//
// Targets: documentation, syntax highlighting, and external tooling.
// Not currently wired into the compiler build.

grammar Rane;

///////////////////////////////////////////////////////////////////////////
// Parser rules (Implemented surface: `rane_parser.cpp`)
///////////////////////////////////////////////////////////////////////////

program
  : stmtOrSemi* EOF
  ;

stmtOrSemi
  : stmt SEMI?
  | SEMI
  ;

stmt
  // v1 terminator is handled by enclosing construct; allow it in grammar for tooling.
  : END                                            #stmtEnd

  // v1 prose/node surface
  | MODULE IDENTIFIER                              #stmtModule
  | NODE IDENTIFIER COLON stmtOrSemi* END SEMI?     #stmtNode
  | START AT_KW NODE? IDENTIFIER                    #stmtStartAt
  | GO TO_KW NODE? IDENTIFIER                       #stmtGoNode
  | SAY expr                                        #stmtSay

  // v1 structured-data surface
  | STRUCT IDENTIFIER COLON v1StructField* END      #stmtV1StructDecl
  | SET v1SetRhs                                    #stmtV1Set
  | ADD v1AddRhs                                    #stmtV1Add

  // Core statements (subset actually handled in current `parse_stmt`)
  | BREAK                                           #stmtBreak
  | CONTINUE                                        #stmtContinue

  | GOTO expr ARROW IDENTIFIER COMMA IDENTIFIER     #stmtCJump

  | CALL IDENTIFIER LPAREN argList? RPAREN (INTO SLOT INT_LITERAL)?  #stmtCallStmt

  | CHAN PUSH IDENTIFIER COMMA expr                 #stmtChanPush
  | CHAN POP  IDENTIFIER INTO IDENTIFIER            #stmtChanPop

  | MMIO REGION IDENTIFIER FROM INT_LITERAL SIZE INT_LITERAL         #stmtMmioRegion
  | READ32 IDENTIFIER COMMA expr INTO IDENTIFIER                     #stmtRead32
  | WRITE32 IDENTIFIER COMMA expr COMMA expr                          #stmtWrite32

  | MEM COPY expr COMMA expr COMMA expr             #stmtMemCopy
  | MEM ZERO expr COMMA expr                        #stmtMemZero
  | MEM FILL expr COMMA expr COMMA expr             #stmtMemFill

  | HALT                                            #stmtHalt
  | (TRAP_OP | TRAP) expr?                          #stmtTrap

  // NOTE: `let`, `if`, `while`, `{...}`, `return`, `label`, etc exist in the AST
  // but are not currently parsed by the shown `parse_stmt()` implementation branch.
  ;

v1StructField
  : IDENTIFIER COLON IDENTIFIER SEMI?  // fieldName ':' typeName
  ;

v1SetRhs
  // decl form: set x: Ty to expr
  : IDENTIFIER COLON IDENTIFIER TO_KW expr
  // assign form: set targetExpr to expr   (targetExpr: ident ('.' ident)*)
  | v1TargetExpr TO_KW expr
  ;

v1AddRhs
  : v1TargetExpr BY expr
  ;

v1TargetExpr
  : IDENTIFIER (DOT IDENTIFIER)*
  ;

argList
  : expr (COMMA expr)*
  ;

///////////////////////////////////////////////////////////////////////////
// Expressions (Implemented: `parse_expr`, `parse_unary_expr`, `parse_postfix_expr`, `parse_primary_expr`)
///////////////////////////////////////////////////////////////////////////

expr
  : ternaryExpr
  ;

ternaryExpr
  : binaryExpr (QUESTION expr COLON expr)?
  ;

binaryExpr
  : unaryExpr (binOp unaryExpr)*
  ;

binOp
  : STAR
  | SLASH
  | PERCENT
  | PLUS
  | MINUS
  | CARET
  | AMP
  | PIPE
  | LT
  | LE
  | GT
  | GE
  | EQ
  | NE
  | ASSIGN            // v1 compatibility: '=' treated as equality in expressions
  | AND
  | OR
  | ANDAND
  | OROR
  | SHL
  | SHR
  | SAR
  | XOR
  ;

unaryExpr
  : MINUS unaryExpr
  | (NOT | EXCLAM) unaryExpr
  | TILDE unaryExpr
  | postfixExpr
  ;

postfixExpr
  : primaryExpr (postfixOp)*
  ;

postfixOp
  : DOT IDENTIFIER
  | LBRACKET expr RBRACKET
  | LPAREN argList? RPAREN
  ;

primaryExpr
  : HASH IDENTIFIER               #exprIdentLiteral
  | NULL                          #exprNullLiteral
  | BOOL_LITERAL                  #exprBoolLiteral
  | INT_LITERAL                   #exprIntLiteral
  | STRING_LITERAL                #exprStringLiteral

  | CHOOSE (MAX | MIN)? LPAREN expr COMMA expr RPAREN  #exprChoose

  | LPAREN expr RPAREN              #exprParen

  // Identifier-driven primary:
  // - v1 struct literals: TypeName{...} or TypeName(...)
  // - special calls: addr(...), load(...), store(...)
  // - normal calls: foo(...)
  // - variables: foo
  | IDENTIFIER
    (
      // v1 struct literal named-fields
      LBRACE (v1NamedField ( (COMMA | SEMI)? v1NamedField )* (COMMA | SEMI)? )? RBRACE
      // v1 struct literal positional
    | LPAREN argList? RPAREN
      // if none, then it is a variable
    )?                              #exprIdentifierDriven
  ;

v1NamedField
  : IDENTIFIER COLON expr
  ;

///////////////////////////////////////////////////////////////////////////
// Lexer rules
//
// These tokens mirror the repo lexer (`rane_lexer.cpp` / `rane_lexer.h`).
// Many are reserved/planned and not used by parser rules above.
///////////////////////////////////////////////////////////////////////////

MODULE      : 'module';
NODE        : 'node';
START       : 'start';
AT_KW       : 'at';
GO          : 'go';
TO_KW       : 'to';
SAY         : 'say';

SET         : 'set';
PUT         : 'put';
BY          : 'by';
END         : 'end';

LET         : 'let';
IF          : 'if';
THEN        : 'then';
ELSE        : 'else';
WHILE       : 'while';
DO          : 'do';
FOR         : 'for';
BREAK       : 'break';
CONTINUE    : 'continue';
RETURN      : 'return';
RET         : 'ret';

DEF         : 'proc' | 'def';
CALL        : 'call';

IMPORT      : 'import';
EXPORT      : 'export';
INCLUDE     : 'include';
EXCLUDE     : 'exclude';

DECIDE      : 'decide';
CASE        : 'case';
DEFAULT     : 'default';

JUMP        : 'jump';
GOTO        : 'goto';
MARK        : 'mark';
LABEL       : 'label';

GUARD       : 'guard';
ZONE        : 'zone';
HOT         : 'hot';
COLD        : 'cold';
DETERMINISTIC : 'deterministic';

REPEAT      : 'repeat';
UNROLL      : 'unroll';

TRUE        : 'true';
FALSE       : 'false';
NOT         : 'not';
AND         : 'and';
OR          : 'or';
XOR         : 'xor';

SHL         : 'shl';
SHR         : 'shr';
SAR         : 'sar';

TRY         : 'try';
CATCH       : 'catch';
THROW       : 'throw';

DEFINE      : 'define';
IFDEF       : 'ifdef';
IFNDEF      : 'ifndef';
PRAGMA      : 'pragma';
NAMESPACE   : 'namespace';
ENUM        : 'enum';
STRUCT      : 'struct';
CLASS       : 'class';
PUBLIC      : 'public';
PRIVATE     : 'private';
PROTECTED   : 'protected';
STATIC      : 'static';
INLINE      : 'inline';
EXTERN_KW   : 'extern';
VIRTUAL     : 'virtual';
CONST       : 'const';
VOLATILE    : 'volatile';
CONSTEXPR   : 'constexpr';
CONSTEVAL   : 'consteval';
CONSTINIT   : 'constinit';

NEW         : 'new';
DEL         : 'del';
CAST        : 'cast';
TYPE        : 'type';
TYPEALIAS   : 'typealias';
ALIAS       : 'alias';
MUT         : 'mut';
IMMUTABLE   : 'immutable';
MUTABLE     : 'mutable';
NULL        : 'null';
MATCH       : 'match';
PATTERN     : 'pattern';
LAMBDA      : 'lambda';

HANDLE      : 'handle';
TARGET      : 'target';
SPLICE      : 'splice';
SPLIT       : 'split';
DIFFERENCE  : 'difference';
INCREMENT   : 'increment';
DECREMENT   : 'decrement';
DEDICATE    : 'dedicate';
MUTEX       : 'mutex';
IGNORE      : 'ignore';
BYPASS      : 'bypass';
ISOLATE     : 'isolate';
SEPARATE    : 'separate';
JOIN        : 'join';
DECLARATION : 'declaration';
COMPILE     : 'compile';
SCORE       : 'score';
SYS         : 'sys';
ADMIN       : 'admin';
PLOT        : 'plot';
PEAK        : 'peak';
POINT       : 'point';
REG         : 'reg';
EXCEPTION   : 'exception';
ALIGN       : 'align';
MUTATE      : 'mutate';
STRING_KW   : 'string';
LITERAL     : 'literal';
LINEAR      : 'linear';
NONLINEAR   : 'nonlinear';
PRIMITIVES  : 'primitives';
TUPLES      : 'tuples';
MEMBER      : 'member';
OPEN        : 'open';
CLOSE       : 'close';

// Core builtins / statements used by current parser
MMIO        : 'mmio';
REGION      : 'region';
READ32      : 'read32';
WRITE32     : 'write32';
MEM         : 'mem';
COPY        : 'copy';
ZERO        : 'zero';
FILL        : 'fill';
CHAN        : 'chan';
PUSH        : 'push';
POP         : 'pop';
INTO        : 'into';
SLOT        : 'slot';
FROM        : 'from';
SIZE        : 'size';

HALT        : 'halt';
TRAP        : 'trap';
TRAP_OP     : 'trap_op';

CHOOSE      : 'choose';
MAX         : 'max';
MIN         : 'min';

// Operators / punctuation
EQ          : '==';
NE          : '!=';
LE          : '<=';
GE          : '>=';
LT          : '<';
GT          : '>';

ANDAND      : '&&';
OROR        : '||';

ASSIGN      : '=';
MAPS_TO     : '=>';
ARROW       : '->';
ASSIGN_INTO : '<-';
DOUBLE_COLON: '::';

COLON       : ':';
SEMI        : ';';
COMMA       : ',';
DOT         : '.';
LPAREN      : '(';
RPAREN      : ')';
LBRACE      : '{';
RBRACE      : '}';
LBRACKET    : '[';
RBRACKET    : ']';

HASH        : '#';
AT          : '@';
DOLLAR      : '$';
BACKSLASH   : '\\';
UNDERSCORE  : '_';

PLUS        : '+';
MINUS       : '-';
STAR        : '*';
SLASH       : '/';
PERCENT     : '%';
AMP         : '&';
PIPE        : '|';
CARET       : '^';
TILDE       : '~';
QUESTION    : '?';
EXCLAM      : '!';

// Literals
BOOL_LITERAL
  : TRUE
  | FALSE
  ;

// Matches decimal, 0x..., 0b..., and underscores (parser strips underscores)
// This is intentionally permissive to match current lexer behavior.
INT_LITERAL
  : '0' [xX] [0-9a-fA-F_]+
  | '0' [bB] [01_]+
  | [0-9] [0-9_]*
  ;

// Lexer in repo returns inner contents without quotes; keep quotes in token for tooling.
STRING_LITERAL
  : '"' ( '\\' . | ~["\\] )* '"'
  ;

// Identifiers
IDENTIFIER
  : [A-Za-z_] [A-Za-z0-9_]*
  ;

// Whitespace/comments
WS
  : [ \t\r\n]+ -> skip
  ;

LINE_COMMENT
  : '//' ~[\r\n]* -> skip
  ;

BLOCK_COMMENT
  : '/*' .*? '*/' -> skip
  ;
