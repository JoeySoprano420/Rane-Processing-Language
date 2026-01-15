#pragma// ciam_ids.h
// Deterministic ID allocation for CIAM (guards, tracepoints, blocks, anchors)
// As of 01_12_2026
//
// Goal:
//   Same input → same IDs, byte-for-byte, across machines and runs,
//   and resilient to reordering of unrelated code.
//
// Strategy (3 layers):
//   L1) Prefer FRONTEND-STABLE NODE IDS (node_id) + symbol ids (sym_id).
//   L2) If node_id isn’t stable yet, derive a STABLE PATH KEY (lexical path).
//   L3) Final fallback: SPAN HASH + LOCAL CONTEXT HASH (discouraged, but works).
//
// Determinism requirements:
//   - All allocations are derived from a stable key and a stable seed.
//   - No “increment a global counter while walking a vector” unless the walk order
//     is itself canonicalized (sorted by stable key).
//
// IMPORTANT:
//   The only “mutable” thing is a per-function multiset counter for collisions,
//   but it is applied AFTER sorting keys, so it remains deterministic.

#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <span>
#include <vector>
#include <algorithm>
#include <array>

namespace rane::ciam {

    //------------------------------------------------------------------------------
    // Basic fixed hash: FNV-1a 64-bit (fast, stable, no deps)
    //------------------------------------------------------------------------------
    constexpr uint64_t fnv1a64(std::span<const uint8_t> bytes) {
        uint64_t h = 1469598103934665603ull;
        for (auto b : bytes) { h ^= uint64_t(b); h *= 1099511628211ull; }
        return h;
    }
    inline uint64_t fnv1a64_sv(std::string_view s) {
        return fnv1a64(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(s.data()), s.size()));
    }

    //------------------------------------------------------------------------------
    // Canonical source hash (stable_seed)
    //------------------------------------------------------------------------------
    // Build stable_seed from CANONICALIZED SOURCE TEXT (not raw file):
    // - normalize CRLF→LF
    // - strip trailing whitespace
    // - ensure final newline
    // - normalize numeric separators if you want (optional)
    // - keep comments if you want artifacts to change when comments change (usually no)
    // Recommendation for CIAM determinism: hash the canonical surface produced by PASS 0.
    inline uint64_t make_stable_seed_from_canonical_source(std::string_view canonical_utf8) {
        return fnv1a64_sv(canonical_utf8);
    }

    //------------------------------------------------------------------------------
    // Types (compatible with prior header)
    //------------------------------------------------------------------------------
    using node_id = uint32_t;
    using sym_id = uint32_t;
    using guard_id = uint32_t;
    using tp_id = uint32_t;

    struct span { uint32_t line, col, len; };

    // Anchor location in IR before RVAs exist
    struct ir_anchor {
        sym_id fn_sym = 0;
        uint32_t bb = 0;
        uint32_t inst = 0;
    };

    //------------------------------------------------------------------------------
    // Stable Key: a 128-bit-ish key represented as two u64s
    //------------------------------------------------------------------------------
    struct stable_key {
        uint64_t hi = 0;
        uint64_t lo = 0;

        friend bool operator<(stable_key a, stable_key b) {
            return (a.hi < b.hi) || (a.hi == b.hi && a.lo < b.lo);
        }
        friend bool operator==(stable_key a, stable_key b) {
            return a.hi == b.hi && a.lo == b.lo;
        }
    };

    constexpr stable_key mix_key(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
        // simple mixing; not cryptographic, just stable
        stable_key k;
        k.hi = (a * 0x9E3779B185EBCA87ull) ^ (c + 0xD6E8FEB86659FD93ull);
        k.lo = (b * 0xC2B2AE3D27D4EB4Full) ^ (d + 0x165667B19E3779F9ull);
        return k;
    }

    //------------------------------------------------------------------------------
    // Layer 1: Prefer frontend-provided stable NodeKey
    //------------------------------------------------------------------------------
    // Best practice: your parser assigns each node a stable "lexical ordinal"
    // within its parent, and also stores a stable parent chain.
    // That makes node_id stable even if you rebuild vectors differently.
    struct node_stability {
        bool has_stable_node_id = false; // node_id is stable across runs
        bool has_lexical_path = false; // lexical path available (see Layer 2)

        // If lexical path exists:
        // path = [proc_lex, stmt_lex, expr_lex, ...] from proc root to node
        std::span<const uint32_t> lexical_path{};
    };

    //------------------------------------------------------------------------------
    // Layer 2: Stable Path Key (resilient to reordering unrelated code)
    //------------------------------------------------------------------------------
    // Build a key from:
    //   - stable_seed (whole program)
    //   - containing function sym_id
    //   - lexical_path (ordinals from root to node)
    //   - rule_id (so two different rules on same node don’t collide)
    //   - “role tag” (guard kind / trace kind / block kind)
    inline stable_key key_from_lexical_path(
        uint64_t stable_seed,
        sym_id fn,
        std::span<const uint32_t> path,
        uint32_t rule_id,
        uint32_t role_tag)
    {
        uint64_t h1 = stable_seed ^ (uint64_t(fn) << 32) ^ uint64_t(rule_id);
        uint64_t h2 = 0xA5A5A5A5A5A5A5A5ull ^ uint64_t(role_tag);

        // fold path deterministically
        uint64_t hp = 1469598103934665603ull;
        for (uint32_t x : path) {
            uint8_t b[4] = {
              uint8_t(x & 0xFFu),
              uint8_t((x >> 8) & 0xFFu),
              uint8_t((x >> 16) & 0xFFu),
              uint8_t((x >> 24) & 0xFFu)
            };
            hp ^= fnv1a64(std::span<const uint8_t>(b, 4));
            hp *= 1099511628211ull;
        }

        return mix_key(h1, h2, hp, (uint64_t(fn) << 1) ^ stable_seed);
    }

    //------------------------------------------------------------------------------
    // Layer 3: Span Hash fallback (least stable, but deterministic)
    //------------------------------------------------------------------------------
    // Use when node_id and lexical_path aren’t stable yet.
    // Make it less brittle by mixing:
    //   - span (line/col/len)
    //   - containing fn sym
    //   - a tiny neighborhood hash (e.g., previous token kind hash) if available
    inline stable_key key_from_span_fallback(
        uint64_t stable_seed,
        sym_id fn,
        span s,
        uint32_t rule_id,
        uint32_t role_tag,
        uint64_t neighborhood_hint = 0)
    {
        uint64_t a = stable_seed ^ (uint64_t(fn) << 32) ^ uint64_t(rule_id);
        uint64_t b = (uint64_t(s.line) << 32) ^ uint64_t(s.col);
        uint64_t c = (uint64_t(s.len) << 32) ^ uint64_t(role_tag);
        uint64_t d = neighborhood_hint ^ (uint64_t(fn) * 0x9E3779B185EBCA87ull);
        return mix_key(a, b, c, d);
    }

    //------------------------------------------------------------------------------
    // Collision handling: deterministic rank-after-sort
    //------------------------------------------------------------------------------
    // Even with good keys, collisions are possible (esp. span fallback).
    // Resolve by:
    //   1) Collect all candidates (guards/traces/blocks) with their stable_key.
    //   2) Sort by (stable_key, secondary_tiebreak).
    //   3) Assign ids sequentially in that sorted order.
    // This guarantees determinism and reordering resilience.
    //
    // Secondary tiebreak suggestion (must be stable):
    //   - (fn_sym, span.line, span.col, span.len, rule_id, role_tag, node_id)
    //
    // If collisions still persist, the sorted list order remains deterministic.

    struct id_candidate {
        stable_key key{};
        // tiebreak fields (all stable)
        sym_id fn = 0;
        span   where{};
        uint32_t rule_id = 0;
        uint32_t role_tag = 0;
        node_id nid = 0;

        // output
        uint32_t assigned = 0;
    };

    inline void assign_ids_sorted(std::vector<id_candidate>& items, uint32_t start_at = 1) {
        std::sort(items.begin(), items.end(),
            [](id_candidate const& A, id_candidate const& B) {
                if (A.key < B.key) return true;
                if (B.key < A.key) return false;

                // deterministic tiebreak chain
                if (A.fn != B.fn) return A.fn < B.fn;
                if (A.where.line != B.where.line) return A.where.line < B.where.line;
                if (A.where.col != B.where.col)  return A.where.col < B.where.col;
                if (A.where.len != B.where.len)  return A.where.len < B.where.len;
                if (A.rule_id != B.rule_id)    return A.rule_id < B.rule_id;
                if (A.role_tag != B.role_tag)   return A.role_tag < B.role_tag;
                return A.nid < B.nid;
            });

        uint32_t next = start_at;
        for (auto& it : items) it.assigned = next++;
    }

    //------------------------------------------------------------------------------
    // Recommended “role tags” (freeze these numbers)
    //------------------------------------------------------------------------------
    enum : uint32_t {
        ROLE_GUARD = 0x47415244u, // 'GARD'
        ROLE_TRACE = 0x54524143u, // 'TRAC'
        ROLE_BLOCK = 0x424C4B21u, // 'BLK!'
    };

    // Guard/Trace sub-tags (combine with kind)
    inline uint32_t role_tag_guard(uint16_t guard_kind) {
        return ROLE_GUARD ^ (uint32_t(guard_kind) << 16);
    }
    inline uint32_t role_tag_trace(uint16_t trace_kind) {
        return ROLE_TRACE ^ (uint32_t(trace_kind) << 16);
    }
    inline uint32_t role_tag_block(uint16_t block_kind) {
        return ROLE_BLOCK ^ (uint32_t(block_kind) << 16);
    }

    //------------------------------------------------------------------------------
    // Block IDs: make them stable even if CFG construction order changes
    //------------------------------------------------------------------------------
    // Rule:
    //   - Each block gets a stable key based on:
    //       (fn_sym, block_entry_lexical_path OR first-instruction anchor/span)
    //   - Then blocks are sorted by that key and assigned bb ids 0..N-1.
    //
    // This ensures:
    //   - reordering unrelated blocks doesn’t change ids
    //   - adding a new block changes ids only where it should (after insertion in key order)

    struct block_candidate {
        stable_key key{};
        sym_id fn = 0;
        span  entry_span{};
        uint32_t assigned_bb = 0;
    };

    inline void assign_block_ids_sorted(std::vector<block_candidate>& blocks) {
        std::sort(blocks.begin(), blocks.end(),
            [](auto const& A, auto const& B) {
                if (A.key < B.key) return true;
                if (B.key < A.key) return false;
                if (A.fn != B.fn) return A.fn < B.fn;
                if (A.entry_span.line != B.entry_span.line) return A.entry_span.line < B.entry_span.line;
                if (A.entry_span.col != B.entry_span.col)  return A.entry_span.col < B.entry_span.col;
                return A.entry_span.len < B.entry_span.len;
            });
        uint32_t bb = 0;
        for (auto& b : blocks) b.assigned_bb = bb++;
    }

    //------------------------------------------------------------------------------
    // Practical recipe (use this verbatim in CIAM)
    //------------------------------------------------------------------------------
    //
    // 1) PASS 0 outputs canonical surface text.
    //    stable_seed = hash(canonical_surface_text)
    //
    // 2) During desugar/enforce passes, DO NOT assign guard_id immediately.
    //    Instead, record candidates:
    //      - fn_sym
    //      - node stability info (node_id and/or lexical_path)
    //      - span
    //      - rule_id
    //      - kind (guard_kind/trace_kind)
    //    Build stable_key using L1/L2/L3 priority.
    //
    // 3) After the pass finishes (or per function), assign ids by sorting candidates:
    //      assign_ids_sorted(candidates, start_at=1)
    //
    // 4) When building IR, emit guard_begin/guard_end with the assigned guard_id
    //    and record anchors (fn_sym, bb_id, inst_index).
    //
    // 5) If anchors aren’t known until later, store guard_id now and patch anchor later.
    //    (IDs must never change after assignment.)
    //
    // 6) For blocks, do the same: collect block candidates, sort, assign bb ids,
    //    then patch IR block ids if needed.
    //
    // This makes the pipeline stable even if:
    //   - vectors are iterated in different orders
    //   - unrelated code is moved elsewhere
    //   - additional nodes are inserted (only local key order shifts)
    //
    //------------------------------------------------------------------------------

    // Convenience: build candidate key using the best available stability layer.
    inline stable_key make_best_key_for_node(
        uint64_t stable_seed,
        sym_id fn,
        uint32_t rule_id,
        uint32_t role_tag,
        node_id nid,
        node_stability st,
        span where,
        uint64_t neighborhood_hint = 0)
    {
        if (st.has_lexical_path && st.lexical_path.size() != 0) {
            return key_from_lexical_path(stable_seed, fn, st.lexical_path, rule_id, role_tag);
        }
        // If you truly have a stable node_id, you can mix that too, but lexical path is better.
        if (st.has_stable_node_id && nid != 0) {
            // Treat nid as a tiny “path”
            uint32_t nid_arr[1] = { nid };
            return key_from_lexical_path(stable_seed, fn, std::span<const uint32_t>(nid_arr, 1), rule_id, role_tag);
        }
        return key_from_span_fallback(stable_seed, fn, where, rule_id, role_tag, neighborhood_hint);
    }

} // namespace rane::ciam once
