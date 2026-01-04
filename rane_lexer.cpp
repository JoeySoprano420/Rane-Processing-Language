#include "rane_lexer.h"
#include <ctype.h>
#include <string.h>

void rane_lexer_init(rane_lexer_t* lex, const char* src, size_t len) {
  lex->source = src;
  lex->source_len = len;
  lex->pos = 0;
  lex->line = 1;
  lex->col = 1;
}

static int is_at_end(rane_lexer_t* lex) {
  return lex->pos >= lex->source_len;
}

static char peek(rane_lexer_t* lex) {
  if (is_at_end(lex)) return '\0';
  return lex->source[lex->pos];
}

static char advance(rane_lexer_t* lex) {
  char c = peek(lex);
  lex->pos++;
  if (c == '\n') {
    lex->line++;
    lex->col = 1;
  } else {
    lex->col++;
  }
  return c;
}

static int match(rane_lexer_t* lex, char expected) {
  if (is_at_end(lex) || peek(lex) != expected) return 0;
  advance(lex);
  return 1;
}

static rane_token_t make_token(rane_lexer_t* lex, rane_token_type_t type, const char* start, size_t len) {
  rane_token_t tok;
  tok.type = type;
  tok.start = start;
  tok.length = len;
  tok.line = lex->line;
  tok.col = lex->col - len; // approximate
  return tok;
}

static rane_token_t error_token(rane_lexer_t* lex, const char* msg) {
  rane_token_t tok;
  tok.type = TOK_ERROR;
  tok.start = msg;
  tok.length = strlen(msg);
  tok.line = lex->line;
  tok.col = lex->col;
  return tok;
}

static void skip_whitespace(rane_lexer_t* lex) {
  while (!is_at_end(lex)) {
    char c = peek(lex);
    if (c == ' ' || c == '\t' || c == '\r') {
      advance(lex);
    } else if (c == '\n') {
      advance(lex);
    } else {
      break;
    }
  }
}

static rane_token_type_t check_keyword(rane_lexer_t* lex, size_t start, size_t len, const char* rest, rane_token_type_t type) {
  if (lex->pos - start == len && memcmp(lex->source + start, rest, len) == 0) {
    return type;
  }
  return TOK_IDENTIFIER;
}

static rane_token_type_t identifier_type(rane_lexer_t* lex, size_t start, size_t len) {
  // Simple keyword check
  switch (len) {
    case 2:
      if (memcmp(lex->source + start, "if", 2) == 0) return TOK_KW_IF;
      if (memcmp(lex->source + start, "do", 2) == 0) return TOK_KW_DO;
      if (memcmp(lex->source + start, "or", 2) == 0) return TOK_KW_OR;
      break;
    case 3:
      if (memcmp(lex->source + start, "let", 3) == 0) return TOK_KW_LET;
      if (memcmp(lex->source + start, "and", 3) == 0) return TOK_KW_AND;
      if (memcmp(lex->source + start, "xor", 3) == 0) return TOK_KW_XOR;
      if (memcmp(lex->source + start, "not", 3) == 0) return TOK_KW_NOT;
      if (memcmp(lex->source + start, "shl", 3) == 0) return TOK_KW_SHL;
      if (memcmp(lex->source + start, "shr", 3) == 0) return TOK_KW_SHR;
      if (memcmp(lex->source + start, "sar", 3) == 0) return TOK_KW_SAR;
      break;
    case 4:
      if (memcmp(lex->source + start, "then", 4) == 0) return TOK_KW_THEN;
      if (memcmp(lex->source + start, "else", 4) == 0) return TOK_KW_ELSE;
      if (memcmp(lex->source + start, "jump", 4) == 0) return TOK_KW_JUMP;
      if (memcmp(lex->source + start, "mark", 4) == 0) return TOK_KW_MARK;
      if (memcmp(lex->source + start, "proc", 4) == 0) return TOK_KW_DEF;
      if (memcmp(lex->source + start, "true", 4) == 0) return TOK_BOOL_LITERAL;
      break;
    case 5:
      if (memcmp(lex->source + start, "while", 5) == 0) return TOK_KW_WHILE;
      if (memcmp(lex->source + start, "false", 5) == 0) return TOK_BOOL_LITERAL;
      break;
    case 6:
      if (memcmp(lex->source + start, "return", 6) == 0) return TOK_KW_RETURN;
      break;
    default:
      break;
  }
  return TOK_IDENTIFIER;
}

static rane_token_t identifier(rane_lexer_t* lex) {
  size_t start = lex->pos - 1; // already advanced first char
  while (!is_at_end(lex) && (isalnum(peek(lex)) || peek(lex) == '_')) {
    advance(lex);
  }
  size_t len = lex->pos - start;
  rane_token_type_t type = identifier_type(lex, start, len);
  return make_token(lex, type, lex->source + start, len);
}

static rane_token_t number(rane_lexer_t* lex) {
  size_t start = lex->pos - 1;
  while (!is_at_end(lex) && isdigit(peek(lex))) {
    advance(lex);
  }
  return make_token(lex, TOK_INT_LITERAL, lex->source + start, lex->pos - start);
}

static rane_token_t string_lit(rane_lexer_t* lex) {
  // starting quote already consumed
  size_t start = lex->pos;
  while (!is_at_end(lex) && peek(lex) != '"') {
    // TODO: handle escapes
    advance(lex);
  }
  if (is_at_end(lex)) return error_token(lex, "Unterminated string literal");
  size_t len = lex->pos - start;
  advance(lex); // closing quote
  return make_token(lex, TOK_STRING_LITERAL, lex->source + start, len);
}

rane_token_t rane_lexer_next(rane_lexer_t* lex) {
  skip_whitespace(lex);
  if (is_at_end(lex)) return make_token(lex, TOK_EOF, "", 0);

  char c = advance(lex);
  if (isalpha(c) || c == '_') return identifier(lex);
  if (isdigit(c)) return number(lex);
  if (c == '"') return string_lit(lex);

  switch (c) {
    case '(': return make_token(lex, TOK_KW_LPAREN, lex->source + lex->pos - 1, 1);
    case ')': return make_token(lex, TOK_KW_RPAREN, lex->source + lex->pos - 1, 1);
    case '{': return make_token(lex, TOK_KW_LBRACE, lex->source + lex->pos - 1, 1);
    case '}': return make_token(lex, TOK_KW_RBRACE, lex->source + lex->pos - 1, 1);
    case '[': return make_token(lex, TOK_KW_LBRACKET, lex->source + lex->pos - 1, 1);
    case ']': return make_token(lex, TOK_KW_RBRACKET, lex->source + lex->pos - 1, 1);
    case ';': return make_token(lex, TOK_KW_SEMICOLON, lex->source + lex->pos - 1, 1);
    case ',': return make_token(lex, TOK_KW_COMMA, lex->source + lex->pos - 1, 1);

    case ':':
      if (match(lex, ':')) return make_token(lex, TOK_KW_DOUBLE_COLON, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_COLON, lex->source + lex->pos - 1, 1);

    case '=':
      if (match(lex, '=')) return make_token(lex, TOK_KW_EQ, lex->source + lex->pos - 2, 2);
      if (match(lex, '>')) return make_token(lex, TOK_KW_MAPS_TO, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_ASSIGN, lex->source + lex->pos - 1, 1);

    case '!':
      if (match(lex, '=')) return make_token(lex, TOK_KW_NE, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_EXCLAM, lex->source + lex->pos - 1, 1);

    case '-':
      if (match(lex, '>')) return make_token(lex, TOK_KW_ARROW, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_MINUS, lex->source + lex->pos - 1, 1);

    case '<':
      if (match(lex, '=')) return make_token(lex, TOK_KW_LE, lex->source + lex->pos - 2, 2);
      if (match(lex, '-')) return make_token(lex, TOK_KW_ASSIGN_INTO, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_LT, lex->source + lex->pos - 1, 1);

    case '>':
      if (match(lex, '=')) return make_token(lex, TOK_KW_GE, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_GT, lex->source + lex->pos - 1, 1);

    case '+': return make_token(lex, TOK_KW_PLUS, lex->source + lex->pos - 1, 1);
    case '*': return make_token(lex, TOK_KW_STAR, lex->source + lex->pos - 1, 1);
    case '/': return make_token(lex, TOK_KW_SLASH, lex->source + lex->pos - 1, 1);
    case '%': return make_token(lex, TOK_KW_PERCENT, lex->source + lex->pos - 1, 1);

    case '&': return make_token(lex, TOK_KW_AMP, lex->source + lex->pos - 1, 1);
    case '|': return make_token(lex, TOK_KW_PIPE, lex->source + lex->pos - 1, 1);
    case '^': return make_token(lex, TOK_KW_CARET, lex->source + lex->pos - 1, 1);
    case '~': return make_token(lex, TOK_KW_TILDE, lex->source + lex->pos - 1, 1);
    case '?': return make_token(lex, TOK_KW_QUESTION, lex->source + lex->pos - 1, 1);
    case '.': return make_token(lex, TOK_KW_DOT, lex->source + lex->pos - 1, 1);
    case '@': return make_token(lex, TOK_KW_AT, lex->source + lex->pos - 1, 1);
    case '#': return make_token(lex, TOK_KW_HASH, lex->source + lex->pos - 1, 1);
    case '$': return make_token(lex, TOK_KW_DOLLAR, lex->source + lex->pos - 1, 1);
    case '\\': return make_token(lex, TOK_KW_BACKSLASH, lex->source + lex->pos - 1, 1);
    case '_': return make_token(lex, TOK_KW_UNDERSCORE, lex->source + lex->pos - 1, 1);
  }

  return error_token(lex, "Unexpected character");
}

const char* rane_token_type_str(rane_token_type_t type) {
  switch (type) {
    case TOK_EOF: return "EOF";
    case TOK_IDENTIFIER: return "IDENTIFIER";
    case TOK_INT_LITERAL: return "INT_LITERAL";
    case TOK_KW_LET: return "LET";
    case TOK_KW_IF: return "IF";
    case TOK_KW_THEN: return "THEN";
    case TOK_KW_ELSE: return "ELSE";
    case TOK_KW_JUMP: return "JUMP";
    case TOK_KW_MARK: return "MARK";
    case TOK_KW_WHILE: return "WHILE";
    case TOK_KW_RETURN: return "RETURN";
    // Add more
    default: return "UNKNOWN";
  }
}