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

// Parse expression (simple: int literal or identifier)
static rane_expr_t* parse_expr(rane_parser_t* p) {
  return parse_primary_expr(p);
}

static rane_expr_t* parse_unary_expr(rane_parser_t* p) {
  if (match(p, TOK_KW_MINUS)) {
    rane_expr_t* expr = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    expr->kind = EXPR_UNARY;
    expr->unary.op = UN_NEG;
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
  if (match(p, TOK_IDENTIFIER)) {
    rane_expr_t* e = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    e->kind = EXPR_VAR;
    memcpy(e->var.name, p->previous.start, p->previous.length);
    e->var.name[p->previous.length] = 0;
    return e;
  }
  if (match(p, TOK_KW_LPAREN)) {
    rane_expr_t* e = parse_expr(p);
    consume(p, TOK_KW_RPAREN, "Expect ) after expression");
    return e;
  }
  return NULL; // error
}

// Parse statement: let ident = expr ; or ident = expr ; or jump ident ; or mark ident ; or if expr then stmt else stmt ; or while expr do stmt ;
static rane_stmt_t* parse_stmt(rane_parser_t* p) {
  if (match(p, TOK_KW_LET)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    s->kind = STMT_LET;
    consume(p, TOK_IDENTIFIER, "Expect identifier after let");
    memcpy(s->let.name, p->previous.start, p->previous.length);
    s->let.name[p->previous.length] = 0;
    s->let.type = RANE_TYPE_U64; // default
    consume(p, TOK_KW_ASSIGN, "Expect = after identifier");
    s->let.expr = parse_expr(p);
    consume(p, TOK_KW_SEMICOLON, "Expect ; after expression");
    return s;
  } else if (match(p, TOK_IDENTIFIER)) {
    // Could be assign or jump or mark
    char target[64];
    memcpy(target, p->previous.start, p->previous.length);
    target[p->previous.length] = 0;
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
    return s;
  } else if (match(p, TOK_KW_WHILE)) {
    rane_stmt_t* s = (rane_stmt_t*)malloc(sizeof(rane_stmt_t));
    s->kind = STMT_WHILE;
    s->while_stmt.cond = parse_expr(p);
    consume(p, TOK_KW_DO, "Expect do after condition");
    s->while_stmt.body = parse_stmt(p);
    return s;
  }
  return NULL;
}

rane_error_t rane_parse_source(const char* source, rane_stmt_t** out_ast_root) {
  rane_parser_t p;
  parser_init(&p, source, strlen(source));

  *out_ast_root = parse_stmt(&p);

  if (*out_ast_root && p.current.type == TOK_EOF) {
    return RANE_OK;
  }
  return RANE_E_INVALID_ARG; // parse error
}