#pragma once

#include "rane_loader.h"
#include "rane_tir.h"

// Register allocation for TIR

rane_error_t rane_allocate_registers(rane_tir_module_t* tir_module);