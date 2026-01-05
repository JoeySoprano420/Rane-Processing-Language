#include "rane_parser.h"
#include "rane_lexer.h"
#include "rane_ast.h"
#include "rane_diag.h"
#include <stdlib.h>
#include <string.h>

static uint64_t parse_u64_literal(const rane_token_t& t) {
  // Supports: decimal, hex (0x), binary (0b), and underscores.
  char buf[128];
  size_t n = t.length;
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  size_t w = 0;
  for (size_t i = 0; i < n && w + 1 < sizeof(buf); i++) {
    char c = t.start[i];
    if (c == '_') continue;
    buf[w++] = c;
  }
  buf[w] = 0;

  int base = 10;
  if (w >= 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) base = 16;
  else if (w >= 2 && buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B')) return (uint64_t)_strtoui64(buf + 2, NULL, 2);
  return (uint64_t)_strtoui64(buf, NULL, base);
}

// Parser state
typedef struct rane_parser_s {
  rane_lexer_t lexer;
  rane_token_t current;
  rane_token_t previous;
  rane_diag_t* diag;
  int had_error;
} rane_parser_t;

static void set_diag(rane_parser_t* p, rane_diag_code_t code, rane_token_t at, const char* msg) {
  if (!p || !p->diag) return;
  p->diag->code = code;
  p->diag->span = rane_span_from_token((uint32_t)at.line, (uint32_t)at.col, (uint32_t)at.length);
  strncpy_s(p->diag->message, sizeof(p->diag->message), msg ? msg : "", _TRUNCATE);
}

static rane_span_t span_from_tok(const rane_token_t* t) {
  if (!t) return rane_span_from_token(0, 0, 0);
  return rane_span_from_token((uint32_t)t->line, (uint32_t)t->col, (uint32_t)t->length);
}

static void parser_init(rane_parser_t* p, const char* source, size_t len, rane_diag_t* diag) {
  rane_lexer_init(&p->lexer, source, len);
  p->current = rane_lexer_next(&p->lexer);
  p->previous = rane_token_t{TOK_EOF, NULL, 0, 0, 0};
  p->diag = diag;
  p->had_error = 0;
  if (p->diag) {
    p->diag->code = RANE_DIAG_OK;
    p->diag->span = rane_span_from_token(0, 0, 0);
    p->diag->message[0] = 0;
  }
}

static void advance(rane_parser_t* p) {
  p->previous = p->current;
  p->current = rane_lexer_next(&p-> lexer);
  if (p->current.type == TOK_ERROR) {
    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Lex error");
  }
}

static int check(rane_parser_t* p, rane_token_type_t type) {
  return p->current.type == type;
}

static int match(rane_parser_t* p, rane_token_type_t type) {
  if (check(p, type)) {
    advance(p);
    return 1;
  }
  return 0;
}

static rane_token_t consume(rane_parser_t* p, rane_token_type_t type, const char* msg) {
  if (check(p, type)) {
    advance(p);
    return p->previous;
  }
  p->had_error = 1;
  set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, msg);
  return rane_token_t{TOK_ERROR, msg, strlen(msg), p->current.line, p->current.col};
}

static void consume_optional_semicolon(rane_parser_t* p) {
  if (!p) return;
  if (check(p, TOK_KW_SEMICOLON)) {
    advance(p);
  }
}

// Forward declarations
static rane_expr_t* parse_expr(rane_parser_t* p);
static rane_expr_t* parse_unary_expr(rane_parser_t* p);
static rane_expr_t* parse_primary_expr(rane_parser_t* p);
static rane_expr_t* parse_postfix_expr(rane_parser_t* p);

static rane_stmt_t* parse_stmt(rane_parser_t* p);
static rane_stmt_t* parse_program(rane_parser_t* p);

// v1 helpers (declared early; defined later)
static void tok_to_ident(char out[64], const rane_token_t& t);
static void parse_type_name_after_colon(rane_parser_t* p, rane_type_name_t* out_ty);
static rane_expr_t* parse_v1_struct_literal_after_type_name(rane_parser_t* p, const rane_token_t& type_tok);

static rane_stmt_t* parse_program(rane_parser_t* p) {
  if (!p) return NULL;

  rane_stmt_t* b = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  if (!b) return NULL;
  memset(b, 0, sizeof(*b));
  b->span = rane_span_from_token(1, 1, 0);
  b->kind = STMT_BLOCK;
  b->block.stmts = (rane_stmt_t**)malloc(sizeof(rane_stmt_t*) * 256);
  b->block.stmt_count = 0;

  while (!check(p, TOK_EOF) && !p->had_error) {
    rane_stmt_t* s = parse_stmt(p);
    if (!s) break;
    b->block.stmts[b->block.stmt_count++] = s;
    consume_optional_semicolon(p);

    if (b->block.stmt_count >= 256) {
      p->had_error = 1;
      set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Program too large (too many top-level statements)");
      break;
    }
  }

  return b;
}

static rane_stmt_t* parse_block_stmt(rane_parser_t* p, rane_token_t lbrace_tok) {
  rane_stmt_t* b = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(b, 0, sizeof(*b));
  b->span = span_from_tok(&lbrace_tok);
  b->kind = STMT_BLOCK;
  b->block.stmts = (rane_stmt_t**)malloc(sizeof(rane_stmt_t*) * 64);
  b->block.stmt_count = 0;

  while (!check(p, TOK_EOF) && !check(p, TOK_KW_RBRACE)) {
    rane_stmt_t* s = parse_stmt(p);
    if (!s) break;
    b->block.stmts[b->block.stmt_count++] = s;
    consume_optional_semicolon(p);
  }

  consume(p, TOK_KW_RBRACE, "Expect } to close block");
  return b;
}

static int token_precedence(rane_token_type_t t) {
  switch (t) {
    case TOK_KW_STAR:
    case TOK_KW_SLASH:
    case TOK_KW_PERCENT:
      return 70;
    case TOK_KW_PLUS:
    case TOK_KW_MINUS:
      return 60;

    // Bitwise
    case TOK_KW_CARET:
      return 35;
    case TOK_KW_AMP:
      return 30;
    case TOK_KW_PIPE:
      return 25;

    case TOK_KW_LT:
    case TOK_KW_LE:
    case TOK_KW_GT:
    case TOK_KW_GE:
      return 50;
    case TOK_KW_EQ:
    case TOK_KW_NE:
      return 40;

    // v1: allow '=' in expressions as equality (same as '==')
    case TOK_KW_ASSIGN:
      return 40;

    // Logical (word keywords)
    case TOK_KW_AND:
    case TOK_KW_ANDAND:
      return 20;
    case TOK_KW_OR:
    case TOK_KW_OROR:
      return 10;

    // Shifts
    case TOK_KW_SHL:
    case TOK_KW_SHR:
    case TOK_KW_SAR:
      return 55;

    // Bitwise (word-form)
    case TOK_KW_XOR:
      return 35;

    default:
      return -1;
  }
}

static rane_bin_op_e token_to_binop(rane_token_type_t t) {
  switch (t) {
    case TOK_KW_PLUS: return BIN_ADD;
    case TOK_KW_MINUS: return BIN_SUB;
    case TOK_KW_STAR: return BIN_MUL;
    case TOK_KW_SLASH: return BIN_DIV;
    case TOK_KW_PERCENT: return BIN_MOD;

    // Bitwise
    case TOK_KW_AMP: return BIN_AND;
    case TOK_KW_PIPE: return BIN_OR;
    case TOK_KW_CARET: return BIN_XOR;

    case TOK_KW_LT: return BIN_LT;
    case TOK_KW_LE: return BIN_LE;
    case TOK_KW_GT: return BIN_GT;
    case TOK_KW_GE: return BIN_GE;
    case TOK_KW_EQ: return BIN_EQ;
    case TOK_KW_NE: return BIN_NE;

    // v1: single '=' is equality in expressions
    case TOK_KW_ASSIGN: return BIN_EQ;

    // Logical
    case TOK_KW_AND: return BIN_LOGAND;
    case TOK_KW_OR: return BIN_LOGOR;
    case TOK_KW_ANDAND: return BIN_LOGAND;
    case TOK_KW_OROR: return BIN_LOGOR;

    // Shifts
    case TOK_KW_SHL: return BIN_SHL;
    case TOK_KW_SHR: return BIN_SHR;
    case TOK_KW_SAR: return BIN_SAR;

    // Bitwise (word-form)
    case TOK_KW_XOR: return BIN_XOR;

    default: return BIN_ADD;
  }
}

static rane_expr_t* parse_binary_rhs(rane_parser_t* p, int min_prec, rane_expr_t* lhs) {
  while (1) {
    int prec = token_precedence(p->current.type);
    if (prec < min_prec) break;

    rane_token_type_t op_tok = p->current.type;
    rane_token_t op_token = p->current;
    advance(p);

    rane_expr_t* rhs = parse_unary_expr(p);
    if (!rhs) return lhs;

    int next_prec = token_precedence(p->current.type);
    if (next_prec > prec) {
      rhs = parse_binary_rhs(p, prec + 1, rhs);
    }

    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&op_token);
    e->kind = EXPR_BINARY;
    e->binary.op = token_to_binop(op_tok);
    e->binary.left = lhs;
    e->binary.right = rhs;
    lhs = e;
  }
  return lhs;
}

// Parse expression (simple: int literal or identifier)
static rane_expr_t* parse_expr(rane_parser_t* p) {
  rane_expr_t* lhs = parse_unary_expr(p);
  if (!lhs) return NULL;
  lhs = parse_binary_rhs(p, 0, lhs);

  // Ternary (C-like): cond ? then_expr : else_expr
  if (match(p, TOK_KW_QUESTION)) {
    rane_token_t q = p->previous;
    rane_expr_t* then_e = parse_expr(p);
    consume(p, TOK_KW_COLON, "Expect : in ternary expression");
    rane_expr_t* else_e = parse_expr(p);

    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&q);
    e->kind = EXPR_TERNARY;
    e->ternary.cond = lhs;
    e->ternary.then_expr = then_e;
    e->ternary.else_expr = else_e;
    return e;
  }

  return lhs;
}

static rane_expr_t* parse_unary_expr(rane_parser_t* p) {
  if (match(p, TOK_KW_MINUS)) {
    rane_token_t op = p->previous;
    rane_expr_t* expr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(expr, 0, sizeof(*expr));
    expr->span = span_from_tok(&op);
    expr->kind = EXPR_UNARY;
    expr->unary.op = UN_NEG;
    expr->unary.expr = parse_unary_expr(p);
    return expr;
  }
  if (match(p, TOK_KW_NOT) || match(p, TOK_KW_EXCLAM)) {
    rane_token_t op = p->previous;
    rane_expr_t* expr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(expr, 0, sizeof(*expr));
    expr->span = span_from_tok(&op);
    expr->kind = EXPR_UNARY;
    expr->unary.op = UN_NOT;
    expr->unary.expr = parse_unary_expr(p);
    return expr;
  }
  if (match(p, TOK_KW_TILDE)) {
    rane_token_t op = p->previous;
    rane_expr_t* expr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(expr, 0, sizeof(*expr));
    expr->span = span_from_tok(&op);
    expr->kind = EXPR_UNARY;
    expr->unary.op = UN_BITNOT;
    expr->unary.expr = parse_unary_expr(p);
    return expr;
  }
  return parse_postfix_expr(p);
}

static rane_expr_t* parse_postfix_expr(rane_parser_t* p) {
  rane_expr_t* e = parse_primary_expr(p);
  if (!e) return NULL;

  for (;;) {
    // member: expr . ident
    if (match(p, TOK_KW_DOT)) {
      rane_token_t dot = p->previous;
      rane_token_t id = consume(p, TOK_IDENTIFIER, "Expect member name after .");

      rane_expr_t* m = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(m, 0, sizeof(*m));
      m->span = span_from_tok(&dot);
      m->kind = EXPR_MEMBER;
      m->member.base = e;

      size_t n = id.length;
      if (n >= sizeof(m->member.member)) n = sizeof(m->member.member) - 1;
      memcpy(m->member.member, id.start, n);
      m->member.member[n] = 0;

      e = m;
      continue;
    }

    // index: expr [ expr ]
    if (match(p, TOK_KW_LBRACKET)) {
      rane_token_t lb = p->previous;
      rane_expr_t* idx = parse_expr(p);
      consume(p, TOK_KW_RBRACKET, "Expect ] after index expression");

      rane_expr_t* x = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(x, 0, sizeof(*x));
      x->span = span_from_tok(&lb);
      x->kind = EXPR_INDEX;
      x->index.base = e;
      x->index.index = idx;

      e = x;
      continue;
    }

    // call: expr ( args... )
    if (match(p, TOK_KW_LPAREN)) {
      rane_token_t lp = p->previous;

      rane_expr_t** args = NULL;
      uint32_t argc = 0;
      if (!check(p, TOK_KW_RPAREN)) {
        args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 8);
        argc = 0;
        args[argc++] = parse_expr(p);
        while (match(p, TOK_KW_COMMA)) {
          if (argc >= 8) break;
          args[argc++] = parse_expr(p);
        }
      }
      consume(p, TOK_KW_RPAREN, "Expect ) after call arguments");

      rane_expr_t* c = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(c, 0, sizeof(*c));
      c->span = span_from_tok(&lp);
      c->kind = EXPR_CALL;
      c->call.callee = e;
      c->call.name[0] = 0;
      c->call.args = args;
      c->call.arg_count = argc;

      e = c;
      continue;
    }

    break;
  }

  return e;
}

static rane_expr_t* parse_primary_expr(rane_parser_t* p) {
  // ident literal: #identifier
  if (match(p, TOK_KW_HASH)) {
    rane_token_t ht = p->previous;
    rane_token_t id = consume(p, TOK_IDENTIFIER, "Expect identifier after #");

    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&ht);
    e->kind = EXPR_LIT_IDENT;

    size_t n = id.length;
    if (n >= sizeof(e->lit_ident.value)) n = sizeof(e->lit_ident.value) - 1;
    memcpy(e->lit_ident.value, id.start, n);
    e->lit_ident.value[n] = 0;
    return e;
  }

  // null literal
  if (match(p, TOK_KW_NULL)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_LIT_NULL;
    e->lit_null.reserved = 0;
    return e;
  }

  if (match(p, TOK_BOOL_LITERAL)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_LIT_BOOL;
    if (t.length == 4 && memcmp(t.start, "true", 4) == 0) e->lit_bool.value = 1;
    else e->lit_bool.value = 0;
    return e;
  }

  if (match(p, TOK_INT_LITERAL)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_LIT_INT;
    e->lit_int.value = parse_u64_literal(t);
    e->lit_int.type = RANE_TYPE_U64;
    return e;
  }

  if (match(p, TOK_STRING_LITERAL)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_LIT_TEXT;
    e->lit_text.start = t.start;
    e->lit_text.length = (uint32_t)t.length;
    return e;
  }

  // choose max/min
  if (match(p, TOK_KW_CHOOSE)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_CHOOSE;

    if (match(p, TOK_KW_MAX)) e->choose.kind = (decltype(e->choose.kind))0;
    else if (match(p, TOK_KW_MIN)) e->choose.kind = (decltype(e->choose.kind))1;
    else e->choose.kind = (decltype(e->choose.kind))0;

    consume(p, TOK_KW_LPAREN, "Expect ( after choose kind");
    e->choose.a = parse_expr(p);
    consume(p, TOK_KW_COMMA, "Expect , in choose");
    e->choose.b = parse_expr(p);
    consume(p, TOK_KW_RPAREN, "Expect ) after choose");
    return e;
  }

  if (match(p, TOK_KW_LPAREN)) {
    rane_expr_t* e = parse_expr(p);
    consume(p, TOK_KW_RPAREN, "Expect ) after expression");
    return e;
  }

  // addr/load/store/call/var (identifier-driven)
  if (match(p, TOK_IDENTIFIER)) {
    rane_token_t t = p->previous;
    char name[64] = {0};
    size_t n = t.length;
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    memcpy(name, t.start, n);
    name[n] = 0;

    // v1 struct literal: TypeName{...} or TypeName(...)
    if (check(p, TOK_KW_LBRACE) || check(p, TOK_KW_LPAREN)) {
      rane_expr_t* sl = parse_v1_struct_literal_after_type_name(p, t);
      if (sl) return sl;
      if (p->had_error) return NULL;
    }

    if (strcmp(name, "addr") == 0 && check(p, TOK_KW_LPAREN)) {
      advance(p);

      rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(e, 0, sizeof(*e));
      e->span = span_from_tok(&t);
      e->kind = EXPR_ADDR;

      e->addr.base = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after addr base");
      e->addr.index = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after addr index");

      rane_token_t scale_tok = consume(p, TOK_INT_LITERAL, "Expect integer scale in addr(...)");
      char buf[64];
      size_t bn = scale_tok.length;
      if (bn >= sizeof(buf)) bn = sizeof(buf) - 1;
      memcpy(buf, scale_tok.start, bn);
      buf[bn] = 0;
      e->addr.scale = (uint64_t)_strtoui64(buf, NULL, 10);

      consume(p, TOK_KW_COMMA, "Expect , after addr scale");

      rane_token_t disp_tok = consume(p, TOK_INT_LITERAL, "Expect integer displacement in addr(...)");
      bn = disp_tok.length;
      if (bn >= sizeof(buf)) bn = sizeof(buf) - 1;
      memcpy(buf, disp_tok.start, bn);
      buf[bn] = 0;
      e->addr.disp = (uint64_t)_strtoui64(buf, NULL, 10);

      consume(p, TOK_KW_RPAREN, "Expect ) to close addr(...)");
      return e;
    }

    if (strcmp(name, "load") == 0 && check(p, TOK_KW_LPAREN)) {
      advance(p);

      rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(e, 0, sizeof(*e));
      e->span = span_from_tok(&t);
      e->kind = EXPR_LOAD;
      e->load.type = RANE_TYPE_U64;
      e->load.volatility = 0;

      rane_token_t ty = consume(p, TOK_IDENTIFIER, "Expect type name in load(type, addr)");
      char tname[64] = {0};
      size_t tn = ty.length;
      if (tn >= sizeof(tname)) tn = sizeof(tname) - 1;
      memcpy(tname, ty.start, tn);
      tname[tn] = 0;

      if (strcmp(tname, "u8") == 0) e->load.type = RANE_TYPE_U8;
      else if (strcmp(tname, "u16") == 0) e->load.type = RANE_TYPE_U16;
      else if (strcmp(tname, "u32") == 0) e->load.type = RANE_TYPE_U32;
      else if (strcmp(tname, "u64") == 0) e->load.type = RANE_TYPE_U64;
      else if (strcmp(tname, "i8") == 0) e->load.type = RANE_TYPE_I8;
      else if (strcmp(tname, "i16") == 0) e->load.type = RANE_TYPE_I16;
      else if (strcmp(tname, "i32") == 0) e->load.type = RANE_TYPE_I32;
      else if (strcmp(tname, "i64") == 0) e->load.type = RANE_TYPE_I64;
      else if (strcmp(tname, "p64") == 0) e->load.type = RANE_TYPE_P64;
      else if (strcmp(tname, "b1") == 0 || strcmp(tname, "bool") == 0) e->load.type = RANE_TYPE_B1;

      consume(p, TOK_KW_COMMA, "Expect , after load type");
      e->load.addr_expr = parse_expr(p);
      consume(p, TOK_KW_RPAREN, "Expect ) to close load(...)");
      return e;
    }

    if (strcmp(name, "store") == 0 && check(p, TOK_KW_LPAREN)) {
      advance(p);

      rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(e, 0, sizeof(*e));
      e->span = span_from_tok(&t);
      e->kind = EXPR_STORE;
      e->store.type = RANE_TYPE_U64;
      e->store.volatility = 0;

      rane_token_t ty = consume(p, TOK_IDENTIFIER, "Expect type name in store(type, addr, value)");
      char tname[64] = {0};
      size_t tn = ty.length;
      if (tn >= sizeof(tname)) tn = sizeof(tname) - 1;
      memcpy(tname, ty.start, tn);
      tname[tn] = 0;

      if (strcmp(tname, "u8") == 0) e->store.type = RANE_TYPE_U8;
      else if (strcmp(tname, "u16") == 0) e->store.type = RANE_TYPE_U16;
      else if (strcmp(tname, "u32") == 0) e->store.type = RANE_TYPE_U32;
      else if (strcmp(tname, "u64") == 0) e->store.type = RANE_TYPE_U64;
      else if (strcmp(tname, "i8") == 0) e->store.type = RANE_TYPE_I8;
      else if (strcmp(tname, "i16") == 0) e->store.type = RANE_TYPE_I16;
      else if (strcmp(tname, "i32") == 0) e->store.type = RANE_TYPE_I32;
      else if (strcmp(tname, "i64") == 0) e->store.type = RANE_TYPE_I64;
      else if (strcmp(tname, "p64") == 0) e->store.type = RANE_TYPE_P64;
      else if (strcmp(tname, "b1") == 0 || strcmp(tname, "bool") == 0) e->store.type = RANE_TYPE_B1;

      consume(p, TOK_KW_COMMA, "Expect , after store type");
      e->store.addr_expr = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after store addr");
      e->store.value_expr = parse_expr(p);
      consume(p, TOK_KW_RPAREN, "Expect ) to close store(...)");
      return e;
    }

    if (match(p, TOK_KW_LPAREN)) {
      rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(e, 0, sizeof(*e));
      e->span = span_from_tok(&t);
      e->kind = EXPR_CALL;
      memcpy(e->call.name, name, sizeof(e->call.name));
      e->call.callee = NULL;
      e->call.args = NULL;
      e->call.arg_count = 0;

      if (!check(p, TOK_KW_RPAREN)) {
        e->call.args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 8);
        e->call.arg_count = 0;
        e->call.args[e->call.arg_count++] = parse_expr(p);
        while (match(p, TOK_KW_COMMA)) {
          e->call.args[e->call.arg_count++] = parse_expr(p);
          if (e->call.arg_count >= 8) break;
        }
      }

      consume(p, TOK_KW_RPAREN, "Expect ) after call arguments");
      return e;
    }

    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_VAR;
    memcpy(e->var.name, t.start, t.length < sizeof(e->var.name) ? t.length : sizeof(e->var.name) - 1);
    e->var.name[t.length < sizeof(e->var.name) ? t.length : sizeof(e->var.name) - 1] = 0;
    return e;
  }

  return NULL;
}

static rane_stmt_t* parse_node_stmt(rane_parser_t* p, rane_token_t node_tok) {
  rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(s, 0, sizeof(*s));
  s->span = span_from_tok(&node_tok);
  s->kind = STMT_NODE;

  rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect node name after node");
  size_t n = name_tok.length;
  if (n >= sizeof(s->node_decl.name)) n = sizeof(s->node_decl.name) - 1;
  memcpy(s->node_decl.name, name_tok.start, n);
  s->node_decl.name[n] = 0;

  consume(p, TOK_KW_COLON, "Expect : after node name");

  // Node body: parse until 'end'
  rane_stmt_t* b = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(b, 0, sizeof(*b));
  b->span = span_from_tok(&node_tok);
  b->kind = STMT_BLOCK;
  b->block.stmts = (rane_stmt_t**)malloc(sizeof(rane_stmt_t*) * 256);
  b->block.stmt_count = 0;

  while (!check(p, TOK_EOF) && !check(p, TOK_KW_END)) {
    rane_stmt_t* st = parse_stmt(p);
    if (!st) {
      // If terminator, stop.
      if (check(p, TOK_KW_END)) break;

      // If an actual error was raised, stop.
      if (p->had_error) break;

      // Otherwise, recover: consume one token to avoid an infinite loop.
      advance(p);
      continue;
    }

    b->block.stmts[b->block.stmt_count++] = st;
    consume_optional_semicolon(p);
  }

  consume(p, TOK_KW_END, "Expect end to close node");

  s->node_decl.body = b;
  consume_optional_semicolon(p);
  return s;
}

static rane_stmt_t* parse_stmt_v1_struct_decl(rane_parser_t* p) {
  rane_token_t kw = p->previous;

  rane_token_t name = consume(p, TOK_IDENTIFIER, "Expect struct name");
  consume(p, TOK_KW_COLON, "Expect : after struct name");

  rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(s, 0, sizeof(*s));
  s->span = span_from_tok(&kw);
  s->kind = STMT_STRUCT_DECL;
  tok_to_ident(s->struct_decl.name, name);
  s->struct_decl.field_count = 0;

  while (!check(p, TOK_EOF) && !check(p, TOK_KW_END)) {
    // field: name: type
    if (check(p, TOK_IDENTIFIER)) {
      rane_token_t fn = p->current;
      advance(p);
      consume(p, TOK_KW_COLON, "Expect : after field name");

      if (s->struct_decl.field_count >= 32) {
        p->had_error = 1;
        set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Too many struct fields");
        break;
      }

      uint32_t i = s->struct_decl.field_count++;
      tok_to_ident(s->struct_decl.fields[i].name, fn);
      parse_type_name_after_colon(p, &s->struct_decl.fields[i].type_name);
      continue;
    }

    // Skip unexpected tokens to avoid infinite loops
    advance(p);
  }

  consume(p, TOK_KW_END, "Expect end to close struct");
  return s;
}

static rane_stmt_t* parse_stmt_v1_set(rane_parser_t* p) {
  rane_token_t kw = p->previous;

  rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(s, 0, sizeof(*s));
  s->span = span_from_tok(&kw);
  s->kind = STMT_SET;

  // Lookahead: if it starts with identifier and next token is ':' => declaration form.
  if (check(p, TOK_IDENTIFIER)) {
    rane_token_t first = p->current;
    advance(p);

    if (match(p, TOK_KW_COLON)) {
      tok_to_ident(s->set_stmt.name, first);
      // already consumed ':', parse type
      parse_type_name_after_colon(p, &s->set_stmt.type_name);
      consume(p, TOK_KW_TO_KW, "Expect to in set declaration");
      s->set_stmt.value = parse_expr(p);
      s->set_stmt.target_expr = NULL;
      return s;
    }

    // Not a decl; treat the identifier as the start of a target expression.
    // Put it back into an EXPR_VAR node, then parse postfix (member etc.)
    rane_expr_t* base = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(base, 0, sizeof(*base));
    base->span = span_from_tok(&first);
    base->kind = EXPR_VAR;
    tok_to_ident(base->var.name, first);

    // Continue parsing member/call/index on top of base by reusing postfix loop.
    // We fake the parser state by setting up a tiny wrapper: parse_postfix_expr expects primary to be parsed.
    // Easiest: temporarily treat base as current expression and manually replicate postfix parsing here.
    rane_expr_t* e = base;
    for (;;) {
      if (match(p, TOK_KW_DOT)) {
        rane_token_t dot = p->previous;
        rane_token_t id = consume(p, TOK_IDENTIFIER, "Expect member name after .");

        rane_expr_t* m = (rane_expr_t*)malloc(sizeof(rane_expr_t));
        memset(m, 0, sizeof(*m));
        m->span = span_from_tok(&dot);
        m->kind = EXPR_MEMBER;
        m->member.base = e;
        tok_to_ident(m->member.member, id);
        e = m;
        continue;
      }
      break;
    }

    s->set_stmt.target_expr = e;
    s->set_stmt.name[0] = 0;
    s->set_stmt.type_name.name[0] = 0;

    // v1 uses `to` keyword already tokenized as TOK_KW_TO_KW.
    consume(p, TOK_KW_TO_KW, "Expect to in set");
    s->set_stmt.value = parse_expr(p);
    return s;
  }

  p->had_error = 1;
  set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Expect identifier after set");
  return s;
}

static rane_stmt_t* parse_stmt_v1_add(rane_parser_t* p) {
  rane_token_t kw = p->previous;

  rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(s, 0, sizeof(*s));
  s->span = span_from_tok(&kw);
  s->kind = STMT_ADD;

  // Parse target (identifier with optional .member)
  rane_expr_t* tgt = NULL;
  if (match(p, TOK_IDENTIFIER)) {
    rane_token_t id = p->previous;
    rane_expr_t* base = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(base, 0, sizeof(*base));
    base->span = span_from_tok(&id);
    base->kind = EXPR_VAR;
    tok_to_ident(base->var.name, id);

    rane_expr_t* e = base;
    while (match(p, TOK_KW_DOT)) {
      rane_token_t dot = p->previous;
      rane_token_t mem = consume(p, TOK_IDENTIFIER, "Expect member name after .");
      rane_expr_t* m = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(m, 0, sizeof(*m));
      m->span = span_from_tok(&dot);
      m->kind = EXPR_MEMBER;
      m->member.base = e;
      tok_to_ident(m->member.member, mem);
      e = m;
    }

    tgt = e;
  } else {
    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Expect target after add");
  }

  consume(p, TOK_KW_BY, "Expect by in add statement");
  rane_expr_t* v = parse_expr(p);

  s->add_stmt.target_expr = tgt;
  s->add_stmt.value = v;
  return s;
}

static rane_stmt_t* parse_stmt(rane_parser_t* p) {
  // Terminator keyword for v1 blocks (node/struct). It is consumed by the enclosing parser.
  if (check(p, TOK_KW_END)) {
    return NULL;
  }

  // v1 node bodies often contain `halt` (tokenized as IDENTIFIER in this lexer).
  if (check(p, TOK_IDENTIFIER) && p->current.length == 4 && memcmp(p->current.start, "halt", 4) == 0) {
    advance(p);
    rane_token_t t = p->previous;
    consume_optional_semicolon(p);

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&t);
    s->kind = STMT_PROC_CALL;
    strcpy_s(s->proc_call.proc_name, sizeof(s->proc_call.proc_name), "__rane_halt");
    s->proc_call.slot = 0;
    s->proc_call.args = NULL;
    s->proc_call.arg_count = 0;
    return s;
  }

  // --- v1 prose/node surface ---
  if (match(p, TOK_KW_MODULE)) {
    rane_token_t t = p->previous;
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&t);
    s->kind = STMT_MODULE;

    rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect module name after module");
    size_t n = name_tok.length;
    if (n >= sizeof(s->module_decl.name)) n = sizeof(s->module_decl.name) - 1;
    memcpy(s->module_decl.name, name_tok.start, n);
    s->module_decl.name[n] = 0;

    consume_optional_semicolon(p);
    return s;
  }

  if (match(p, TOK_KW_NODE)) {
    return parse_node_stmt(p, p->previous);
  }

  if (match(p, TOK_KW_STRUCT)) return parse_stmt_v1_struct_decl(p);
  if (match(p, TOK_KW_SET)) return parse_stmt_v1_set(p);
  if (match(p, TOK_KW_ADD)) return parse_stmt_v1_add(p);

  // start at node <ident>
  if (match(p, TOK_KW_START)) {
    rane_token_t st = p->previous;
    // accept: start at node X
    if (match(p, TOK_KW_AT_KW)) {
      // accept optional 'node'
      if (check(p, TOK_KW_NODE)) advance(p);

      rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect node name after start at node");
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&st);
      s->kind = STMT_START_AT;

      size_t n = name_tok.length;
      if (n >= sizeof(s->start_at.node_name)) n = sizeof(s->start_at.node_name) - 1;
      memcpy(s->start_at.node_name, name_tok.start, n);
      s->start_at.node_name[n] = 0;

      consume_optional_semicolon(p);
      return s;
    }

    // fallback to existing parser behavior (not a v1 start decl)
    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Expect 'at' after start");
    return NULL;
  }

  // go to node <ident>
  if (match(p, TOK_KW_GO)) {
    rane_token_t gt = p->previous;
    consume(p, TOK_KW_TO_KW, "Expect to after go");
    // accept optional 'node'
    if (check(p, TOK_KW_NODE)) advance(p);

    rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect node name after go to node");
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&gt);
    s->kind = STMT_GO_NODE;

    size_t n = name_tok.length;
    if (n >= sizeof(s->go_node.node_name)) n = sizeof(s->go_node.node_name) - 1;
    memcpy(s->go_node.node_name, name_tok.start, n);
    s->go_node.node_name[n] = 0;

    consume_optional_semicolon(p);
    return s;
  }

  // say <expr>
  if (match(p, TOK_KW_SAY)) {
    rane_token_t st = p->previous;
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&st);
    s->kind = STMT_SAY;
    s->say.expr = parse_expr(p);
    consume_optional_semicolon(p);
    return s;
  }

  // break/continue (valid inside repeat/while at lowering time; parser accepts anywhere)
  if (match(p, TOK_KW_BREAK)) {
    rane_token_t t = p->previous;
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&t);
    s->kind = STMT_BREAK;
    consume_optional_semicolon(p);
    return s;
  }

  if (match(p, TOK_KW_CONTINUE)) {
    rane_token_t t = p->previous;
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&t);
    s->kind = STMT_CONTINUE;
    consume_optional_semicolon(p);
    return s;
  }

  // cjump <expr> -> <true_label> , <false_label>;
  // Use `goto` keyword for now.
  if (match(p, TOK_KW_GOTO)) {
    rane_token_t cj = p->previous;
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&cj);
    s->kind = STMT_CJUMP;

    s->cjump.cond = parse_expr(p);
    consume(p, TOK_KW_ARROW, "Expect -> after cjump condition");

    rane_token_t t = consume(p, TOK_IDENTIFIER, "Expect true label");
    size_t tn = t.length;
    if (tn >= sizeof(s->cjump.true_marker)) tn = sizeof(s->cjump.true_marker) - 1;
    memcpy(s->cjump.true_marker, t.start, tn);
    s->cjump.true_marker[tn] = 0;

    consume(p, TOK_KW_COMMA, "Expect , between cjump labels");

    rane_token_t f = consume(p, TOK_IDENTIFIER, "Expect false label");
    size_t fn = f.length;
    if (fn >= sizeof(s->cjump.false_marker)) fn = sizeof(s->cjump.false_marker) - 1;
    memcpy(s->cjump.false_marker, f.start, fn);
    s->cjump.false_marker[fn] = 0;

    consume_optional_semicolon(p);
    return s;
  }

  // call statement: `call name(args...) [into slot N];`
  if (match(p, TOK_KW_CALL)) {
    rane_token_t call_tok = p->previous;

    rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect identifier after call");
    char name[64] = {0};
    size_t nn = name_tok.length;
    if (nn >= sizeof(name)) nn = sizeof(name) - 1;
    memcpy(name, name_tok.start, nn);
    name[nn] = 0;

    consume(p, TOK_KW_LPAREN, "Expect ( after call name");

    rane_expr_t** args = NULL;
    uint32_t argc = 0;
    if (!check(p, TOK_KW_RPAREN)) {
      args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 8);
      argc = 0;
      args[argc++] = parse_expr(p);
      while (match(p, TOK_KW_COMMA)) {
        if (argc >= 8) break;
        args[argc++] = parse_expr(p);
      }
    }
    consume(p, TOK_KW_RPAREN, "Expect ) after call arguments");

    uint32_t out_slot = 0;
    if (match(p, TOK_KW_INTO)) {
      consume(p, TOK_KW_SLOT, "Expect slot after into");
      rane_token_t sl = consume(p, TOK_INT_LITERAL, "Expect integer slot index");
      char buf[32];
      size_t n = sl.length;
      if (n >= sizeof(buf)) n = sizeof(buf) - 1;
      memcpy(buf, sl.start, n);
      buf[n] = 0;
      out_slot = (uint32_t)_strtoui64(buf, NULL, 10);
    }

    consume_optional_semicolon(p);

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&call_tok);
    s->kind = STMT_PROC_CALL;
    strncpy_s(s->proc_call.proc_name, sizeof(s->proc_call.proc_name), name, _TRUNCATE);
    s->proc_call.slot = out_slot;
    s->proc_call.args = args;
    s->proc_call.arg_count = argc;
    return s;
  }

  // chan push/pop
  if (match(p, TOK_KW_CHAN)) {
    rane_token_t chan_tok = p->previous;

    if (match(p, TOK_KW_PUSH)) {
      rane_token_t cname = consume(p, TOK_IDENTIFIER, "Expect channel name");
      consume(p, TOK_KW_COMMA, "Expect , after channel name");
      rane_expr_t* v = parse_expr(p);
      consume_optional_semicolon(p);

      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&chan_tok);
      s->kind = STMT_CHAN_PUSH;

      size_t n = cname.length;
      if (n >= sizeof(s->chan_push.chan)) n = sizeof(s->chan_push.chan) - 1;
      memcpy(s->chan_push.chan, cname.start, n);
      s->chan_push.chan[n] = 0;
      s->chan_push.value = v;
      return s;
    }

    if (match(p, TOK_KW_POP)) {
      rane_token_t cname = consume(p, TOK_IDENTIFIER, "Expect channel name");
      consume(p, TOK_KW_INTO, "Expect into after chan pop");
      rane_token_t target = consume(p, TOK_IDENTIFIER, "Expect target variable");
      consume_optional_semicolon(p);

      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&chan_tok);
      s->kind = STMT_CHAN_POP;

      size_t n = cname.length;
      if (n >= sizeof(s->chan_pop.chan)) n = sizeof(s->chan_pop.chan) - 1;
      memcpy(s->chan_pop.chan, cname.start, n);
      s->chan_pop.chan[n] = 0;

      size_t tn = target.length;
      if (tn >= sizeof(s->chan_pop.target)) tn = sizeof(s->chan_pop.target) - 1;
      memcpy(s->chan_pop.target, target.start, tn);
      s->chan_pop.target[tn] = 0;
      return s;
    }

    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Unexpected token after chan");
    return NULL;
  }

  // mmio region
  if (match(p, TOK_KW_MMIO)) {
    rane_token_t mm = p->previous;
    consume(p, TOK_KW_REGION, "Expect region after mmio");

    rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect region name");
    consume(p, TOK_KW_FROM, "Expect from after region name");
    rane_token_t base_tok = consume(p, TOK_INT_LITERAL, "Expect base address integer");
    consume(p, TOK_KW_SIZE, "Expect size after base");
    rane_token_t size_tok = consume(p, TOK_INT_LITERAL, "Expect size integer");
    consume_optional_semicolon(p);

    char buf[64];
    size_t n = base_tok.length;
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, base_tok.start, n);
    buf[n] = 0;
    uint64_t base = (uint64_t)_strtoui64(buf, NULL, 10);

    n = size_tok.length;
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, size_tok.start, n);
    buf[n] = 0;
    uint64_t sz = (uint64_t)_strtoui64(buf, NULL, 10);

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&mm);
    s->kind = STMT_MMIO_REGION_DECL;

    size_t rn = name_tok.length;
    if (rn >= sizeof(s->mmio_region_decl.name)) rn = sizeof(s->mmio_region_decl.name) - 1;
    memcpy(s->mmio_region_decl.name, name_tok.start, rn);
    s->mmio_region_decl.name[rn] = 0;

    s->mmio_region_decl.base = base;
    s->mmio_region_decl.size = sz;
    return s;
  }

  // read32/write32
  if (match(p, TOK_KW_READ32)) {
    rane_token_t rt = p->previous;
    rane_token_t region = consume(p, TOK_IDENTIFIER, "Expect region name after read32");
    consume(p, TOK_KW_COMMA, "Expect , after region");
    rane_expr_t* off = parse_expr(p);
    consume(p, TOK_KW_INTO, "Expect into");
    rane_token_t target = consume(p, TOK_IDENTIFIER, "Expect target variable");
    consume_optional_semicolon(p);

    rane_expr_t* addr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(addr, 0, sizeof(*addr));
    addr->span = span_from_tok(&rt);
    addr->kind = EXPR_MMIO_ADDR;

    size_t rn = region.length;
    if (rn >= sizeof(addr->mmio_addr.region)) rn = sizeof(addr->mmio_addr.region) - 1;
    memcpy(addr->mmio_addr.region, region.start, rn);
    addr->mmio_addr.region[rn] = 0;
    addr->mmio_addr.offset = off;

    rane_expr_t* load = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(load, 0, sizeof(*load));
    load->span = span_from_tok(&rt);
    load->kind = EXPR_LOAD;
    load->load.type = RANE_TYPE_U32;
    load->load.addr_expr = addr;
    load->load.volatility = 1;

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&rt);
    s->kind = STMT_ASSIGN;

    size_t tn = target.length;
    if (tn >= sizeof(s->assign.target)) tn = sizeof(s->assign.target) - 1;
    memcpy(s->assign.target, target.start, tn);
    s->assign.target[tn] = 0;
    s->assign.expr = load;
    return s;
  }

  if (match(p, TOK_KW_WRITE32)) {
    rane_token_t wt = p->previous;
    rane_token_t region = consume(p, TOK_IDENTIFIER, "Expect region name after write32");
    consume(p, TOK_KW_COMMA, "Expect , after region");
    rane_expr_t* off = parse_expr(p);
    consume(p, TOK_KW_COMMA, "Expect , after offset");
    rane_expr_t* val = parse_expr(p);
    consume_optional_semicolon(p);

    rane_expr_t* addr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(addr, 0, sizeof(*addr));
    addr->span = span_from_tok(&wt);
    addr->kind = EXPR_MMIO_ADDR;

    size_t rn = region.length;
    if (rn >= sizeof(addr->mmio_addr.region)) rn = sizeof(addr->mmio_addr.region) - 1;
    memcpy(addr->mmio_addr.region, region.start, rn);
    addr->mmio_addr.region[rn] = 0;
    addr->mmio_addr.offset = off;

    rane_expr_t* st = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(st, 0, sizeof(*st));
    st->span = span_from_tok(&wt);
    st->kind = EXPR_STORE;
    st->store.type = RANE_TYPE_U32;
    st->store.addr_expr = addr;
    st->store.value_expr = val;
    st->store.volatility = 1;

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&wt);
    s->kind = STMT_ASSIGN;
    strcpy_s(s->assign.target, sizeof(s->assign.target), "_");
    s->assign.expr = st;
    return s;
  }

  // mem copy
  if (match(p, TOK_KW_MEM)) {
    rane_token_t mt = p->previous;

    if (match(p, TOK_KW_COPY)) {
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&mt);
      s->kind = STMT_MEM_COPY;

      s->mem_copy.dst = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after dst");
      s->mem_copy.src = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after src");
      s->mem_copy.size = parse_expr(p);
      consume_optional_semicolon(p);
      return s;
    }

    // mem zero dst, size;
    if (match(p, TOK_KW_ZERO)) {
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&mt);
      s->kind = STMT_PROC_CALL;
      strcpy_s(s->proc_call.proc_name, sizeof(s->proc_call.proc_name), "memset");

      rane_expr_t** args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 3);
      uint32_t argc = 0;

      args[argc++] = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after dst");

      // value=0
      rane_expr_t* zero = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(zero, 0, sizeof(*zero));
      zero->span = span_from_tok(&mt);
      zero->kind = EXPR_LIT_INT;
      zero->lit_int.type = RANE_TYPE_U64;
      zero->lit_int.value = 0;
      args[argc++] = zero;

      consume(p, TOK_KW_COMMA, "Expect , after value");
      args[argc++] = parse_expr(p);

      consume_optional_semicolon(p);

      s->proc_call.slot = 0;
      s->proc_call.args = args;
      s->proc_call.arg_count = argc;
      return s;
    }

    // mem fill dst, value, size;
    if (match(p, TOK_KW_FILL)) {
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&mt);
      s->kind = STMT_PROC_CALL;
      strcpy_s(s->proc_call.proc_name, sizeof(s->proc_call.proc_name), "memset");

      rane_expr_t** args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 3);
      uint32_t argc = 0;

      args[argc++] = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after dst");
      args[argc++] = parse_expr(p);
      consume(p, TOK_KW_COMMA, "Expect , after value");
      args[argc++] = parse_expr(p);

      consume_optional_semicolon(p);

      s->proc_call.slot = 0;
      s->proc_call.args = args;
      s->proc_call.arg_count = argc;
      return s;
    }

    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Unexpected token after mem");
    return NULL;
  }

  // --- control/termination keywords ---
  // halt;
  if (match(p, TOK_KW_HALT)) {
    rane_token_t t = p->previous;
    consume_optional_semicolon(p);

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&t);
    s->kind = STMT_PROC_CALL;
    strcpy_s(s->proc_call.proc_name, sizeof(s->proc_call.proc_name), "__rane_halt");
    s->proc_call.slot = 0;
    s->proc_call.args = NULL;
    s->proc_call.arg_count = 0;
    return s;
  }

  // trap [code];
  if (match(p, TOK_KW_TRAP_OP) || match(p, TOK_KW_TRAP)) {
    rane_token_t t = p->previous;

    rane_expr_t** args = NULL;
    uint32_t argc = 0;

    // Optional immediate/int expression argument.
    if (!check(p, TOK_KW_SEMICOLON)) {
      args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 1);
      argc = 1;
      args[0] = parse_expr(p);
    }

    consume_optional_semicolon(p);

    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&t);
    s->kind = STMT_PROC_CALL;
    strcpy_s(s->proc_call.proc_name, sizeof(s->proc_call.proc_name), "__rane_trap");
    s->proc_call.slot = 0;
    s->proc_call.args = args;
    s->proc_call.arg_count = argc;
    return s;
  }

  // If we get here and the next token looks like a statement start, surface a useful error.
  if (check(p, TOK_IDENTIFIER)) {
    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Unexpected statement (unrecognized identifier at statement position)");
    return NULL;
  }

  return NULL;
}

// Helper: copy identifier token into fixed buffer
static void tok_to_ident(char out[64], const rane_token_t& t) {
  if (!out) return;
  size_t n = t.length;
  if (n >= 64) n = 63;
  memcpy(out, t.start, n);
  out[n] = 0;
}

static int is_type_keyword_token(rane_token_type_t t) {
  // Minimal primitive set for v1 examples.
  // Tokenizer currently treats these as TOK_IDENTIFIER.
  (void)t;
  return 0;
}

static void parse_type_name_after_colon(rane_parser_t* p, rane_type_name_t* out_ty) {
  if (!out_ty) return;
  memset(out_ty, 0, sizeof(*out_ty));
  rane_token_t t = consume(p, TOK_IDENTIFIER, "Expect type name");
  tok_to_ident(out_ty->name, t);
}

static rane_expr_t* parse_v1_struct_literal_after_type_name(rane_parser_t* p, const rane_token_t& type_tok) {
  char type_name[64] = {0};
  tok_to_ident(type_name, type_tok);

  rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
  memset(e, 0, sizeof(*e));
  e->span = span_from_tok(&type_tok);
  e->kind = EXPR_STRUCT_LITERAL;
  strncpy_s(e->struct_lit.type_name, sizeof(e->struct_lit.type_name), type_name, _TRUNCATE);
  e->struct_lit.named_fields = 0;
  e->struct_lit.field_count = 0;
  e->struct_lit.pos_count = 0;

  // Named-field: TypeName{ field: expr ... }
  if (match(p, TOK_KW_LBRACE)) {
    e->struct_lit.named_fields = 1;

    // Allow empty literal {}
    while (!check(p, TOK_EOF) && !check(p, TOK_KW_RBRACE)) {
      rane_token_t fn = consume(p, TOK_IDENTIFIER, "Expect field name in struct literal");
      consume(p, TOK_KW_COLON, "Expect : after field name in struct literal");
      rane_expr_t* v = parse_expr(p);

      if (e->struct_lit.field_count < 32) {
        uint32_t i = e->struct_lit.field_count++;
        tok_to_ident(e->struct_lit.fields[i].name, fn);
        e->struct_lit.fields[i].value = v;
      } else {
        p->had_error = 1;
        set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Too many fields in struct literal");
        break;
      }

      // Optional separators: comma or semicolon; otherwise whitespace/newline is already skipped.
      if (match(p, TOK_KW_COMMA) || match(p, TOK_KW_SEMICOLON)) {
        continue;
      }
    }

    consume(p, TOK_KW_RBRACE, "Expect } to close struct literal");
    return e;
  }

  // Positional: TypeName(expr, ...)
  if (match(p, TOK_KW_LPAREN)) {
    e->struct_lit.named_fields = 0;

    if (!check(p, TOK_KW_RPAREN)) {
      while (!check(p, TOK_EOF)) {
        if (e->struct_lit.pos_count >= 32) {
          p->had_error = 1;
          set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Too many positional args in struct literal");
          break;
        }
        e->struct_lit.pos_args[e->struct_lit.pos_count++] = parse_expr(p);
        if (!match(p, TOK_KW_COMMA)) break;
      }
    }

    consume(p, TOK_KW_RPAREN, "Expect ) to close struct literal");
    return e;
  }

  // Not a struct literal.
  free(e);
  return NULL;
}


// ---------------------------
// Public parse APIs
// ---------------------------

rane_error_t rane_parse_source_len_ex(const char* source, size_t len, rane_stmt_t** out_ast_root, rane_diag_t* out_diag) {
  if (!source || !out_ast_root) return RANE_E_INVALID_ARG;
  rane_parser_t p;
  parser_init(&p, source, len, out_diag);

  *out_ast_root = parse_program(&p);

  while (!p.had_error && p.current.type != TOK_EOF) {
    advance(&p);
  }

  if (p.had_error) return RANE_E_INVALID_ARG;
  if (*out_ast_root && p.current.type == TOK_EOF) return RANE_OK;
  if (out_diag && out_diag->code == RANE_DIAG_OK) {
    set_diag(&p, RANE_DIAG_PARSE_ERROR, p.current, "Unexpected trailing tokens");
  }
  return RANE_E_INVALID_ARG;
}

rane_error_t rane_parse_source_len(const char* source, size_t len, rane_stmt_t** out_ast_root) {
  return rane_parse_source_len_ex(source, len, out_ast_root, NULL);
}

rane_error_t rane_parse_source_ex(const char* source, rane_stmt_t** out_ast_root, rane_diag_t* out_diag) {
  if (!source || !out_ast_root) return RANE_E_INVALID_ARG;
  return rane_parse_source_len_ex(source, strlen(source), out_ast_root, out_diag);
}

rane_error_t rane_parse_source(const char* source, rane_stmt_t** out_ast_root) {
  return rane_parse_source_ex(source, out_ast_root, NULL);
}