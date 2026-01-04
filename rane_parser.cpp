#include "rane_parser.h"
#include "rane_lexer.h"
#include "rane_ast.h"
#include "rane_diag.h"
#include <stdlib.h>
#include <string.h>

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
  p->current = rane_lexer_next(&p->lexer);
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

// Forward declarations
static rane_expr_t* parse_expr(rane_parser_t* p);
static rane_expr_t* parse_unary_expr(rane_parser_t* p);
static rane_expr_t* parse_primary_expr(rane_parser_t* p);

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

    // Logical (word keywords)
    case TOK_KW_AND:
      return 20;
    case TOK_KW_OR:
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

    // Logical
    case TOK_KW_AND: return BIN_LOGAND;
    case TOK_KW_OR: return BIN_LOGOR;

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
  return parse_binary_rhs(p, 0, lhs);
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
  if (match(p, TOK_KW_NOT)) {
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
  return parse_primary_expr(p);
}

static rane_expr_t* parse_primary_expr(rane_parser_t* p) {
  if (match(p, TOK_BOOL_LITERAL)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);
    e->kind = EXPR_LIT_BOOL;
    // token text is in source; compare directly
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
    char buf[64];
    size_t n = t.length;
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, t.start, n);
    buf[n] = 0;
    e->lit_int.value = (uint64_t)_strtoui64(buf, NULL, 10);
    e->lit_int.type = RANE_TYPE_U64;
    return e;
  }

  if (match(p, TOK_STRING_LITERAL)) {
    rane_token_t t = p->previous;
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(e, 0, sizeof(*e));
    e->span = span_from_tok(&t);

    // Keep existing bootstrap representation: string => heap char* encoded as P64 in EXPR_LIT_INT.
    e->kind = EXPR_LIT_INT;
    size_t n = t.length;
    char* s = (char*)malloc(n + 1);
    if (!s) {
      p->had_error = 1;
      set_diag(p, RANE_DIAG_INTERNAL_ERROR, t, "Out of memory");
      return NULL;
    }
    memcpy(s, t.start, n);
    s[n] = 0;
    e->lit_int.value = (uint64_t)(uintptr_t)s;
    e->lit_int.type = RANE_TYPE_P64;
    return e;
  }

  if (match(p, TOK_IDENTIFIER)) {
    rane_token_t t = p->previous;
    char name[64] = {0};
    size_t n = t.length;
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    memcpy(name, t.start, n);
    name[n] = 0;

    if (match(p, TOK_KW_LPAREN)) {
      rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(e, 0, sizeof(*e));
      e->span = span_from_tok(&t);
      e->kind = EXPR_CALL;
      memcpy(e->call.name, name, sizeof(e->call.name));
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
    memcpy(e->var.name, name, sizeof(e->var.name));
    return e;
  }

  if (match(p, TOK_KW_LPAREN)) {
    rane_expr_t* e = parse_expr(p);
    consume(p, TOK_KW_RPAREN, "Expect ) after expression");
    return e;
  }

  return NULL;
}

static void consume_optional_semicolon(rane_parser_t* p) {
  if (check(p, TOK_KW_SEMICOLON)) advance(p);
}

static rane_stmt_t* parse_stmt(rane_parser_t* p);

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

static rane_stmt_t* parse_program(rane_parser_t* p) {
  rane_stmt_t* b = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(b, 0, sizeof(*b));
  b->kind = STMT_BLOCK;
  b->block.stmts = (rane_stmt_t**)malloc(sizeof(rane_stmt_t*) * 256);
  b->block.stmt_count = 0;

  while (!check(p, TOK_EOF) && !p->had_error) {
    rane_stmt_t* s = parse_stmt(p);
    if (!s) break;
    b->block.stmts[b->block.stmt_count++] = s;
    consume_optional_semicolon(p);
  }

  return b;
}

static rane_stmt_t* parse_proc_stmt(rane_parser_t* p, rane_token_t proc_tok) {
  rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  memset(s, 0, sizeof(*s));
  s->span = span_from_tok(&proc_tok);
  s->kind = STMT_PROC;

  rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect function name after proc");
  size_t n = name_tok.length;
  if (n >= sizeof(s->proc.name)) n = sizeof(s->proc.name) - 1;
  memcpy(s->proc.name, name_tok.start, n);
  s->proc.name[n] = 0;

  consume(p, TOK_KW_LPAREN, "Expect ( after proc name");

  s->proc.params = (char**)malloc(sizeof(char*) * 16);
  s->proc.param_count = 0;
  if (!check(p, TOK_KW_RPAREN)) {
    do {
      rane_token_t pnam = consume(p, TOK_IDENTIFIER, "Expect parameter name");
      size_t pn = pnam.length;
      if (pn == 0) break;
      if (pn > 63) pn = 63;
      char* buf = (char*)malloc(pn + 1);
      memcpy(buf, pnam.start, pn);
      buf[pn] = 0;
      s->proc.params[s->proc.param_count++] = buf;
      if (s->proc.param_count >= 16) break;
    } while (match(p, TOK_KW_COMMA));
  }
  consume(p, TOK_KW_RPAREN, "Expect ) after parameter list");

  rane_token_t lbrace = consume(p, TOK_KW_LBRACE, "Expect { to start proc body");
  s->proc.body = parse_block_stmt(p, lbrace);
  return s;
}

static rane_stmt_t* parse_stmt(rane_parser_t* p) {
  if (match(p, TOK_KW_LBRACE)) {
    return parse_block_stmt(p, p->previous);
  }

  if (match(p, TOK_KW_DEF)) {
    return parse_proc_stmt(p, p->previous);
  }

  if (match(p, TOK_KW_RETURN)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&p->previous);
    s->kind = STMT_RETURN;

    // Allow: return; or return expr;
    if (check(p, TOK_KW_SEMICOLON) || check(p, TOK_KW_RBRACE) || check(p, TOK_EOF)) {
      s->ret.expr = NULL;
      consume_optional_semicolon(p);
      return s;
    }

    s->ret.expr = parse_expr(p);
    consume_optional_semicolon(p);
    return s;
  }

  if (match(p, TOK_KW_LET)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&p->previous);
    s->kind = STMT_LET;

    rane_token_t name_tok = consume(p, TOK_IDENTIFIER, "Expect identifier after let");
    size_t n = name_tok.length;
    if (n >= sizeof(s->let.name)) n = sizeof(s->let.name) - 1;
    memcpy(s->let.name, name_tok.start, n);
    s->let.name[n] = 0;

    s->let.type = RANE_TYPE_U64; // default
    consume(p, TOK_KW_ASSIGN, "Expect = after identifier");
    s->let.expr = parse_expr(p);
    consume_optional_semicolon(p);
    return s;
  }

  if (match(p, TOK_IDENTIFIER)) {
    rane_token_t ident = p->previous;
    char target[64] = {0};
    size_t n = ident.length;
    if (n >= sizeof(target)) n = sizeof(target) - 1;
    memcpy(target, ident.start, n);
    target[n] = 0;

    if (check(p, TOK_KW_LPAREN)) {
      // call statement
      advance(p); // consume '('

      rane_expr_t* call = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      memset(call, 0, sizeof(*call));
      call->span = span_from_tok(&ident);
      call->kind = EXPR_CALL;
      memcpy(call->call.name, target, sizeof(call->call.name));

      if (!check(p, TOK_KW_RPAREN)) {
        call->call.args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 8);
        call->call.arg_count = 0;
        call->call.args[call->call.arg_count++] = parse_expr(p);
        while (match(p, TOK_KW_COMMA)) {
          call->call.args[call->call.arg_count++] = parse_expr(p);
          if (call->call.arg_count >= 8) break;
        }
      }

      consume(p, TOK_KW_RPAREN, "Expect ) after call arguments");
      consume_optional_semicolon(p);

      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&ident);
      s->kind = STMT_ASSIGN;
      strcpy_s(s->assign.target, sizeof(s->assign.target), "_");
      s->assign.expr = call;
      return s;
    }

    if (match(p, TOK_KW_ASSIGN)) {
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&ident);
      s->kind = STMT_ASSIGN;
      memcpy(s->assign.target, target, sizeof(s->assign.target));
      s->assign.expr = parse_expr(p);
      consume(p, TOK_KW_SEMICOLON, "Expect ; after expression");
      return s;
    }

    if (match(p, TOK_KW_SEMICOLON)) {
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      memset(s, 0, sizeof(*s));
      s->span = span_from_tok(&ident);
      s->kind = STMT_MARKER;
      memcpy(s->marker.name, target, sizeof(s->marker.name));
      return s;
    }

    p->had_error = 1;
    set_diag(p, RANE_DIAG_PARSE_ERROR, p->current, "Unexpected identifier");
    return NULL;
  }

  if (match(p, TOK_KW_JUMP)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&p->previous);
    s->kind = STMT_JUMP;
    rane_token_t m = consume(p, TOK_IDENTIFIER, "Expect identifier after jump");
    size_t n = m.length;
    if (n >= sizeof(s->jump.marker)) n = sizeof(s->jump.marker) - 1;
    memcpy(s->jump.marker, m.start, n);
    s->jump.marker[n] = 0;
    consume(p, TOK_KW_SEMICOLON, "Expect ; after marker");
    return s;
  }

  if (match(p, TOK_KW_IF)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&p->previous);
    s->kind = STMT_IF;
    s->if_stmt.cond = parse_expr(p);
    consume(p, TOK_KW_THEN, "Expect then after condition");
    s->if_stmt.then_branch = parse_stmt(p);
    if (match(p, TOK_KW_ELSE)) {
      s->if_stmt.else_branch = parse_stmt(p);
    } else {
      s->if_stmt.else_branch = NULL;
    }
    consume_optional_semicolon(p);
    return s;
  }

  if (match(p, TOK_KW_WHILE)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    memset(s, 0, sizeof(*s));
    s->span = span_from_tok(&p->previous);
    s->kind = STMT_WHILE;
    s->while_stmt.cond = parse_expr(p);
    consume(p, TOK_KW_DO, "Expect do after condition");
    s->while_stmt.body = parse_stmt(p);
    consume_optional_semicolon(p);
    return s;
  }

  return NULL;
}

rane_error_t rane_parse_source_ex(const char* source, rane_stmt_t** out_ast_root, rane_diag_t* out_diag) {
  if (!source || !out_ast_root) return RANE_E_INVALID_ARG;
  rane_parser_t p;
  parser_init(&p, source, strlen(source), out_diag);

  *out_ast_root = parse_program(&p);

  if (p.had_error) return RANE_E_INVALID_ARG;
  if (*out_ast_root && p.current.type == TOK_EOF) return RANE_OK;
  if (out_diag && out_diag->code == RANE_DIAG_OK) {
    set_diag(&p, RANE_DIAG_PARSE_ERROR, p.current, "Unexpected trailing tokens");
  }
  return RANE_E_INVALID_ARG;
}

rane_error_t rane_parse_source(const char* source, rane_stmt_t** out_ast_root) {
  return rane_parse_source_ex(source, out_ast_root, NULL);
}