#include "rane_except.h"
#include <stdlib.h>
#include <setjmp.h>

static jmp_buf exception_buf;
static rane_exception_t* current_exception = NULL;

void rane_throw(const char* msg, int code) {
  if (current_exception) free(current_exception);
  current_exception = (rane_exception_t*)malloc(sizeof(rane_exception_t));
  current_exception->message = msg;
  current_exception->code = code;
  longjmp(exception_buf, 1);
}

rane_exception_t* rane_catch() {
  return current_exception;
}

void rane_try(void (*fn)()) {
  if (setjmp(exception_buf) == 0) {
    fn();
  }
  // Exception caught
}