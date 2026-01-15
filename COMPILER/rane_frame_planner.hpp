// rane_frame_planner.hpp
// Micro “frame planner” for RSP-only backend (Windows x64 ABI)
// - Produces rsp-relative stack-slot strings: "qword [rsp+32]" etc.
// - Computes call-site {shadow_and_align} for Windows x64:
//   - Always reserves 32-byte shadow space at a call site
//   - Ensures RSP is 16-byte aligned at the CALL instruction
//
// Assumptions / contract:
// - You track a function-local stack layout in bytes (locals/temps/spills).
// - You decide whether to reserve the function’s stack frame once in the prolog (recommended).
// - For calls, you may need extra temporary "outgoing arg stack" beyond the home/shadow.
//
// Two ways to use:
//  A) Prolog reserves frame_size_aligned (locals+spills+outgoing max), then calls only need
//     shadow_and_align = align_fix_for_current_rsp + 0x20
//  B) No pre-reserved outgoing area: each call reserves shadow + args + alignment fix.
//
// This helper supports both by letting you pass "current_rsp_mod16".

#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace rane::winx64 {

// ------------------------------------------------------------
// 0) Windows x64 ABI constants
// ------------------------------------------------------------
static constexpr uint32_t kShadowSpaceBytes = 32; // 0x20 mandatory at every call site

// ------------------------------------------------------------
// 1) Tiny formatter for [rsp+off] operands
// ------------------------------------------------------------
enum class MemWidth : uint8_t { B8, B16, B32, B64 };

inline std::string width_prefix(MemWidth w) {
  switch (w) {
    case MemWidth::B8:  return "byte ";
    case MemWidth::B16: return "word ";
    case MemWidth::B32: return "dword ";
    case MemWidth::B64: return "qword ";
  }
  return "qword ";
}

// Returns: "qword [rsp+32]" / "dword [rsp+96]" / "byte [rsp+8]" etc.
inline std::string rsp_slot(MemWidth w, uint32_t off_bytes) {
  std::string s = width_prefix(w);
  s += "[rsp";
  if (off_bytes == 0) {
    s += "]";
    return s;
  }
  s += "+";
  s += std::to_string(off_bytes);
  s += "]";
  return s;
}

// Same, but without width prefix: "[rsp+32]"
inline std::string rsp_addr(uint32_t off_bytes) {
  std::string s = "[rsp";
  if (off_bytes == 0) { s += "]"; return s; }
  s += "+";
  s += std::to_string(off_bytes);
  s += "]";
  return s;
}

// ------------------------------------------------------------
// 2) Frame planner (RSP-only) with named regions
// ------------------------------------------------------------
struct FramePlan {
  // Layout is RSP-relative AFTER your function prolog has executed.
  //
  // If you do: sub rsp, frame_size_aligned
  // then inside the function body:
  //   [rsp + locals_base + i] addresses locals/spills/outgoing as you define.
  //
  // Regions (you can keep it ultra simple):
  //   locals_base      = 0
  //   outgoing_base    = locals_bytes_aligned   (optional)
  //   total_frame      = locals + outgoing_max  (aligned to 16)
  //
  uint32_t locals_bytes = 0;         // space for locals/spills/temps
  uint32_t outgoing_max_bytes = 0;   // max stack args you might need (above shadow), optional
  uint32_t total_frame_aligned = 0;  // aligned to 16

  // Offsets (from current RSP after prolog)
  uint32_t locals_base = 0;
  uint32_t outgoing_base = 0;

  static constexpr uint32_t align_up(uint32_t x, uint32_t a) {
    return (x + (a - 1)) & ~(a - 1);
  }

  // Build a plan. Outgoing area is optional but recommended if you want constant call sequences.
  // - locals_bytes: spill slots etc.
  // - outgoing_max_bytes: maximum stack-args area you will need for ANY call in this function.
  //   (this does NOT include the 32-byte shadow; shadow is handled at each call site)
  void build(uint32_t locals, uint32_t outgoing_max = 0) {
    locals_bytes = align_up(locals, 16); // keep locals nicely aligned
    outgoing_max_bytes = align_up(outgoing_max, 16);

    locals_base = 0;
    outgoing_base = locals_bytes;

    total_frame_aligned = align_up(locals_bytes + outgoing_max_bytes, 16);
  }

  // Convenience: get a local slot operand at local_off within locals region
  // Example: local_qword(32) -> "qword [rsp+32]"
  std::string local(MemWidth w, uint32_t local_off) const {
    return rsp_slot(w, locals_base + local_off);
  }

  // Convenience: get outgoing arg slot at out_off within outgoing region
  // Example: out_qword(0) -> first stack arg slot space (if you use outgoing region)
  std::string outgoing(MemWidth w, uint32_t out_off) const {
    return rsp_slot(w, outgoing_base + out_off);
  }
};

// ------------------------------------------------------------
// 3) Call-site planner: shadow_and_align calculator
// ------------------------------------------------------------
//
// Goal: produce "shadow_and_align" for patterns like:
//   sub rsp, {shadow_and_align}
//   call target
//   add rsp, {shadow_and_align}
//
// Requirement: RSP must be 16-byte aligned *at the CALL instruction*.
//
// Let current RSP before "sub" have mod16 = m (0..15).
// After subtracting N bytes, new mod16 = (m - (N mod16)) mod16.
// We want new mod16 == 0.
//
// Windows requires at least 32 bytes shadow at every call.
// So N = 32 + stack_args_bytes + align_fix
//
// Choose align_fix in {0,8} to satisfy alignment (since 32 is 0 mod16).
// If stack_args_bytes is multiple of 16, then N mod16 = align_fix mod16.
// If stack_args_bytes is 8 mod16, then N mod16 = (8 + align_fix) mod16.
//
// This helper computes minimal align_fix (0 or 8) given:
// - current_rsp_mod16: RSP % 16 just before the call sequence
// - stack_args_bytes: bytes of stack-passed args for this call (NOT including shadow)
//   (must already be rounded up to 8-byte slots; we’ll internally align to 8)
//
// IMPORTANT: If you always ensure your function body maintains a known mod16 (common),
// you can compute current_rsp_mod16 once and reuse.
//
// If you use an RSP-only prolog: sub rsp, frame_size_aligned
// and frame_size_aligned is 16-aligned, then current_rsp_mod16 is stable
// across the function (ignoring push/pop etc. you may emit).
//
struct CallSite {
  uint32_t shadow_and_align = 0; // bytes to sub/add around call
  uint32_t align_fix = 0;        // 0 or 8
  uint32_t stack_args_rounded = 0;

  static constexpr uint32_t align_up(uint32_t x, uint32_t a) {
    return (x + (a - 1)) & ~(a - 1);
  }

  // current_rsp_mod16: 0 or 8 in most sane codegens, but we accept 0..15
  // stack_args_bytes: stack bytes for args beyond RCX,RDX,R8,R9 (before call)
  // Returns: shadow_and_align (>= 32) with correct alignment.
  static CallSite plan(uint32_t current_rsp_mod16, uint32_t stack_args_bytes) {
    CallSite cs{};
    // Windows stack args are 8-byte slots; keep them 8-aligned at least
    cs.stack_args_rounded = align_up(stack_args_bytes, 8);

    // Base without fix
    uint32_t base = kShadowSpaceBytes + cs.stack_args_rounded;

    // We want (current_rsp_mod16 - (base + fix) mod16) mod16 == 0
    // => (base + fix) mod16 == current_rsp_mod16
    // base_mod = base % 16
    uint32_t base_mod = base & 15u;

    // Try fix = 0 or 8 (keeps alignment without wasting 16)
    uint32_t fix0 = 0;
    uint32_t fix8 = 8;

    auto ok = [&](uint32_t fix) -> bool {
      return ((base_mod + (fix & 15u)) & 15u) == (current_rsp_mod16 & 15u);
    };

    if (ok(fix0)) cs.align_fix = 0;
    else if (ok(fix8)) cs.align_fix = 8;
    else {
      // Fallback: choose whatever makes it work (0..15), but you should never hit this
      // if current_rsp_mod16 is 0/8 and stack_args_rounded is multiple of 8.
      uint32_t need = ( (current_rsp_mod16 & 15u) + 16u - base_mod ) & 15u;
      cs.align_fix = need;
    }

    cs.shadow_and_align = base + cs.align_fix;
    return cs;
  }
};

// ------------------------------------------------------------
// 4) Practical helpers for typical RSP-only backend
// ------------------------------------------------------------
//
// If you use: sub rsp, frame_size_aligned  (where frame_size_aligned is 16-aligned)
// then inside the function body, your "current_rsp_mod16" is stable.
//
// Usually, on Windows x64, entry RSP is 8 mod16 (because CALL pushed return address).
// If you sub a 16-aligned frame_size, mod16 stays 8.
// That means: current_rsp_mod16 == 8 in most functions (rsp-only, no pushes).
//
// You can use this constant if your codegen avoids extra pushes/pops.
static constexpr uint32_t kTypicalRspMod16_AfterRspOnlyProlog = 8;

// Compute shadow_and_align assuming the typical stable mod16=8.
inline uint32_t shadow_and_align_typical(uint32_t stack_args_bytes) {
  return CallSite::plan(kTypicalRspMod16_AfterRspOnlyProlog, stack_args_bytes).shadow_and_align;
}

} // namespace rane::winx64

// ------------------------------------------------------------
// OPTIONAL MINI DEMO (define RANE_FRAME_PLANNER_DEMO)
// ------------------------------------------------------------
#ifdef RANE_FRAME_PLANNER_DEMO
#include <iostream>
int main() {
  using namespace rane::winx64;

  FramePlan fp;
  fp.build(/*locals=*/96, /*outgoing_max=*/32); // 96 bytes locals, 32 bytes outgoing args max
  std::cout << "total_frame_aligned = " << fp.total_frame_aligned << "\n";
  std::cout << "local qword @32 = " << fp.local(MemWidth::B64, 32) << "\n";
  std::cout << "outgoing qword @0 = " << fp.outgoing(MemWidth::B64, 0) << "\n";

  // Call needing 24 bytes stack args (e.g., 3 args beyond RCX,RDX,R8,R9)
  auto cs = CallSite::plan(kTypicalRspMod16_AfterRspOnlyProlog, 24);
  std::cout << "shadow_and_align = " << cs.shadow_and_align
            << " (shadow 32 + args " << cs.stack_args_rounded
            << " + fix " << cs.align_fix << ")\n";
  return 0;
}
#endif
