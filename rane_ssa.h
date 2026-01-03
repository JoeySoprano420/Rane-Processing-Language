#pragma once

#include "rane_loader.h"
#include "rane_tir.h"

// SSA construction for TIR

rane_error_t rane_build_ssa(rane_tir_module_t* tir_module);