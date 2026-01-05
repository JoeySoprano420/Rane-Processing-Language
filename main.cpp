#include "rane_driver.h"

#include <stdio.h>
#include <string.h>

#include "rane_loader.h"
#include "rane_parser.h"
#include "rane_typecheck.h"
#include "rane_tir.h"
#include "rane_ssa.h"
#include "rane_regalloc.h"
#include "rane_optimize.h"
#include "rane_stdlib.h"
#include "rane_hashmap.h"
#include "rane_bst.h"
#include "rane_gc.h"
#include "rane_except.h"
#include "rane_aot.h"
#include "rane_vm.h"
#include "rane_graph.h"
#include "rane_heap.h"
#include "rane_net.h"
#include "rane_crypto.h"
#include "rane_file.h"
#include "rane_thread.h"
#include "rane_security.h"
#include "rane_perf.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc >= 2) {
        const char* in = argv[1];
        const char* out = "a.exe";
        int opt = 2;
        int emit_c = 0;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                out = argv[++i];
            } else if (strcmp(argv[i], "-O0") == 0) opt = 0;
            else if (strcmp(argv[i], "-O1") == 0) opt = 1;
            else if (strcmp(argv[i], "-O2") == 0) opt = 2;
            else if (strcmp(argv[i], "-O3") == 0) opt = 3;
            else if (strcmp(argv[i], "-emit-c") == 0) emit_c = 1;
        }

        rane_driver_options_t opts;
        opts.input_path = in;
        opts.output_path = out;
        opts.opt_level = opt;

        rane_error_t err = emit_c ? rane_compile_file_to_c(&opts)
                                  : rane_compile_file_to_exe(&opts);
        if (err != RANE_OK) {
            fprintf(stderr, "rane: compile failed (%d)\n", (int)err);
            return 1;
        }

        printf("rane: wrote %s\n", out);
        return 0;
    }

    // Validate core subsystems in the demo path.
    rane_gc_run_selftest();

    // Fallback: keep existing in-process demo path
    rane_layout_spec_t layout = {};
    layout.abi_version = RANE_LOADER_ABI_VERSION;

    // Provide a complete fixed layout so strict band reservation succeeds.
    layout.core_band_base = 0x0000000100000000ULL;
    layout.core_band_size = 0x0000000010000000ULL; // 256MB

    layout.aot_band_base = 0x0000000200000000ULL;
    layout.aot_slot_size = 0x0000000001000000ULL; // 16MB
    layout.aot_slots = 32;                         // 512MB AOT band

    layout.jit_band_base = 0x0000000600000000ULL;
    layout.jit_band_size = 0x0000000010000000ULL; // 256MB
    layout.jit_tier1_off = 0x0;
    layout.jit_tier2_off = 0x04000000ULL;   // 64MB
    layout.jit_stubs_off = 0x08000000ULL;   // 128MB

    layout.meta_band_base = 0x0000000700000000ULL;
    layout.meta_band_size = 0x0000000010000000ULL; // 256MB

    layout.heap_band_base = 0x0000000800000000ULL;
    layout.heap_band_size = 0x0000000040000000ULL; // 1GB

    layout.mmap_band_base = 0x0000000C00000000ULL;
    layout.mmap_band_size = 0x0000000040000000ULL; // 1GB

    rane_policy_t policy = {};
    policy.abi_version = RANE_LOADER_ABI_VERSION;
    policy.flags = RANE_POLICY_DENY_RWX_ALWAYS | RANE_POLICY_ENFORCE_EXEC_BANDS;

    rane_loader_state_t* st = nullptr;
    rane_error_t err = rane_loader_init(&layout, &policy, &st);
    if (err != RANE_OK) {
        std::cout << "Init failed: " << err << std::endl;
        return 1;
    }

    std::cout << "Loader initialized successfully." << std::endl;

    // Example RANE source (stub)
    const char* source = "let x = 42;";

    // Parse
    rane_stmt_t* ast = nullptr;
    err = rane_parse_source(source, &ast);
    if (err != RANE_OK) {
        std::cout << "Parse failed" << std::endl;
        return 1;
    }

    // Type check
    err = rane_typecheck_ast(ast);
    if (err != RANE_OK) {
        std::cout << "Type check failed" << std::endl;
        return 1;
    }

    // Lower to TIR
    rane_tir_module_t tir_mod = {};
    err = rane_lower_ast_to_tir(ast, &tir_mod);
    if (err != RANE_OK) {
        std::cout << "Lowering failed" << std::endl;
        return 1;
    }

    // SSA
    err = rane_build_ssa(&tir_mod);
    if (err != RANE_OK) {
        std::cout << "SSA failed" << std::endl;
        return 1;
    }

    // Reg alloc
    err = rane_allocate_registers(&tir_mod);
    if (err != RANE_OK) {
        std::cout << "Reg alloc failed" << std::endl;
        return 1;
    }

    // Optimize
    err = rane_optimize_tir(&tir_mod);
    if (err != RANE_OK) {
        std::cout << "Optimization failed" << std::endl;
        return 1;
    }

    // Codegen
    uint8_t code_buf[1024];
    rane_codegen_ctx_t ctx = { code_buf, sizeof(code_buf), 0 };
    err = rane_x64_codegen_tir_to_machine(&tir_mod, &ctx);
    if (err != RANE_OK) {
        std::cout << "Codegen failed" << std::endl;
        return 1;
    }

    std::cout << "Compiled to " << ctx.code_size << " bytes." << std::endl;

    // AOT Compile
    void* aot_code;
    size_t aot_size;
    err = rane_aot_compile(&tir_mod, &aot_code, &aot_size);
    if (err != RANE_OK) {
        std::cout << "AOT compile failed" << std::endl;
        return 1;
    }

    return 0;
}
