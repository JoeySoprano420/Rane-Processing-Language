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

  // ... (existing grammar above remains unchanged)

///////////////////////////////////////////////////////////////////////////
// Parser rules (Implemented surface: `rane_parser.cpp`)
// EXTENDED for full syntax.rane coverage
///////////////////////////////////////////////////////////////////////////

program
  : (stmtOrSemi | docComment)* EOF
  ;

stmtOrSemi
  : stmt SEMI?
  | SEMI
  ;

stmt
  // Existing rules...

  // --- ADDED: Core surface and control flow ---
  | LET IDENTIFIER ASSIGN expr                              #stmtLet
  | IF expr block (ELSE block)?                             #stmtIf
  | WHILE expr block                                        #stmtWhile
  | FOR forInit? expr? SEMI? expr? block                    #stmtFor
  | RETURN expr?                                            #stmtReturn
  | block                                                   #stmtBlock
  | LABEL IDENTIFIER COLON                                  #stmtLabel

  // --- ADDED: Proc/function definitions with modifiers/attributes ---
  | modifier* attribute* DEF IDENTIFIER LPAREN paramList? RPAREN block #stmtProcDef

  // --- ADDED: Enum, union, typealias, alias, macro, template, record, variant, event ---
  | ENUM IDENTIFIER (COLON IDENTIFIER)? enumBody            #stmtEnum
  | UNION IDENTIFIER unionBody                              #stmtUnion
  | TYPEALIAS IDENTIFIER ASSIGN typeExpr                    #stmtTypeAlias
  | ALIAS IDENTIFIER ASSIGN typeExpr                        #stmtAlias
  | MACRO IDENTIFIER macroParams ASSIGN macroBody           #stmtMacro
  | TEMPLATE LT templateParams GT stmt                      #stmtTemplate
  | RECORD IDENTIFIER recordBody                            #stmtRecord
  | VARIANT IDENTIFIER ASSIGN variantBody                   #stmtVariant
  | EVENT IDENTIFIER LPAREN paramList? RPAREN SEMI?         #stmtEvent

  // --- ADDED: Advanced error handling ---
  | TRY block (CATCH LPAREN IDENTIFIER RPAREN block)? (FINALLY block)? #stmtTry

  // --- ADDED: Pattern matching, decide, match guards ---
  | MATCH expr matchBody                                    #stmtMatch
  | DECIDE expr decideBody                                  #stmtDecide

  // --- ADDED: Attribute, annotation, doc comment ---
  | attribute stmt                                          #stmtAttrStmt
  | docComment stmt                                         #stmtDocStmt

  // --- ADDED: Misc advanced forms ---
  | ASYNC stmt                                              #stmtAsync
  | AWAIT expr                                              #stmtAwait
  | YIELD expr                                              #stmtYield
  | COROUTINE stmt                                          #stmtCoroutine
  | STATIC_ASSERT LPAREN expr COMMA STRING_LITERAL RPAREN   #stmtStaticAssert
  | IMPORT IDENTIFIER FROM STRING_LITERAL SEMI?              #stmtImportFrom
  | EXPORT stmt                                             #stmtExport
  | INCLUDE STRING_LITERAL SEMI?                            #stmtInclude
  | EXCLUDE STRING_LITERAL SEMI?                            #stmtExclude
  | WITH expr AS IDENTIFIER block                           #stmtWith
  | USING IDENTIFIER ASSIGN expr SEMI?                      #stmtUsing
  | DEFER block                                             #stmtDefer
  | SPAWN expr                                              #stmtSpawn
  | JOIN expr                                               #stmtJoin
  | SUBSCRIBE LPAREN IDENTIFIER COMMA expr RPAREN SEMI?     #stmtSubscribe
  | EMIT IDENTIFIER LPAREN argList? RPAREN SEMI?            #stmtEmit
  // ... (add more as needed for full coverage)

  // --- ADDED: Advanced memory, align, mutate, etc. ---
  | ALIGN LPAREN INT_LITERAL RPAREN STRUCT IDENTIFIER structBody #stmtAlignStruct
  | MUTATE v1TargetExpr TO_KW expr                              #stmtMutate

  // --- ADDED: Operator overloading, generics, constraints ---
  | OPERATOR overloadOp LPAREN paramList? RPAREN block          #stmtOpOverload

  // --- ADDED: Slicing, spread, destructure ---
  | LET destructure ASSIGN expr                                 #stmtDestructure

  // --- ADDED: Event, pipeline, monad, etc. ---
  | PIPELINE expr pipelineBody                                  #stmtPipeline

  // ... (existing rules continue)
  ;

block
  : LBRACE stmtOrSemi* RBRACE
  ;

forInit
  : LET IDENTIFIER ASSIGN expr SEMI?
  ;

paramList
  : param (COMMA param)*
  ;

param
  : IDENTIFIER (COLON typeExpr)? (ASSIGN expr)?
  ;

modifier
  : STATIC
  | INLINE
  | EXTERN_KW
  | VIRTUAL
  | CONST
  | VOLATILE
  | CONSTEXPR
  | CONSTEVAL
  | CONSTINIT
  | PUBLIC
  | PRIVATE
  | PROTECTED
  | HOT
  | COLD
  | DEDICATE
  | MUTEX
  | GUARD
  | ZONE
  | LINEAR
  | NONLINEAR
  | PRIMITIVES
  | TUPLES
  | HANDLE
  | TARGET
  | ISOLATE
  | SEPARATE
  | JOIN
  | DECLARATION
  | COMPILE
  | SCORE
  | SYS
  | ADMIN
  | PLOT
  | PEAK
  | POINT
  | REG
  | EXCEPTION
  | IGNORE
  | BYPASS
  | INCREMENT
  | DECREMENT
  ;

attribute
  : AT IDENTIFIER (LPAREN argList? RPAREN)?
  ;

docComment
  : DOC_COMMENT
  ;

enumBody
  : LBRACE enumField (COMMA enumField)* (COMMA)? RBRACE
  ;

enumField
  : IDENTIFIER (ASSIGN expr)?
  ;

unionBody
  : LBRACE unionField (COMMA unionField)* (COMMA)? RBRACE
  ;

unionField
  : IDENTIFIER COLON typeExpr
  ;

recordBody
  : LBRACE recordField (COMMA recordField)* (COMMA)? RBRACE
  ;

recordField
  : IDENTIFIER COLON typeExpr
  ;

variantBody
  : variantField (PIPE variantField)*
  ;

variantField
  : IDENTIFIER (LPAREN typeExpr (COMMA typeExpr)* RPAREN)?
  ;

macroParams
  : LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN
  ;

macroBody
  : expr
  ;

templateParams
  : templateParam (COMMA templateParam)*
  ;

templateParam
  : IDENTIFIER (COLON typeExpr)?
  ;

matchBody
  : LBRACE matchCase* RBRACE
  ;

matchCase
  : CASE pattern (IF expr)? COLON stmtOrSemi*
  | DEFAULT COLON stmtOrSemi*
  ;

decideBody
  : LBRACE decideCase* RBRACE
  ;

decideCase
  : CASE expr COLON stmtOrSemi*
  | DEFAULT COLON stmtOrSemi*
  ;

pattern
  : expr
  | LBRACKET pattern (COMMA pattern)* (COMMA)? RBRACKET
  | LPAREN pattern (COMMA pattern)* (COMMA)? RPAREN
  | IDENTIFIER
  | UNDERSCORE
  ;

destructure
  : LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN
  | LBRACKET IDENTIFIER (COMMA IDENTIFIER)* (COMMA)? RBRACKET
  ;

overloadOp
  : PLUS | MINUS | STAR | SLASH | PERCENT | CARET | AMP | PIPE | LT | GT | EQ | NE | LE | GE
  ;

pipelineBody
  : (PIPE expr)+
  ;

// ... (existing expression and lexer rules remain unchanged, but consider adding tokens for new keywords and forms as needed)

// ... (existing grammar above remains unchanged)

///////////////////////////////////////////////////////////////////////////
// 12) Hyper-extended parser rules for meta, async, reflection, and future forms
//     - These rules further expand parser coverage for all syntax.rane constructs.
//     - If not yet implemented, they serve as future test scaffolding.
///////////////////////////////////////////////////////////////////////////

// --- ADDED: async/await/yield/coroutine ---
stmt
  // ... (existing alternatives)
  | ASYNC modifier* attribute* DEF IDENTIFIER LPAREN paramList? RPAREN block         #stmtAsyncProcDef
  | AWAIT expr                                                                      #stmtAwaitExpr
  | YIELD expr                                                                      #stmtYieldExpr
  | COROUTINE modifier* attribute* DEF IDENTIFIER LPAREN paramList? RPAREN block     #stmtCoroutineProcDef

// --- ADDED: reflection/introspection/meta ---
  | META modifier* attribute* DEF IDENTIFIER LPAREN paramList? RPAREN block          #stmtMetaProcDef
  | TYPEOF LPAREN typeExpr RPAREN                                                    #stmtTypeof
  | FIELDS LPAREN typeExpr RPAREN                                                    #stmtFields
  | METHODS LPAREN typeExpr RPAREN                                                   #stmtMethods

// --- ADDED: annotation/derive/singleton ---
  | attribute* STRUCT IDENTIFIER COLON structFieldExt* END                           #stmtStructWithAttr
  | attribute* DEF IDENTIFIER LPAREN paramList? RPAREN block                         #stmtProcWithAttr

structFieldExt
  : IDENTIFIER COLON typeExpr (ASSIGN expr)? SEMI?
  ;

// --- ADDED: resource management ---
  | DEFER block                                                                     #stmtDeferBlock
  | USING IDENTIFIER ASSIGN expr SEMI?                                              #stmtUsingStmt

// --- ADDED: advanced error handling/result/option ---
  | IF expr block (ELSE block)?                                                     #stmtIf
  | MATCH expr matchBody                                                            #stmtMatch
  | TRY block (CATCH LPAREN IDENTIFIER RPAREN block)? (FINALLY block)?              #stmtTryCatchFinally

// --- ADDED: generics/constraints/templates ---
  | TEMPLATE LT templateParams GT DEF IDENTIFIER LPAREN paramList? RPAREN block      #stmtTemplateProcDef
  | TEMPLATE LT templateParams GT STRUCT IDENTIFIER COLON structFieldExt* END        #stmtTemplateStructDef

// --- ADDED: slicing/spread/rest ---
  | LET LBRACKET IDENTIFIER (COMMA DOTDOT tail=IDENTIFIER)? RBRACKET ASSIGN expr     #stmtArrayDestructure
  | LET LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN ASSIGN expr                     #stmtTupleDestructure

// --- ADDED: match guards ---
matchCase
  : CASE pattern (IF expr)? COLON stmtOrSemi*
  | DEFAULT COLON stmtOrSemi*
  ;

// --- ADDED: type unions/intersections ---
typeExpr
  : typeExpr PIPE typeExpr      #typeUnion
  | typeExpr AMP typeExpr       #typeIntersection
  | baseTypeExpr                #typeBase
  ;

baseTypeExpr
  : IDENTIFIER
  | IDENTIFIER LT typeExpr (COMMA typeExpr)* GT
  ;

// --- ADDED: static assert/compile-time eval ---
  | STATIC_ASSERT LPAREN expr COMMA STRING_LITERAL RPAREN                           #stmtStaticAssert
  | CONSTEVAL DEF IDENTIFIER LPAREN paramList? RPAREN block                        #stmtConstevalProcDef

// --- ADDED: doc comments ---
docComment
  : DOC_COMMENT
  ;

// --- ADDED: macro hygiene/expansion ---
  | MACRO IDENTIFIER macroParams ASSIGN macroBody                                   #stmtMacroDef

// --- ADDED: operator overloading ---
  | STRUCT IDENTIFIER COLON structFieldExt* operatorOverload* END                   #stmtStructWithOpOverload

operatorOverload
  : DEF overloadOp LPAREN paramList? RPAREN block
  ;

// --- ADDED: event/observer/publisher/subscriber ---
  | EVENT IDENTIFIER LPAREN paramList? RPAREN SEMI?                                 #stmtEventDecl
  | SUBSCRIBE LPAREN IDENTIFIER COMMA expr RPAREN SEMI?                             #stmtSubscribeStmt
  | EMIT IDENTIFIER LPAREN argList? RPAREN SEMI?                                    #stmtEmitStmt

// --- ADDED: pipeline/chain/monad ---
  | expr (PIPE expr)+                                                               #stmtPipelineExpr

// --- ADDED: advanced block/label forms ---
block
  : LBRACE stmtOrSemi* RBRACE
  | stmt                                                                             // single-statement block
  ;

label
  : IDENTIFIER COLON
  ;

// --- ADDED: doc comment token ---
DOC_COMMENT
  : '///' ~[\r\n]* -> channel(HIDDEN)
  ;

// ... (existing rules and lexer remain unchanged)

// ... (existing grammar above remains unchanged)

///////////////////////////////////////////////////////////////////////////
// 13) Ultra-extended parser rules for future, meta, and exotic forms
//     - These rules further expand parser coverage for all advanced/future RANE constructs.
//     - If not yet implemented, they serve as future test scaffolding.
///////////////////////////////////////////////////////////////////////////

// --- ADDED: advanced async/await/yield/coroutine forms ---
stmt
  // ... (existing alternatives)
  | ASYNC block                                                        #stmtAsyncBlock
  | AWAIT expr                                                         #stmtAwaitExpr2
  | YIELD expr                                                         #stmtYieldExpr2
  | COROUTINE block                                                    #stmtCoroutineBlock

// --- ADDED: advanced meta-programming and reflection ---
  | REFLECT expr                                                       #stmtReflect
  | META expr                                                          #stmtMetaExpr
  | TYPEOF LPAREN expr RPAREN                                          #stmtTypeofExpr
  | FIELDS LPAREN expr RPAREN                                          #stmtFieldsExpr
  | METHODS LPAREN expr RPAREN                                         #stmtMethodsExpr

// --- ADDED: advanced attribute/annotation/derive/singleton forms ---
  | AT IDENTIFIER (LPAREN argList? RPAREN)? stmt                       #stmtAttrStmt2
  | AT IDENTIFIER (LPAREN argList? RPAREN)? DEF IDENTIFIER LPAREN paramList? RPAREN block #stmtAttrProcDef

// --- ADDED: advanced resource management ---
  | DEFER block                                                        #stmtDeferBlock2
  | USING IDENTIFIER ASSIGN expr block                                 #stmtUsingBlock

// --- ADDED: advanced error handling/result/option forms ---
  | TRY block (CATCH LPAREN IDENTIFIER RPAREN block)? (FINALLY block)? #stmtTryCatchFinally2
  | THROW expr                                                         #stmtThrow

// --- ADDED: advanced generics/templates/constraints ---
  | TEMPLATE LT templateParams GT DEF IDENTIFIER LPAREN paramList? RPAREN block #stmtTemplateProcDef2
  | TEMPLATE LT templateParams GT STRUCT IDENTIFIER COLON structFieldExt* END   #stmtTemplateStructDef2

// --- ADDED: advanced slicing/spread/rest ---
  | LET LBRACKET IDENTIFIER (COMMA DOTDOT IDENTIFIER)? RBRACKET ASSIGN expr     #stmtArrayDestructure2
  | LET LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN ASSIGN expr                #stmtTupleDestructure2

// --- ADDED: advanced match guards and pattern forms ---
matchCase
  : CASE pattern (IF expr)? COLON stmtOrSemi*
  | DEFAULT COLON stmtOrSemi*
  ;

// --- ADDED: advanced type unions/intersections/optionals ---
typeExpr
  : typeExpr PIPE typeExpr      #typeUnion2
  | typeExpr AMP typeExpr       #typeIntersection2
  | typeExpr QUESTION           #typeOptional
  | baseTypeExpr                #typeBase2
  ;

baseTypeExpr
  : IDENTIFIER
  | IDENTIFIER LT typeExpr (COMMA typeExpr)* GT
  ;

// --- ADDED: advanced static assert/compile-time eval ---
  | STATIC_ASSERT LPAREN expr (COMMA STRING_LITERAL)? RPAREN                #stmtStaticAssert2
  | CONSTEVAL DEF IDENTIFIER LPAREN paramList? RPAREN block                 #stmtConstevalProcDef2

// --- ADDED: advanced macro hygiene/expansion ---
  | MACRO IDENTIFIER macroParams ASSIGN macroBody                           #stmtMacroDef2

// --- ADDED: advanced operator overloading ---
  | STRUCT IDENTIFIER COLON structFieldExt* operatorOverload* END           #stmtStructWithOpOverload2

operatorOverload
  : DEF overloadOp LPAREN paramList? RPAREN block
  ;

// --- ADDED: advanced event/observer/publisher/subscriber forms ---
  | EVENT IDENTIFIER LPAREN paramList? RPAREN SEMI?                         #stmtEventDecl2
  | SUBSCRIBE LPAREN IDENTIFIER COMMA expr RPAREN SEMI?                     #stmtSubscribeStmt2
  | EMIT IDENTIFIER LPAREN argList? RPAREN SEMI?                            #stmtEmitStmt2

// --- ADDED: advanced pipeline/chain/monad forms ---
  | expr (PIPE expr)+                                                       #stmtPipelineExpr2

// --- ADDED: advanced block/label forms ---
block
  : LBRACE stmtOrSemi* RBRACE
  | stmt                                                                   // single-statement block
  ;

label
  : IDENTIFIER COLON
  ;

// --- ADDED: advanced doc comment token ---
DOC_COMMENT
  : '///' ~[\r\n]* -> channel(HIDDEN)
  ;

// --- ADDED: advanced lambda, arrow, and function types ---
lambdaExpr
  : LAMBDA LPAREN paramList? RPAREN block
  | LPAREN paramList? RPAREN ARROW expr
  ;

functionType
  : DEF LPAREN paramList? RPAREN ARROW typeExpr
  ;

// --- ADDED: advanced tuple, array, and record destructuring ---
destructure
  : LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN
  | LBRACKET IDENTIFIER (COMMA IDENTIFIER)* (COMMA)? RBRACKET
  | IDENTIFIER DOTDOT IDENTIFIER
  ;

// --- ADDED: advanced pattern matching for tuples, arrays, and records ---
pattern
  : expr
  | LBRACKET pattern (COMMA pattern)* (COMMA)? RBRACKET
  | LPAREN pattern (COMMA pattern)* (COMMA)? RPAREN
  | IDENTIFIER
  | UNDERSCORE
  | IDENTIFIER COLON pattern
  | IDENTIFIER LPAREN pattern (COMMA pattern)* RPAREN
  ;

// --- ADDED: advanced pipeline/monad chaining ---
pipelineBody
  : (PIPE expr)+
  ;

// ... (existing rules and lexer remain unchanged)

// ... (existing grammar above remains unchanged)

///////////////////////////////////////////////////////////////////////////
// 14) Ultra-future and meta-programming parser rules for RANE
//     - These rules further expand parser coverage for all advanced, meta, and experimental forms.
//     - If not yet implemented, they serve as future test scaffolding.
///////////////////////////////////////////////////////////////////////////

// --- ADDED: advanced async/await/yield/coroutine/parallel forms ---
stmt
  // ... (existing alternatives)
  | ASYNC block                                                        #stmtAsyncBlock
  | AWAIT expr                                                         #stmtAwaitExpr3
  | YIELD expr                                                         #stmtYieldExpr3
  | COROUTINE block                                                    #stmtCoroutineBlock2
  | PARALLEL block                                                     #stmtParallelBlock

// --- ADDED: advanced meta-programming, reflection, and introspection ---
  | REFLECT expr                                                       #stmtReflect2
  | META expr                                                          #stmtMetaExpr2
  | TYPEOF LPAREN expr RPAREN                                          #stmtTypeofExpr2
  | FIELDS LPAREN expr RPAREN                                          #stmtFieldsExpr2
  | METHODS LPAREN expr RPAREN                                         #stmtMethodsExpr2
  | EVAL expr                                                          #stmtEvalExpr

// --- ADDED: advanced attribute/annotation/derive/singleton forms ---
  | AT IDENTIFIER (LPAREN argList? RPAREN)? stmt                       #stmtAttrStmt3
  | AT IDENTIFIER (LPAREN argList? RPAREN)? DEF IDENTIFIER LPAREN paramList? RPAREN block #stmtAttrProcDef2
  | AT IDENTIFIER (LPAREN argList? RPAREN)? STRUCT IDENTIFIER COLON structFieldExt* END #stmtAttrStructDef

// --- ADDED: advanced resource management and region/zone ---
  | DEFER block                                                        #stmtDeferBlock3
  | USING IDENTIFIER ASSIGN expr block                                 #stmtUsingBlock2
  | REGION IDENTIFIER block                                            #stmtRegionBlock
  | ZONE IDENTIFIER block                                              #stmtZoneBlock

// --- ADDED: advanced error handling/result/option forms ---
  | TRY block (CATCH LPAREN IDENTIFIER RPAREN block)? (FINALLY block)? #stmtTryCatchFinally3
  | THROW expr                                                         #stmtThrow2
  | CATCH LPAREN IDENTIFIER RPAREN block                               #stmtCatchBlock
  | FINALLY block                                                      #stmtFinallyBlock

// --- ADDED: advanced generics/templates/constraints/where ---
  | TEMPLATE LT templateParams GT DEF IDENTIFIER LPAREN paramList? RPAREN block (WHERE whereClause)? #stmtTemplateProcDef3
  | TEMPLATE LT templateParams GT STRUCT IDENTIFIER COLON structFieldExt* END (WHERE whereClause)?   #stmtTemplateStructDef3

whereClause
  : constraint (COMMA constraint)*
  ;

constraint
  : IDENTIFIER COLON typeExpr
  ;

// --- ADDED: advanced slicing/spread/rest/ellipsis ---
  | LET LBRACKET IDENTIFIER (COMMA DOTDOT IDENTIFIER)? RBRACKET ASSIGN expr     #stmtArrayDestructure3
  | LET LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN ASSIGN expr                #stmtTupleDestructure3
  | LET IDENTIFIER ELLIPSIS ASSIGN expr                                         #stmtEllipsisDestructure

ELLIPSIS : '...';

// --- ADDED: advanced match guards, pattern forms, and or-patterns ---
matchCase
  : CASE pattern (OR pattern)* (IF expr)? COLON stmtOrSemi*
  | DEFAULT COLON stmtOrSemi*
  ;

// --- ADDED: advanced type unions/intersections/optionals/nullable ---
typeExpr
  : typeExpr PIPE typeExpr      #typeUnion3
  | typeExpr AMP typeExpr       #typeIntersection3
  | typeExpr QUESTION           #typeOptional2
  | typeExpr EXCLAM             #typeNonNull
  | baseTypeExpr                #typeBase3
  ;

// --- ADDED: advanced static assert/compile-time eval/constexpr ---
  | STATIC_ASSERT LPAREN expr (COMMA STRING_LITERAL)? RPAREN                #stmtStaticAssert3
  | CONSTEXPR DEF IDENTIFIER LPAREN paramList? RPAREN block                 #stmtConstexprProcDef

// --- ADDED: advanced macro hygiene/expansion/inline macros ---
  | MACRO IDENTIFIER macroParams ASSIGN macroBody                           #stmtMacroDef3
  | INLINE MACRO IDENTIFIER macroParams ASSIGN macroBody                    #stmtInlineMacroDef

// --- ADDED: advanced operator overloading, user-defined literals ---
  | STRUCT IDENTIFIER COLON structFieldExt* operatorOverload* END           #stmtStructWithOpOverload3
  | DEF IDENTIFIER LITERAL LPAREN paramList? RPAREN block                   #stmtUserDefinedLiteral

// --- ADDED: advanced event/observer/publisher/subscriber forms ---
  | EVENT IDENTIFIER LPAREN paramList? RPAREN SEMI?                         #stmtEventDecl3
  | SUBSCRIBE LPAREN IDENTIFIER COMMA expr RPAREN SEMI?                     #stmtSubscribeStmt3
  | EMIT IDENTIFIER LPAREN argList? RPAREN SEMI?                            #stmtEmitStmt3
  | PUBLISH IDENTIFIER LPAREN argList? RPAREN SEMI?                         #stmtPublishStmt

PUBLISH : 'publish';

// --- ADDED: advanced pipeline/chain/monad forms ---
  | expr (PIPE expr)+                                                       #stmtPipelineExpr3
  | expr (ARROW expr)+                                                      #stmtArrowChainExpr

// --- ADDED: advanced block/label forms, goto, mark, jump ---
block
  : LBRACE stmtOrSemi* RBRACE
  | stmt                                                                   // single-statement block
  ;

label
  : IDENTIFIER COLON
  | MARK IDENTIFIER SEMI?
  ;

  | GOTO IDENTIFIER SEMI?                                                  #stmtGotoLabel
  | JUMP IDENTIFIER SEMI?                                                  #stmtJumpLabel

// --- ADDED: advanced doc comment token ---
DOC_COMMENT
  : '///' ~[\r\n]* -> channel(HIDDEN)
  ;

// --- ADDED: advanced lambda, arrow, and function types ---
lambdaExpr
  : LAMBDA LPAREN paramList? RPAREN block
  | LPAREN paramList? RPAREN ARROW expr
  | LBRACE paramList? ARROW expr RBRACE
  ;

functionType
  : DEF LPAREN paramList? RPAREN ARROW typeExpr
  | LPAREN paramList? RPAREN ARROW typeExpr
  ;

// --- ADDED: advanced tuple, array, and record destructuring ---
destructure
  : LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN
  | LBRACKET IDENTIFIER (COMMA IDENTIFIER)* (COMMA)? RBRACKET
  | IDENTIFIER DOTDOT IDENTIFIER
  | IDENTIFIER ELLIPSIS
  ;

// --- ADDED: advanced pattern matching for tuples, arrays, records, and or-patterns ---
pattern
  : expr
  | LBRACKET pattern (COMMA pattern)* (COMMA)? RBRACKET
  | LPAREN pattern (COMMA pattern)* (COMMA)? RPAREN
  | IDENTIFIER
  | UNDERSCORE
  | IDENTIFIER COLON pattern
  | IDENTIFIER LPAREN pattern (COMMA pattern)* RPAREN
  | pattern OR pattern
  ;

// --- ADDED: advanced pipeline/monad chaining ---
pipelineBody
  : (PIPE expr)+
  | (ARROW expr)+
  ;

// ... (existing rules and lexer remain unchanged)

// ... (existing grammar above remains unchanged)

///////////////////////////////////////////////////////////////////////////
// 15) Maximal meta, system, and experimental parser rules for RANE
//     - These rules further expand parser coverage for all advanced, meta, and system forms.
//     - If not yet implemented, they serve as future test scaffolding.
///////////////////////////////////////////////////////////////////////////

// --- ADDED: advanced system/admin/score/plot/peak/point/reg/exception forms ---
stmt
  // ... (existing alternatives)
  | SYSTEM stmt                                                        #stmtSystem
  | ADMIN stmt                                                         #stmtAdmin
  | SCORE stmt                                                         #stmtScore
  | PLOT stmt                                                          #stmtPlot
  | PEAK stmt                                                          #stmtPeak
  | POINT stmt                                                         #stmtPoint
  | REG stmt                                                           #stmtReg
  | EXCEPTION stmt                                                     #stmtException

// --- ADDED: advanced align/mutate/string/linear/nonlinear/primitives/tuples/member/open/close forms ---
  | ALIGN LPAREN INT_LITERAL RPAREN stmt                               #stmtAlign
  | MUTATE v1TargetExpr TO_KW expr                                     #stmtMutate2
  | STRING_KW stmt                                                     #stmtString
  | LINEAR stmt                                                        #stmtLinear
  | NONLINEAR stmt                                                     #stmtNonlinear
  | PRIMITIVES stmt                                                    #stmtPrimitives
  | TUPLES stmt                                                        #stmtTuples
  | MEMBER stmt                                                        #stmtMember
  | OPEN stmt                                                          #stmtOpen
  | CLOSE stmt                                                         #stmtClose

// --- ADDED: advanced isolate/separate/join/declaration/compile forms ---
  | ISOLATE stmt                                                       #stmtIsolate
  | SEPARATE stmt                                                      #stmtSeparate
  | JOIN stmt                                                          #stmtJoin2
  | DECLARATION stmt                                                   #stmtDeclaration
  | COMPILE stmt                                                       #stmtCompile

// --- ADDED: advanced ignore/bypass/mutex/dedicate/increment/decrement forms ---
  | IGNORE stmt                                                        #stmtIgnore
  | BYPASS stmt                                                        #stmtBypass
  | MUTEX stmt                                                         #stmtMutex
  | DEDICATE stmt                                                      #stmtDedicate
  | INCREMENT v1TargetExpr                                             #stmtIncrement
  | DECREMENT v1TargetExpr                                             #stmtDecrement

// --- ADDED: advanced pragma/define/ifdef/ifndef/mark/label forms ---
  | PRAGMA LPAREN argList? RPAREN                                      #stmtPragma
  | DEFINE IDENTIFIER (ASSIGN expr)?                                   #stmtDefine
  | IFDEF IDENTIFIER stmtOrSemi* END                                   #stmtIfdef
  | IFNDEF IDENTIFIER stmtOrSemi* END                                  #stmtIfndef
  | MARK IDENTIFIER SEMI?                                              #stmtMark
  | LABEL IDENTIFIER COLON                                             #stmtLabel2

// --- ADDED: advanced new/del/cast/type/typealias/alias/mut/immutable/mutable/null forms ---
  | NEW typeExpr LPAREN argList? RPAREN                                #stmtNew
  | DEL expr                                                           #stmtDel
  | CAST LPAREN typeExpr COMMA expr RPAREN                             #stmtCast
  | TYPE IDENTIFIER ASSIGN typeExpr                                    #stmtTypeDef
  | TYPEALIAS IDENTIFIER ASSIGN typeExpr                               #stmtTypeAlias2
  | ALIAS IDENTIFIER ASSIGN typeExpr                                   #stmtAlias2
  | MUT expr                                                           #stmtMut
  | IMMUTABLE expr                                                     #stmtImmutable
  | MUTABLE expr                                                       #stmtMutable
  | NULL                                                               #stmtNull

// --- ADDED: advanced repeat/unroll/deterministic/hot/cold/guard/zone forms ---
  | REPEAT stmt                                                        #stmtRepeat
  | UNROLL stmt                                                        #stmtUnroll
  | DETERMINISTIC stmt                                                 #stmtDeterministic
  | HOT stmt                                                           #stmtHot
  | COLD stmt                                                          #stmtCold
  | GUARD stmt                                                         #stmtGuard
  | ZONE IDENTIFIER block                                              #stmtZone2

// --- ADDED: advanced event/emit/subscribe/publish/handle/target/splice/split/difference forms ---
  | EVENT IDENTIFIER LPAREN paramList? RPAREN SEMI?                    #stmtEventDecl4
  | EMIT IDENTIFIER LPAREN argList? RPAREN SEMI?                       #stmtEmitStmt4
  | SUBSCRIBE LPAREN IDENTIFIER COMMA expr RPAREN SEMI?                #stmtSubscribeStmt4
  | PUBLISH IDENTIFIER LPAREN argList? RPAREN SEMI?                    #stmtPublishStmt2
  | HANDLE stmt                                                        #stmtHandle
  | TARGET stmt                                                        #stmtTarget
  | SPLICE stmt                                                        #stmtSplice
  | SPLIT stmt                                                         #stmtSplit
  | DIFFERENCE stmt                                                    #stmtDifference

// --- ADDED: advanced admin/score/sys/plot/peak/point/reg/exception/align/mutate forms as block wrappers ---
block
  : LBRACE stmtOrSemi* RBRACE
  | SYSTEM block
  | ADMIN block
  | SCORE block
  | PLOT block
  | PEAK block
  | POINT block
  | REG block
  | EXCEPTION block
  | ALIGN LPAREN INT_LITERAL RPAREN block
  | MUTATE v1TargetExpr TO_KW expr
  | stmt // single-statement block
  ;

// --- ADDED: advanced doc comment token (already present, but ensure not lost) ---
DOC_COMMENT
  : '///' ~[\r\n]* -> channel(HIDDEN)
  ;

// ... (existing rules and lexer remain unchanged)

