#pragma once

#include <stdint.h>

// RANE Lexer Tokens

typedef enum rane_token_type_e {
  TOK_EOF = 0,
  TOK_IDENTIFIER,
  TOK_INT_LITERAL,
  TOK_STRING_LITERAL,
  TOK_BOOL_LITERAL,
  TOK_KW_LET,
  TOK_KW_IF,
  TOK_KW_THEN,
  TOK_KW_ELSE,
  TOK_KW_WHILE,
  TOK_KW_JUMP,
  TOK_KW_GUARD,
  TOK_KW_DO,
  TOK_KW_REPEAT,
  TOK_KW_DECIDE,
  TOK_KW_PROCESS,
  TOK_KW_SLOT,
  TOK_KW_MMIO,
  TOK_KW_REGION,
  TOK_KW_READ32,
  TOK_KW_WRITE32,
  TOK_KW_IMPORT,
  TOK_KW_EXPORT,
  TOK_KW_ZONE,
  TOK_KW_HOT,
  TOK_KW_COLD,
  TOK_KW_DETERMINISTIC,
  TOK_KW_MAX,
  TOK_KW_MIN,
  TOK_KW_AS,
  TOK_KW_WITH,
  TOK_KW_TRAP,
  TOK_KW_CLAMP,
  TOK_KW_WRAP,
  TOK_KW_NARROW,
  TOK_KW_CHOOSE,
  TOK_KW_MEM,
  TOK_KW_COPY,
  TOK_KW_ZERO,
  TOK_KW_FILL,
  TOK_KW_PUSH,
  TOK_KW_POP,
  TOK_KW_CHAN,
  TOK_KW_DEF,
  TOK_KW_SIZE,
  TOK_KW_TIER1,
  TOK_KW_TIER2,
  TOK_KW_STUBS,
  TOK_KW_PERMIT,
  TOK_KW_REQUIRE,
  TOK_KW_TAINT,
  TOK_KW_SANITIZE,
  TOK_KW_UNSAFE,
  TOK_KW_PERMIT_ESCAPE,
  TOK_KW_RAW_POINTER,
  TOK_KW_WITHIN,
  TOK_KW_SCOPE,
  TOK_KW_BLOCK,
  TOK_KW_FROM,
  TOK_KW_TO,
  TOK_KW_INTO,
  TOK_KW_USING,
  TOK_KW_TABLE,
  TOK_KW_DEFAULT,
  TOK_KW_CASE,
  TOK_KW_GOTO,
  TOK_KW_MARK,
  TOK_KW_LABEL,
  TOK_KW_CALL,
  TOK_KW_RET,
  TOK_KW_HALT,
  TOK_KW_TRAP_OP,
  TOK_KW_NOT,
  TOK_KW_AND,
  TOK_KW_OR,
  TOK_KW_XOR,
  TOK_KW_SHL,
  TOK_KW_SHR,
  TOK_KW_SAR,
  TOK_KW_ADD,
  TOK_KW_SUB,
  TOK_KW_MUL,
  TOK_KW_DIV,
  TOK_KW_MOD,
  TOK_KW_EQ,
  TOK_KW_NE,
  TOK_KW_LT,
  TOK_KW_LE,
  TOK_KW_GT,
  TOK_KW_GE,
  TOK_KW_ASSIGN,
  TOK_KW_COLON,
  TOK_KW_SEMICOLON,
  TOK_KW_COMMA,
  TOK_KW_DOT,
  TOK_KW_LPAREN,
  TOK_KW_RPAREN,
  TOK_KW_LBRACE,
  TOK_KW_RBRACE,
  TOK_KW_LBRACKET,
  TOK_KW_RBRACKET,
  TOK_KW_ARROW,      // ->
  TOK_KW_ASSIGN_INTO, // <-
  TOK_KW_MAPS_TO,    // =>
  TOK_KW_FLOW,       // ->
  TOK_KW_DOUBLE_COLON, // ::
  TOK_KW_AT,
  TOK_KW_HASH,
  TOK_KW_DOLLAR,
  TOK_KW_PERCENT,
  TOK_KW_AMP,
  TOK_KW_STAR,
  TOK_KW_SLASH,
  TOK_KW_BACKSLASH,
  TOK_KW_PIPE,
  TOK_KW_CARET,
  TOK_KW_TILDE,
  TOK_KW_QUESTION,
  TOK_KW_EXCLAM,
  TOK_KW_PLUS,
  TOK_KW_MINUS,
  TOK_KW_LESS,
  TOK_KW_GREATER,
  TOK_KW_EQUAL,
  TOK_KW_UNDERSCORE,
  TOK_KW_BACKTICK,
  TOK_KW_QUOTE,
  TOK_KW_DOUBLEQUOTE,
  TOK_KW_NEWLINE,
  TOK_KW_TAB,
  TOK_KW_SPACE,
  TOK_ERROR
} rane_token_type_t;

typedef struct rane_token_s {
  rane_token_type_t type;
  const char* start;
  size_t length;
  int line;
  int col;
} rane_token_t;

typedef struct rane_lexer_s {
  const char* source;
  size_t source_len;
  size_t pos;
  int line;
  int col;
} rane_lexer_t;

// Initialize lexer
void rane_lexer_init(rane_lexer_t* lex, const char* src, size_t len);

// Get next token
rane_token_t rane_lexer_next(rane_lexer_t* lex);

// Token to string (for debugging)
const char* rane_token_type_str(rane_token_type_t type);