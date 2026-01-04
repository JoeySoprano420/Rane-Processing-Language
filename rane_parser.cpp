#include "rane_parser.h"
#include "rane_lexer.h"
#include "rane_ast.h"
#include <stdlib.h>
#include <string.h>

// Parser state
typedef struct rane_parser_s {
  rane_lexer_t lexer;
  rane_token_t current;
  rane_token_t previous;
} rane_parser_t;

static void parser_init(rane_parser_t* p, const char* source, size_t len) {
  rane_lexer_init(&p->lexer, source, len);
  p->current = rane_lexer_next(&p->lexer);
  p->previous = rane_token_t{TOK_EOF, NULL, 0, 0, 0};
}

static void advance(rane_parser_t* p) {
  p->previous = p->current;
  p->current = rane_lexer_next(&p->lexer);
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
  // Error handling: for now, return error token
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
    case TOK_KW_LT:
    case TOK_KW_LE:
    case TOK_KW_GT:
    case TOK_KW_GE:
      return 50;
    case TOK_KW_EQ:
    case TOK_KW_NE:
      return 40;
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
    case TOK_KW_LT: return BIN_LT;
    case TOK_KW_LE: return BIN_LE;
    case TOK_KW_GT: return BIN_GT;
    case TOK_KW_GE: return BIN_GE;
    case TOK_KW_EQ: return BIN_EQ;
    case TOK_KW_NE: return BIN_NE;
    default: return BIN_ADD;
  }
}

static rane_expr_t* parse_binary_rhs(rane_parser_t* p, int min_prec, rane_expr_t* lhs) {
  while (1) {
    int prec = token_precedence(p->current.type);
    if (prec < min_prec) break;

    rane_token_type_t op_tok = p->current.type;
    advance(p);

    rane_expr_t* rhs = parse_unary_expr(p);
    if (!rhs) return lhs;

    int next_prec = token_precedence(p->current.type);
    if (next_prec > prec) {
      rhs = parse_binary_rhs(p, prec + 1, rhs);
    }

    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
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
    rane_expr_t* expr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    expr->kind = EXPR_UNARY;
    expr->unary.op = UN_NEG;
    expr->unary.expr = parse_unary_expr(p);
    return expr;
  }
  if (match(p, TOK_KW_NOT)) {
    rane_expr_t* expr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    expr->kind = EXPR_UNARY;
    expr->unary.op = UN_NOT;
    expr->unary.expr = parse_unary_expr(p);
    return expr;
  }
  return parse_primary_expr(p);
}

static rane_expr_t* parse_primary_expr(rane_parser_t* p) {
  if (match(p, TOK_INT_LITERAL)) {
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    e->kind = EXPR_LIT_INT;
    // Parse value
    char buf[64];
    memcpy(buf, p->previous.start, p->previous.length);
    buf[p->previous.length] = 0;
    e->lit_int.value = atoi(buf);
    e->lit_int.type = RANE_TYPE_U64;
    return e;
  }

  if (match(p, TOK_STRING_LITERAL)) {
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    e->kind = EXPR_LIT_INT;
    // Temporary encoding for strings in v0: store pointer value as immediate.
    // The actual pointer is owned by the driver/exe image; backend will treat it as P64.
    // Copy string contents into a C-heap buffer now for compilation flow.
    size_t n = p->previous.length;
    char* s = (char*)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, p->previous.start, n);
    s[n] = 0;
    e->lit_int.value = (uint64_t)(uintptr_t)s;
    e->lit_int.type = RANE_TYPE_P64;
    return e;
  }

  if (match(p, TOK_IDENTIFIER)) {
    // Support call syntax: ident '(' args ')'
    char name[64] = {0};
    size_t n = p->previous.length;
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    memcpy(name, p->previous.start, n);
    name[n] = 0;

    if (match(p, TOK_KW_LPAREN)) {
      rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      e->kind = EXPR_CALL;
      memcpy(e->call.name, name, sizeof(e->call.name));
      e->call.args = NULL;
      e->call.arg_count = 0;

      // Parse 0 or more args
      if (!check(p, TOK_KW_RPAREN)) {
        // For v0, support single argument
        e->call.args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 4);
        e->call.arg_count = 0;
        e->call.args[e->call.arg_count++] = parse_expr(p);
        while (match(p, TOK_KW_COMMA)) {
          e->call.args[e->call.arg_count++] = parse_expr(p);
          if (e->call.arg_count >= 4) break;
        }
      }

      consume(p, TOK_KW_RPAREN, "Expect ) after call arguments");
      return e;
    }

    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
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

static int is_stmt_terminator(rane_parser_t* p) {
  return check(p, TOK_KW_SEMICOLON) || check(p, TOK_EOF) || check(p, TOK_KW_RBRACE);
}

static void consume_optional_semicolon(rane_parser_t* p) {
  // Semicolons preferred but optional.
  if (check(p, TOK_KW_SEMICOLON)) advance(p);
}

static rane_stmt_t* parse_stmt(rane_parser_t* p);

static rane_stmt_t* parse_block_stmt(rane_parser_t* p) {
  // Assumes '{' already matched
  rane_stmt_t* b = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  b->kind = STMT_BLOCK;
  b->block.stmts = (rane_stmt_t**)malloc(sizeof(rane_stmt_t*) * 64);
  b->block.stmt_count = 0;

  while (!check(p, TOK_EOF) && !check(p, TOK_KW_RBRACE)) {
    rane_stmt_t* s = parse_stmt(p);
    if (!s) break;
    b->block.stmts[b->block.stmt_count++] = s;
    // allow optional ';' between statements
    consume_optional_semicolon(p);
  }

  consume(p, TOK_KW_RBRACE, "Expect } to close block");
  return b;
}

static rane_stmt_t* parse_program(rane_parser_t* p) {
  rane_stmt_t* b = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
  b->kind = STMT_BLOCK;
  b->block.stmts = (rane_stmt_t**)malloc(sizeof(rane_stmt_t*) * 256);
  b->block.stmt_count = 0;

  while (!check(p, TOK_EOF)) {
    rane_stmt_t* s = parse_stmt(p);
    if (!s) break;
    b->block.stmts[b->block.stmt_count++] = s;
    consume_optional_semicolon(p);
  }

  return b;
}

// Parse statement
static rane_stmt_t* parse_stmt(rane_parser_t* p) {
  if (match(p, TOK_KW_LBRACE)) {
    return parse_block_stmt(p);
  }

  if (match(p, TOK_KW_LET)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    s->kind = STMT_LET;
    consume(p, TOK_IDENTIFIER, "Expect identifier after let");
    memcpy(s->let.name, p->previous.start, p->previous.length);
    s->let.name[p->previous.length] = 0;
    s->let.type = RANE_TYPE_U64; // default
    consume(p, TOK_KW_ASSIGN, "Expect = after identifier");
    s->let.expr = parse_expr(p);
    // semicolon optional
    consume_optional_semicolon(p);
    return s;
  } else if (match(p, TOK_IDENTIFIER)) {
    char target[64];
    memcpy(target, p->previous.start, p->previous.length);
    target[p->previous.length] = 0;

    // Call statement: ident '(' ... ')' ';'
    if (check(p, TOK_KW_LPAREN)) {
      // Reconstruct a call expr node using the identifier we consumed
      // Consume '(' and parse args
      advance(p); // consume '('

      rane_expr_t* call = (rane_expr_t*)malloc(sizeof(rane_expr_t));
      call->kind = EXPR_CALL;
      memcpy(call->call.name, target, sizeof(call->call.name));
      call->call.args = NULL;
      call->call.arg_count = 0;

      if (!check(p, TOK_KW_RPAREN)) {
        call->call.args = (rane_expr_t**)malloc(sizeof(rane_expr_t*) * 4);
        call->call.arg_count = 0;
        call->call.args[call->call.arg_count++] = parse_expr(p);
        while (match(p, TOK_KW_COMMA)) {
          call->call.args[call->call.arg_count++] = parse_expr(p);
          if (call->call.arg_count >= 4) break;
        }
      }

      consume(p, TOK_KW_RPAREN, "Expect ) after call arguments");
      consume_optional_semicolon(p);

      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      s->kind = STMT_ASSIGN;
      strcpy_s(s->assign.target, sizeof(s->assign.target), "_");
      s->assign.expr = call;
      return s;
    }

    if (match(p, TOK_KW_ASSIGN)) {
      // assign
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      s->kind = STMT_ASSIGN;
      memcpy(s->assign.target, target, sizeof(target));
      s->assign.expr = parse_expr(p);
      consume(p, TOK_KW_SEMICOLON, "Expect ; after expression");
      return s;
    } else if (match(p, TOK_KW_SEMICOLON)) {
      // marker
      rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
      s->kind = STMT_MARKER;
      memcpy(s->marker.name, target, sizeof(target));
      return s;
    }
  } else if (match(p, TOK_KW_JUMP)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    s->kind = STMT_JUMP;
    consume(p, TOK_IDENTIFIER, "Expect identifier after jump");
    memcpy(s->jump.marker, p->previous.start, p->previous.length);
    s->jump.marker[p->previous.length] = 0;
    consume(p, TOK_KW_SEMICOLON, "Expect ; after marker");
    return s;
  } else if (match(p, TOK_KW_IF)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
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
  } else if (match(p, TOK_KW_WHILE)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    s->kind = STMT_WHILE;
    s->while_stmt.cond = parse_expr(p);
    consume(p, TOK_KW_DO, "Expect do after condition");
    s->while_stmt.body = parse_stmt(p);
    consume_optional_semicolon(p);
    return s;
  }
  return NULL;
}

rane_error_t rane_parse_source(const char* source, rane_stmt_t** out_ast_root) {
  rane_parser_t p;
  parser_init(&p, source, strlen(source));

  *out_ast_root = parse_program(&p);

  if (*out_ast_root && p.current.type == TOK_EOF) {
    return RANE_OK;
  }
  return RANE_E_INVALID_ARG;
}