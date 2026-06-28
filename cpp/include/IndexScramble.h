#ifndef AUDIOBABEL_INDEX_SCRAMBLE_H
#define AUDIOBABEL_INDEX_SCRAMBLE_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>

/**
 * @file IndexScramble.h
 * @brief Optional, reversible placement of indices that also diversifies the
 *        decoded audio LENGTH of short indices.
 *
 * By default the index of a payload sits numerically next to the indices of
 * very similar payloads, so neighbouring "library" positions hold near-identical
 * audio, and — because the index uses bijective base-65536 numeration — any
 * *small* index decodes to only one or two samples of near-silence. Browsing the
 * early library positions therefore yields a stream of indistinguishable,
 * sub-millisecond clips even though the index strings look different.
 *
 * This module applies a keyed, reversible permutation that fixes both problems
 * while keeping the index<->payload mapping a perfect bijection:
 *   1. a per-band content scramble so neighbouring payloads no longer share
 *      almost all of their samples, and
 *   2. a length-spreading swap so that *short* indices decode to a wide, varied
 *      range of durations instead of all collapsing to near-silence (or, as the
 *      previous tier design did, all collapsing to a single ~1-second length).
 *
 * @section algorithm Algorithm
 *
 * An L-sample payload maps to an integer in the length-"band"
 * [S_L, S_{L+1}) where S_L = (B^L - 1)/(B - 1) and B = 65536. The band index L
 * is exactly the decoded sample count, so changing an index's band changes its
 * audio length.
 *
 * scramble() = lengthSpread( contentScramble( index ) ):
 *
 *  - contentScramble() applies a keyed XOR stream *within the index's own
 *    band* (length-preserving). Each band has exactly B^L = 2^(16L) elements,
 *    so XOR with a pseudorandom mask is a valid bijection. The stream is keyed
 *    on (seed, L) via splitmix64. XOR is self-inverse, so scramble and
 *    unscramble call the same function. This is applied to every index.
 *
 *  - lengthSpread() is a keyed involution that swaps the block of "short"
 *    indices [1, 2^P) (the PREFIX, all of band <= P/16) with a set of TARGET
 *    slots that are spread across T distinct, well-separated bands ranging from
 *    a short minimum (default 100 ms) up to a long maximum (default 15 s). A
 *    short index i is decomposed as i-1 = o*T + j: the sub-band index j selects
 *    one of the T target lengths (after a bit-reversal so that numerically
 *    adjacent indices land on *far-apart* durations, not neighbouring ones), and
 *    o is run through a keyed Feistel over [0, 2^(P-log2 T)) to choose the
 *    sample values within that band. Because the map is a fixed pairing between
 *    two equal-size disjoint sets, it is its own inverse and trivially a
 *    bijection; the displaced TARGET indices map back down into the short block.
 *    Indices that are neither in the PREFIX nor a TARGET slot pass through
 *    unchanged (their length is already non-trivial).
 *
 * Net effect: browsing consecutive library positions now yields clips whose
 * durations jump across the whole 100 ms .. 15 s range and whose contents are
 * unrelated, while every index still decodes to exactly one payload and back.
 *
 * @section feistel Why XOR stream / Why Feistel for lengthSpread
 * contentScramble() uses an XOR stream: each band has exactly 2^(16L) elements,
 * so XOR with a pseudorandom mask is a length-preserving bijection and is its
 * own inverse — no modular inverse or round reversal needed.
 * lengthSpread() still uses a Feistel network internally for the per-target
 * value permutation: a Feistel is a bijection for ANY round function and is
 * inverted by running rounds in reverse. The length-spread swap itself is an
 * involution, so it needs no inverse at all.
 *
 * @section references References
 * - Feistel networks: https://en.wikipedia.org/wiki/Feistel_cipher
 * - Bijective numeration: https://en.wikipedia.org/wiki/Bijective_numeration
 *
 * @section toggle Toggling
 * The pure scramble()/unscramble() functions are always available. Whether the
 * core encode/decode actually applies them is controlled by config() (a runtime
 * switch used for testing) whose default comes from the compile-time flag
 * AUDIOBABEL_SCRAMBLE. Index::encode/decode check config().enabled directly
 * before calling scramble()/unscramble(). Define AUDIOBABEL_SCRAMBLE
 * (optionally with AUDIOBABEL_SCRAMBLE_SEED) to bake the toggle in for a final
 * build.
 */
namespace AudioBabel::IndexScramble {

using boost::multiprecision::cpp_int;

// --- Compile-time configuration ---------------------------------------------
#ifndef AUDIOBABEL_SCRAMBLE_SEED
#    define AUDIOBABEL_SCRAMBLE_SEED 0x9E3779B97F4A7C15ULL
#endif

/// Shortest and longest decoded duration (milliseconds, at DEFAULT_SAMPLE_RATE)
/// that the length-spread maps short indices onto. Every "short" index lands on
/// one of AUDIOBABEL_SCRAMBLE_LENGTH_COUNT distinct, log-spaced lengths between
/// these two bounds, so the pair sets the variety of browse/short-index audio.
/// Indices outside the PREFIX range (large hand-typed or search-result indices)
/// are unaffected and decode to whatever length the bijection gives them.
/// MIN must be > 0 and < MAX (enforced by static_assert in IndexScramble.cpp),
/// and MIN's sample count must exceed the diversified prefix's band range so the
/// PREFIX and TARGET sets stay disjoint (also static_asserted).
#ifndef AUDIOBABEL_SCRAMBLE_MIN_MS
#    define AUDIOBABEL_SCRAMBLE_MIN_MS 100
#endif
#ifndef AUDIOBABEL_SCRAMBLE_MAX_MS
#    define AUDIOBABEL_SCRAMBLE_MAX_MS 5000
#endif

/// log2 of the number of distinct target lengths a short index can land on.
/// Must be < AUDIOBABEL_SCRAMBLE_PREFIX_BITS. Default 8 => 256 distinct lengths
/// spread log-uniformly across [MIN_MS, MAX_MS].
#ifndef AUDIOBABEL_SCRAMBLE_LENGTH_COUNT_LOG2
#    define AUDIOBABEL_SCRAMBLE_LENGTH_COUNT_LOG2 8
#endif

/// Indices in [1, 2^PREFIX_BITS) are treated as "short" and length-diversified;
/// this comfortably covers every browseable library position and any casually
/// typed short index. PREFIX_BITS minus LENGTH_COUNT_LOG2 must be even (it is the
/// Feistel domain width for the per-target value permutation) — both enforced by
/// static_assert in IndexScramble.cpp.
#ifndef AUDIOBABEL_SCRAMBLE_PREFIX_BITS
#    define AUDIOBABEL_SCRAMBLE_PREFIX_BITS 256
#endif

#ifdef AUDIOBABEL_SCRAMBLE
inline constexpr bool kScrambleEnabledByDefault = true;
#else
inline constexpr bool kScrambleEnabledByDefault = false;
#endif

// --- Pure transform (always available, independent of the toggle) -----------

/**
 * @brief Keyed, reversible permutation of a non-negative index.
 * @param index Non-negative stored index value.
 * @param seed  Key selecting the permutation.
 * @return The scrambled index. A short input (band <= PREFIX_BITS/16) is moved
 *         to one of the spread target lengths (100 ms .. 15 s by default);
 *         longer inputs keep their length but have their content scattered.
 */
auto scramble(const cpp_int& index, uint64_t seed) -> cpp_int;

/**
 * @brief Exact inverse of scramble() for the same seed.
 */
auto unscramble(const cpp_int& index, uint64_t seed) -> cpp_int;

// --- Runtime toggle (defaults from the compile-time flag) -------------------

struct Config {
    bool     enabled;
    uint64_t seed;
};

/// Mutable process-wide config, initialised from the compile-time defaults.
auto config() -> Config&;

} // namespace AudioBabel::IndexScramble

#endif // AUDIOBABEL_INDEX_SCRAMBLE_H
