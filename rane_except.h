#pragma once

// Basic exception handling for RANE

typedef struct rane_exception_s {
  const char* message;
  int code;
} rane_exception_t;

void rane_throw(const char* msg, int code);
rane_exception_t* rane_catch();
void rane_try(void (*fn)());