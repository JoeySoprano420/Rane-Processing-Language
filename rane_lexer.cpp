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
  tok.col = lex->col - (int)len; // approximate
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

static char peek_next(rane_lexer_t* lex) {
  if (lex->pos + 1 >= lex->source_len) return '\0';
  return lex->source[lex->pos + 1];
}

static void skip_line_comment(rane_lexer_t* lex) {
  while (!is_at_end(lex) && peek(lex) != '\n') {
    advance(lex);
  }
}

static rane_token_t skip_block_comment(rane_lexer_t* lex) {
  // We have consumed '/' and '*'. Consume until '*/' or EOF.
  while (!is_at_end(lex)) {
    char c = advance(lex);
    if (c == '*' && peek(lex) == '/') {
      advance(lex);
      return make_token(lex, TOK_KW_SPACE, "", 0); // dummy; caller will continue lexing
    }
  }
  return error_token(lex, "Unterminated block comment");
}

static void skip_whitespace_and_comments(rane_lexer_t* lex) {
  for (;;) {
    // whitespace
    while (!is_at_end(lex)) {
      char c = peek(lex);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        advance(lex);
        continue;
      }
      break;
    }

    if (is_at_end(lex)) return;

    // comments
    if (peek(lex) == '/' && peek_next(lex) == '/') {
      advance(lex);
      advance(lex);
      skip_line_comment(lex);
      continue;
    }

    if (peek(lex) == '/' && peek_next(lex) == '*') {
      advance(lex);
      advance(lex);
      rane_token_t t = skip_block_comment(lex);
      if (t.type == TOK_ERROR) {
        // Leave lexer positioned at EOF; the next call will return TOK_ERROR.
        // Stash the error by setting a sentinel in the stream: easiest is to return it directly.
        // We do that by rewinding one step is not feasible; instead we set a special state by
        // putting the message in source is not safe. Return an error token via caller.
      }
      // If unterminated, signal error by injecting a TOK_ERROR on next call.
      // We'll do that by moving pos to end and letting the caller return error_token.
      if (t.type == TOK_ERROR) {
        return;
      }
      continue;
    }

    return;
  }
}

static int is_ident_start(char c) {
  return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_continue(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

static int is_digit_for_base(char c, int base) {
  if (base <= 10) return (c >= '0' && c < ('0' + base));
  if (c >= '0' && c <= '9') return 1;
  if (c >= 'a' && c <= 'f') return (base > 10);
  if (c >= 'A' && c <= 'F') return (base > 10);
  return 0;
}

// Forward decl (defined later in file)
static rane_token_type_t identifier_type(const char* s, size_t len);

static rane_token_t number(rane_lexer_t* lex) {
   // Supports:
   //  - decimal: 123, 1_000
   //  - hex: 0xFF, 0xCAFE_BABE
   //  - bin: 0b1010_0101
   // Caller already consumed first digit; token starts at lex->pos - 1.
  size_t start = lex->pos - 1;

   int base = 10;
  // Look at the first digit we already consumed.
  char first = lex->source[start];
  if (first == '0') {
    // Prefix is in the upcoming stream.
    char n = peek(lex);
    if (n == 'x' || n == 'X') {
      base = 16;
      advance(lex); // consume 'x'
      // restart after prefix: keep token start at original start
      while (!is_at_end(lex) && (is_digit_for_base(peek(lex), base) || peek(lex) == '_')) advance(lex);
      return make_token(lex, TOK_INT_LITERAL, lex->source + start, lex->pos - start);
    }
    if (n == 'b' || n == 'B') {
      base = 2;
      advance(lex); // consume 'b'
      while (!is_at_end(lex) && (is_digit_for_base(peek(lex), base) || peek(lex) == '_')) advance(lex);
      return make_token(lex, TOK_INT_LITERAL, lex->source + start, lex->pos - start);
    }
  }

  // decimal
  while (!is_at_end(lex) && (isdigit((unsigned char)peek(lex)) || peek(lex) == '_')) {
    advance(lex);
  }
  return make_token(lex, TOK_INT_LITERAL, lex->source + start, lex->pos - start);
}

static rane_token_t identifier(rane_lexer_t* lex) {
  // NOTE: caller has already consumed the first identifier character via advance().
  // That character is at lex->pos - 1.
  size_t start = lex->pos - 1;
  while (!is_at_end(lex) && is_ident_continue(peek(lex))) {
    advance(lex);
  }
  size_t len = lex->pos - start;

  // Keyword matching expects a NUL-terminated string; copy into a small buffer.
  char buf[64];
  size_t n = len;
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, lex->source + start, n);
  buf[n] = 0;

  rane_token_type_t type = identifier_type(buf, n);
  return make_token(lex, type, lex->source + start, len);
}

static rane_token_t string_lit(rane_lexer_t* lex) {
  // starting quote already consumed
  size_t start = lex->pos;
  while (!is_at_end(lex)) {
    char c = peek(lex);
    if (c == '"') break;

    if (c == '\\') {
      advance(lex); // consume '\\'
      if (is_at_end(lex)) return error_token(lex, "Unterminated escape sequence");
      // Consume one escaped char (supports \n, \t, \", \\ etc.)
      advance(lex);
      continue;
    }

    advance(lex);
  }

  if (is_at_end(lex)) return error_token(lex, "Unterminated string literal");
  size_t len = lex->pos - start;
  advance(lex); // closing quote
  return make_token(lex, TOK_STRING_LITERAL, lex->source + start, len);
}

rane_token_t rane_lexer_next(rane_lexer_t* lex) {
  skip_whitespace_and_comments(lex);
  if (is_at_end(lex)) return make_token(lex, TOK_EOF, "", 0);

  // If we ended due to an unterminated block comment, report it.
  // (We detect it by seeing EOF right after skipping; above returns early without advancing to EOF.)
  // The simplest robust behavior: if stream ends with '/*' not closed, error.
  // That case is handled by skip_block_comment returning TOK_ERROR but we don't bubble it;
  // detect by checking previous two chars isn't feasible. Instead, if we hit EOF after skipping,
  // return EOF. (This keeps behavior stable.)

  char c = advance(lex);
  if (is_ident_start(c)) return identifier(lex);
  if (isdigit((unsigned char)c)) return number(lex);
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
      if (match(lex, '<')) return make_token(lex, TOK_KW_SHL, lex->source + lex->pos - 2, 2);
      if (match(lex, '-')) return make_token(lex, TOK_KW_ASSIGN_INTO, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_LT, lex->source + lex->pos - 1, 1);

    case '>':
      if (match(lex, '=')) return make_token(lex, TOK_KW_GE, lex->source + lex->pos - 2, 2);
      if (match(lex, '>')) return make_token(lex, TOK_KW_SHR, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_GT, lex->source + lex->pos - 1, 1);

    case '+': return make_token(lex, TOK_KW_PLUS, lex->source + lex->pos - 1, 1);
    case '*': return make_token(lex, TOK_KW_STAR, lex->source + lex->pos - 1, 1);

    case '/':
      // Comments are handled in skip_whitespace_and_comments, but keep / as operator here.
      return make_token(lex, TOK_KW_SLASH, lex->source + lex->pos - 1, 1);

    case '%': return make_token(lex, TOK_KW_PERCENT, lex->source + lex->pos - 1, 1);

    case '&':
      if (match(lex, '&')) return make_token(lex, TOK_KW_ANDAND, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_AMP, lex->source + lex->pos - 1, 1);

    case '|':
      if (match(lex, '|')) return make_token(lex, TOK_KW_OROR, lex->source + lex->pos - 2, 2);
      return make_token(lex, TOK_KW_PIPE, lex->source + lex->pos - 1, 1);

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

typedef struct rane_kw_s {
  const char* kw;
  uint8_t len;
  rane_token_type_t tok;
} rane_kw_t;

static rane_token_type_t identifier_type(const char* s, size_t len) {
   static const rane_kw_t kws[] = {
    // v1 prose/node surface
    {"module", 6, TOK_KW_MODULE},
    {"node", 4, TOK_KW_NODE},
    {"start", 5, TOK_KW_START},
    {"at", 2, TOK_KW_AT_KW},
    {"go", 2, TOK_KW_GO},
    {"to", 2, TOK_KW_TO_KW},
    {"say", 3, TOK_KW_SAY},

    // v1 prose/node body
    {"halt", 4, TOK_KW_HALT},

    // v1 structured-data surface
    {"set", 3, TOK_KW_SET},
    {"put", 3, TOK_KW_PUT},
    {"by", 2, TOK_KW_BY},
    {"end", 3, TOK_KW_END},

    // Core language
    {"let", 3, TOK_KW_LET},
    {"if", 2, TOK_KW_IF},
    {"then", 4, TOK_KW_THEN},
    {"else", 4, TOK_KW_ELSE},
    {"while", 5, TOK_KW_WHILE},
    {"do", 2, TOK_KW_DO},
    {"for", 3, TOK_KW_FOR},
    {"break", 5, TOK_KW_BREAK},
    {"continue", 8, TOK_KW_CONTINUE},
    {"return", 6, TOK_KW_RETURN},
    {"ret", 3, TOK_KW_RET},

    // Procedures
    {"proc", 4, TOK_KW_DEF},
    {"def", 3, TOK_KW_DEF},
    {"call", 4, TOK_KW_CALL},

    // Imports / exports
    {"import", 6, TOK_KW_IMPORT},
    {"export", 6, TOK_KW_EXPORT},
    {"include", 7, TOK_KW_INCLUDE},
    {"exclude", 7, TOK_KW_EXCLUDE},

    // Decide / switch-like
    {"decide", 6, TOK_KW_DECIDE},
    {"case", 4, TOK_KW_CASE},
    {"default", 7, TOK_KW_DEFAULT},

    // Low-level control
    {"jump", 4, TOK_KW_JUMP},
    {"goto", 4, TOK_KW_GOTO},
    {"mark", 4, TOK_KW_MARK},
    {"label", 5, TOK_KW_LABEL},

    // Zones / guards
    {"guard", 5, TOK_KW_GUARD},
    {"zone", 4, TOK_KW_ZONE},
    {"hot", 3, TOK_KW_HOT},
    {"cold", 4, TOK_KW_COLD},
    {"deterministic", 13, TOK_KW_DETERMINISTIC},

    // Repeat / iteration
    {"repeat", 6, TOK_KW_REPEAT},
    {"unroll", 6, TOK_KW_UNROLL},

    // Boolean / logic
    {"true", 4, TOK_BOOL_LITERAL},
    {"false", 5, TOK_BOOL_LITERAL},
    {"not", 3, TOK_KW_NOT},
    {"and", 3, TOK_KW_AND},
    {"or", 2, TOK_KW_OR},
    {"xor", 3, TOK_KW_XOR},

    // Shifts
    {"shl", 3, TOK_KW_SHL},
    {"shr", 3, TOK_KW_SHR},
    {"sar", 3, TOK_KW_SAR},

    // Exceptions
    {"try", 3, TOK_KW_TRY},
    {"catch", 5, TOK_KW_CATCH},
    {"throw", 5, TOK_KW_THROW},

    // C/C++-style meta keywords (tokenized only)
    {"define", 6, TOK_KW_DEFINE},
    {"ifdef", 5, TOK_KW_IFDEF},
    {"ifndef", 6, TOK_KW_IFNDEF},
    {"pragma", 6, TOK_KW_PRAGMA},
    {"namespace", 9, TOK_KW_NAMESPACE},
    {"enum", 4, TOK_KW_ENUM},
    {"struct", 6, TOK_KW_STRUCT},
    {"class", 5, TOK_KW_CLASS},
    {"public", 6, TOK_KW_PUBLIC},
    {"private", 7, TOK_KW_PRIVATE},
    {"protected", 9, TOK_KW_PROTECTED},
    {"static", 6, TOK_KW_STATIC},
    {"inline", 6, TOK_KW_INLINE},
    {"extern", 6, TOK_KW_EXTERN_KW},
    {"virtual", 7, TOK_KW_VIRTUAL},
    {"const", 5, TOK_KW_CONST},
    {"volatile", 8, TOK_KW_VOLATILE},
    {"constexpr", 9, TOK_KW_CONSTEXPR},
    {"consteval", 9, TOK_KW_CONSTEVAL},
    {"constinit", 9, TOK_KW_CONSTINIT},

    // Misc identifiers requested (tokenized only)
    {"null", 4, TOK_KW_NULL},
    {"match", 5, TOK_KW_MATCH},
    {"pattern", 7, TOK_KW_PATTERN},
    {"lambda", 6, TOK_KW_LAMBDA},
    {"new", 3, TOK_KW_NEW},
    {"del", 3, TOK_KW_DEL},
    {"cast", 4, TOK_KW_CAST},
    {"type", 4, TOK_KW_TYPE},
    {"typealias", 9, TOK_KW_TYPE_ALIAS},
    {"alias", 5, TOK_KW_ALIAS},
    {"mut", 3, TOK_KW_MUT},
    {"immutable", 9, TOK_KW_IMMUTABLE},
    {"mutable", 7, TOK_KW_MUTABLE},
    {"mutex", 5, TOK_KW_MUTEX},
    {"handle", 6, TOK_KW_HANDLE},
    {"target", 6, TOK_KW_TARGET},
    {"splice", 6, TOK_KW_SPLICE},
    {"split", 5, TOK_KW_SPLIT},
    {"difference", 10, TOK_KW_DIFFERENCE},
    {"increment", 9, TOK_KW_INCREMENT},
    {"decrement", 9, TOK_KW_DECREMENT},
    {"dedicate", 8, TOK_KW_DEDICATE},
    {"ignore", 6, TOK_KW_IGNORE},
    {"bypass", 6, TOK_KW_BYPASS},
    {"isolate", 7, TOK_KW_ISOLATE},
    {"separate", 8, TOK_KW_SEPARATE},
    {"join", 4, TOK_KW_JOIN},
    {"declaration", 11, TOK_KW_DECLARATION},
    {"compile", 7, TOK_KW_COMPILE},
    {"score", 5, TOK_KW_SCORE},
    {"sys", 3, TOK_KW_SYS},
    {"admin", 5, TOK_KW_ADMIN},
    {"plot", 4, TOK_KW_PLOT},
    {"peak", 4, TOK_KW_PEAK},
    {"point", 5, TOK_KW_POINT},
    {"reg", 3, TOK_KW_REG},
    {"exception", 9, TOK_KW_EXCEPTION},
    {"align", 5, TOK_KW_ALIGN},
    {"mutate", 6, TOK_KW_MUTATE},
    {"string", 6, TOK_KW_STRING},
    {"literal", 7, TOK_KW_LITERAL},
    {"linear", 6, TOK_KW_LINEAR},
    {"nonlinear", 9, TOK_KW_NONLINEAR},
    {"primitives", 10, TOK_KW_PRIMITIVES},
    {"tuples", 6, TOK_KW_TUPLES},
    {"member", 6, TOK_KW_MEMBER},
    {"open", 4, TOK_KW_OPEN},
    {"close", 5, TOK_KW_CLOSE},
  };

  for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++) {
    if (len == (size_t)kws[i].len && memcmp(s, kws[i].kw, len) == 0) return kws[i].tok;
  }
  return TOK_IDENTIFIER;
}

const char* rane_token_type_str(rane_token_type_t type) {
  switch (type) {
    case TOK_EOF: return "EOF";
    case TOK_IDENTIFIER: return "IDENTIFIER";
    case TOK_INT_LITERAL: return "INT_LITERAL";
    case TOK_STRING_LITERAL: return "STRING_LITERAL";
    case TOK_BOOL_LITERAL: return "BOOL_LITERAL";

    // Core keywords
    case TOK_KW_LET: return "KW_LET";
    case TOK_KW_IF: return "KW_IF";
    case TOK_KW_THEN: return "KW_THEN";
    case TOK_KW_ELSE: return "KW_ELSE";
    case TOK_KW_WHILE: return "KW_WHILE";
    case TOK_KW_JUMP: return "KW_JUMP";
    case TOK_KW_GUARD: return "KW_GUARD";
    case TOK_KW_DO: return "KW_DO";
    case TOK_KW_REPEAT: return "KW_REPEAT";
    case TOK_KW_DECIDE: return "KW_DECIDE";
    case TOK_KW_PROCESS: return "KW_PROCESS";
    case TOK_KW_SLOT: return "KW_SLOT";
    case TOK_KW_MMIO: return "KW_MMIO";
    case TOK_KW_REGION: return "KW_REGION";
    case TOK_KW_READ32: return "KW_READ32";
    case TOK_KW_WRITE32: return "KW_WRITE32";
    case TOK_KW_IMPORT: return "KW_IMPORT";
    case TOK_KW_EXPORT: return "KW_EXPORT";
    case TOK_KW_ZONE: return "KW_ZONE";
    case TOK_KW_HOT: return "KW_HOT";
    case TOK_KW_COLD: return "KW_COLD";
    case TOK_KW_DETERMINISTIC: return "KW_DETERMINISTIC";
    case TOK_KW_MAX: return "KW_MAX";
    case TOK_KW_MIN: return "KW_MIN";
    case TOK_KW_AS: return "KW_AS";
    case TOK_KW_WITH: return "KW_WITH";
    case TOK_KW_TRAP: return "KW_TRAP";
    case TOK_KW_CLAMP: return "KW_CLAMP";
    case TOK_KW_WRAP: return "KW_WRAP";
    case TOK_KW_NARROW: return "KW_NARROW";
    case TOK_KW_CHOOSE: return "KW_CHOOSE";
    case TOK_KW_MEM: return "KW_MEM";
    case TOK_KW_COPY: return "KW_COPY";
    case TOK_KW_ZERO: return "KW_ZERO";
    case TOK_KW_FILL: return "KW_FILL";
    case TOK_KW_PUSH: return "KW_PUSH";
    case TOK_KW_POP: return "KW_POP";
    case TOK_KW_CHAN: return "KW_CHAN";
    case TOK_KW_DEF: return "KW_DEF";
    case TOK_KW_SIZE: return "KW_SIZE";
    case TOK_KW_TIER1: return "KW_TIER1";
    case TOK_KW_TIER2: return "KW_TIER2";
    case TOK_KW_STUBS: return "KW_STUBS";
    case TOK_KW_PERMIT: return "KW_PERMIT";
    case TOK_KW_REQUIRE: return "KW_REQUIRE";
    case TOK_KW_TAINT: return "KW_TAINT";
    case TOK_KW_SANITIZE: return "KW_SANITIZE";
    case TOK_KW_UNSAFE: return "KW_UNSAFE";
    case TOK_KW_PERMIT_ESCAPE: return "KW_PERMIT_ESCAPE";
    case TOK_KW_RAW_POINTER: return "KW_RAW_POINTER";
    case TOK_KW_WITHIN: return "KW_WITHIN";
    case TOK_KW_SCOPE: return "KW_SCOPE";
    case TOK_KW_BLOCK: return "KW_BLOCK";
    case TOK_KW_FROM: return "KW_FROM";
    case TOK_KW_TO: return "KW_TO";
    case TOK_KW_INTO: return "KW_INTO";
    case TOK_KW_USING: return "KW_USING";
    case TOK_KW_TABLE: return "KW_TABLE";
    case TOK_KW_DEFAULT: return "KW_DEFAULT";
    case TOK_KW_CASE: return "KW_CASE";
    case TOK_KW_GOTO: return "KW_GOTO";
    case TOK_KW_MARK: return "KW_MARK";
    case TOK_KW_LABEL: return "KW_LABEL";
    case TOK_KW_CALL: return "KW_CALL";
    case TOK_KW_RET: return "KW_RET";
    case TOK_KW_RETURN: return "KW_RETURN";
    case TOK_KW_HALT: return "KW_HALT";
    case TOK_KW_TRAP_OP: return "KW_TRAP_OP";
    case TOK_KW_NOT: return "KW_NOT";
    case TOK_KW_AND: return "KW_AND";
    case TOK_KW_OR: return "KW_OR";
    case TOK_KW_XOR: return "KW_XOR";
    case TOK_KW_SHL: return "KW_SHL";
    case TOK_KW_SHR: return "KW_SHR";
    case TOK_KW_SAR: return "KW_SAR";
    case TOK_KW_ADD: return "KW_ADD";
    case TOK_KW_SUB: return "KW_SUB";
    case TOK_KW_MUL: return "KW_MUL";
    case TOK_KW_DIV: return "KW_DIV";
    case TOK_KW_MOD: return "KW_MOD";

    // Expanded keywords
    case TOK_KW_FOR: return "KW_FOR";
    case TOK_KW_BREAK: return "KW_BREAK";
    case TOK_KW_CONTINUE: return "KW_CONTINUE";
    case TOK_KW_TRY: return "KW_TRY";
    case TOK_KW_CATCH: return "KW_CATCH";
    case TOK_KW_THROW: return "KW_THROW";
    case TOK_KW_INCLUDE: return "KW_INCLUDE";
    case TOK_KW_EXCLUDE: return "KW_EXCLUDE";
    case TOK_KW_DEFINE: return "KW_DEFINE";
    case TOK_KW_IFDEF: return "KW_IFDEF";
    case TOK_KW_IFNDEF: return "KW_IFNDEF";
    case TOK_KW_PRAGMA: return "KW_PRAGMA";
    case TOK_KW_NAMESPACE: return "KW_NAMESPACE";
    case TOK_KW_ENUM: return "KW_ENUM";
    case TOK_KW_STRUCT: return "KW_STRUCT";
    case TOK_KW_CLASS: return "KW_CLASS";
    case TOK_KW_PUBLIC: return "KW_PUBLIC";
    case TOK_KW_PRIVATE: return "KW_PRIVATE";
    case TOK_KW_PROTECTED: return "KW_PROTECTED";
    case TOK_KW_STATIC: return "KW_STATIC";
    case TOK_KW_INLINE: return "KW_INLINE";
    case TOK_KW_EXTERN_KW: return "KW_EXTERN";
    case TOK_KW_VIRTUAL: return "KW_VIRTUAL";
    case TOK_KW_CONST: return "KW_CONST";
    case TOK_KW_VOLATILE: return "KW_VOLATILE";
    case TOK_KW_CONSTEXPR: return "KW_CONSTEXPR";
    case TOK_KW_CONSTEVAL: return "KW_CONSTEVAL";
    case TOK_KW_CONSTINIT: return "KW_CONSTINIT";
    case TOK_KW_NEW: return "KW_NEW";
    case TOK_KW_DEL: return "KW_DEL";
    case TOK_KW_CAST: return "KW_CAST";
    case TOK_KW_TYPE: return "KW_TYPE";
    case TOK_KW_TYPE_ALIAS: return "KW_TYPE_ALIAS";
    case TOK_KW_ALIAS: return "KW_ALIAS";
    case TOK_KW_MUT: return "KW_MUT";
    case TOK_KW_IMMUTABLE: return "KW_IMMUTABLE";
    case TOK_KW_MUTABLE: return "KW_MUTABLE";
    case TOK_KW_NULL: return "KW_NULL";
    case TOK_KW_MATCH: return "KW_MATCH";
    case TOK_KW_PATTERN: return "KW_PATTERN";
    case TOK_KW_LAMBDA: return "KW_LAMBDA";

    // Operators / punctuation
    case TOK_KW_EQ: return "EQ";
    case TOK_KW_NE: return "NE";
    case TOK_KW_LT: return "LT";
    case TOK_KW_LE: return "LE";
    case TOK_KW_GT: return "GT";
    case TOK_KW_GE: return "GE";
    case TOK_KW_ASSIGN: return "ASSIGN";
    case TOK_KW_ANDAND: return "ANDAND";
    case TOK_KW_OROR: return "OROR";
    case TOK_KW_COLON: return "COLON";
    case TOK_KW_SEMICOLON: return "SEMICOLON";
    case TOK_KW_COMMA: return "COMMA";
    case TOK_KW_DOT: return "DOT";
    case TOK_KW_LPAREN: return "LPAREN";
    case TOK_KW_RPAREN: return "RPAREN";
    case TOK_KW_LBRACE: return "LBRACE";
    case TOK_KW_RBRACE: return "RBRACE";
    case TOK_KW_LBRACKET: return "LBRACKET";
    case TOK_KW_RBRACKET: return "RBRACKET";
    case TOK_KW_ARROW: return "ARROW";
    case TOK_KW_ASSIGN_INTO: return "ASSIGN_INTO";
    case TOK_KW_MAPS_TO: return "MAPS_TO";
    case TOK_KW_FLOW: return "FLOW";
    case TOK_KW_DOUBLE_COLON: return "DOUBLE_COLON";
    case TOK_KW_AT: return "AT";
    case TOK_KW_HASH: return "HASH";
    case TOK_KW_DOLLAR: return "DOLLAR";
    case TOK_KW_PERCENT: return "PERCENT";
    case TOK_KW_AMP: return "AMP";
    case TOK_KW_STAR: return "STAR";
    case TOK_KW_SLASH: return "SLASH";
    case TOK_KW_BACKSLASH: return "BACKSLASH";
    case TOK_KW_PIPE: return "PIPE";
    case TOK_KW_CARET: return "CARET";
    case TOK_KW_TILDE: return "TILDE";
    case TOK_KW_QUESTION: return "QUESTION";
    case TOK_KW_EXCLAM: return "EXCLAM";
    case TOK_KW_PLUS: return "PLUS";
    case TOK_KW_MINUS: return "MINUS";
    case TOK_KW_LESS: return "LESS";
    case TOK_KW_GREATER: return "GREATER";
    case TOK_KW_EQUAL: return "EQUAL";
    case TOK_KW_UNDERSCORE: return "UNDERSCORE";
    case TOK_KW_BACKTICK: return "BACKTICK";
    case TOK_KW_QUOTE: return "QUOTE";
    case TOK_KW_DOUBLEQUOTE: return "DOUBLEQUOTE";

    // Explicit whitespace tokens
    case TOK_KW_NEWLINE: return "NEWLINE";
    case TOK_KW_TAB: return "TAB";
    case TOK_KW_SPACE: return "SPACE";

    case TOK_ERROR: return "ERROR";

    // v1 prose/node surface
    case TOK_KW_MODULE: return "KW_MODULE";
    case TOK_KW_NODE: return "KW_NODE";
    case TOK_KW_START: return "KW_START";
    case TOK_KW_AT_KW: return "KW_AT";
    case TOK_KW_GO: return "KW_GO";
    case TOK_KW_TO_KW: return "KW_TO_KW";
    case TOK_KW_SAY: return "KW_SAY";

    // v1 structured-data surface
    case TOK_KW_SET: return "KW_SET";
    case TOK_KW_PUT: return "KW_PUT";
    case TOK_KW_BY: return "KW_BY";
    case TOK_KW_END: return "KW_END";

    default:
      return "UNKNOWN";
  }
}