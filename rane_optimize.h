#pragma once

#include "rane_tir.h"

// Comprehensive Compiler Optimizations for RANE
// Including IR-level and machine-level passes

// IR-Level Optimizations
rane_error_t rane_opt_constant_folding(rane_tir_module_t* mod);
rane_error_t rane_opt_dead_code_elimination(rane_tir_module_t* mod);
rane_error_t rane_opt_profile_guided_optimization(rane_tir_module_t* mod); // PGO stub
rane_error_t rane_opt_inlining(rane_tir_module_t* mod);
rane_error_t rane_opt_constexpr_evaluation(rane_tir_module_t* mod); // constexpr stub
rane_error_t rane_opt_template_pruning(rane_tir_module_t* mod); // stub for template-like
rane_error_t rane_opt_type_based_alias_analysis(rane_tir_module_t* mod); // TBAA
rane_error_t rane_opt_tail_calls(rane_tir_module_t* mod);
rane_error_t rane_opt_code_compression(rane_tir_module_t* mod); // stub
rane_error_t rane_opt_exception_simplifications(rane_tir_module_t* mod);
rane_error_t rane_opt_alias_analysis(rane_tir_module_t* mod);
rane_error_t rane_opt_call_graph_construction(rane_tir_module_t* mod);
rane_error_t rane_opt_dependence_analysis(rane_tir_module_t* mod);
rane_error_t rane_opt_dominator_tree(rane_tir_module_t* mod);
rane_error_t rane_opt_post_dominator_tree(rane_tir_module_t* mod);
rane_error_t rane_opt_lazy_value_info(rane_tir_module_t* mod);
rane_error_t rane_opt_constant_range_analysis(rane_tir_module_t* mod);
rane_error_t rane_opt_instcombine(rane_tir_module_t* mod);
rane_error_t rane_opt_sccp(rane_tir_module_t* mod);
rane_error_t rane_opt_correlated_value_propagation(rane_tir_module_t* mod);
rane_error_t rane_opt_aggressive_dce(rane_tir_module_t* mod);
rane_error_t rane_opt_reassociate(rane_tir_module_t* mod);
rane_error_t rane_opt_gvn(rane_tir_module_t* mod);
rane_error_t rane_opt_memcpyopt(rane_tir_module_t* mod);
rane_error_t rane_opt_sroa(rane_tir_module_t* mod);
rane_error_t rane_opt_licm(rane_tir_module_t* mod);
rane_error_t rane_opt_loop_rotation(rane_tir_module_t* mod);
rane_error_t rane_opt_loop_unrolling(rane_tir_module_t* mod);
rane_error_t rane_opt_loop_unswitching(rane_tir_module_t* mod);
rane_error_t rane_opt_induction_var_simplification(rane_tir_module_t* mod);
rane_error_t rane_opt_tail_call_elimination(rane_tir_module_t* mod);
rane_error_t rane_opt_jump_threading(rane_tir_module_t* mod);
rane_error_t rane_opt_simplifycfg(rane_tir_module_t* mod);
rane_error_t rane_opt_function_merging(rane_tir_module_t* mod);
rane_error_t rane_opt_global_dce(rane_tir_module_t* mod);
rane_error_t rane_opt_argument_promotion(rane_tir_module_t* mod);
rane_error_t rane_opt_ip_constant_propagation(rane_tir_module_t* mod);
rane_error_t rane_opt_dead_argument_elimination(rane_tir_module_t* mod);
rane_error_t rane_opt_whole_program_devirtualization(rane_tir_module_t* mod);
rane_error_t rane_opt_attributor(rane_tir_module_t* mod);
rane_error_t rane_opt_loop_vectorizer(rane_tir_module_t* mod);
rane_error_t rane_opt_slp_vectorizer(rane_tir_module_t* mod);
rane_error_t rane_opt_interleaved_access_vectorization(rane_tir_module_t* mod);
rane_error_t rane_opt_advanced_aa(rane_tir_module_t* mod);
rane_error_t rane_opt_scoped_aa(rane_tir_module_t* mod);
rane_error_t rane_opt_memoryssa_optimizations(rane_tir_module_t* mod);
rane_error_t rane_opt_load_store_forwarding(rane_tir_module_t* mod);
rane_error_t rane_opt_mem2reg(rane_tir_module_t* mod);

// Machine-Level Optimizations
rane_error_t rane_opt_instruction_scheduling(rane_tir_module_t* mod);
rane_error_t rane_opt_register_allocation(rane_tir_module_t* mod); // already have
rane_error_t rane_opt_peephole_optimizations(rane_tir_module_t* mod);
rane_error_t rane_opt_branch_relaxation(rane_tir_module_t* mod);
rane_error_t rane_opt_machine_cse(rane_tir_module_t* mod);
rane_error_t rane_opt_machine_licm(rane_tir_module_t* mod);
rane_error_t rane_opt_post_ra_scheduling(rane_tir_module_t* mod);
rane_error_t rane_opt_tail_duplication(rane_tir_module_t* mod);

// Optimization Levels
rane_error_t rane_opt_level_O0(rane_tir_module_t* mod); // No optimization
rane_error_t rane_opt_level_O1(rane_tir_module_t* mod); // Basic optimizations
rane_error_t rane_opt_level_O2(rane_tir_module_t* mod); // Most optimizations
rane_error_t rane_opt_level_O3(rane_tir_module_t* mod); // Aggressive optimizations
rane_error_t rane_opt_level_Ofast(rane_tir_module_t* mod); // Breaks standards for speed
rane_error_t rane_opt_level_Os(rane_tir_module_t* mod); // Optimize for size
rane_error_t rane_opt_level_Oz(rane_tir_module_t* mod); // Minimum size
rane_error_t rane_opt_lto(rane_tir_module_t* mod); // Link-time optimization
rane_error_t rane_opt_march_native(rane_tir_module_t* mod); // Use all CPU features

// Main optimization driver
rane_error_t rane_optimize_tir(rane_tir_module_t* tir_module);