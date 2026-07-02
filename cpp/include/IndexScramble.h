#ifndef AUDIOBABEL_INDEX_SCRAMBLE_H
#define AUDIOBABEL_INDEX_SCRAMBLE_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>

// Optional, reversible permutation that (1) scrambles a payload's content
// within its own length band so neighbouring indexes no longer share almost
// all of their samples, and (2) spreads short indexes across a wide, varied
// range of decoded durations (default 100 ms .. 5 s) instead of all
// collapsing to near-silence — since the index uses bijective base-65536
// numeration, a short index otherwise decodes to only one or two samples.
// Both transforms are keyed bijections, so index<->payload stays a perfect
// bijection. Full algorithm (band math, why Feistel over an XOR mask, the
// length-spread swap) is in docs/INDEX_FORMAT.md; tuning-constant
// static_asserts live in cpp/src/IndexScramble.cpp.
//
// scramble()/unscramble() are always available. Whether Index::encode/decode
// actually applies them is controlled by config() (runtime switch, default
// from the compile-time flag AUDIOBABEL_SCRAMBLE). Define
// AUDIOBABEL_SCRAMBLE (optionally with AUDIOBABEL_SCRAMBLE_SEED) to bake the
// toggle in for a final build.
namespace AudioBabel::IndexScramble {

using boost::multiprecision::cpp_int;

// --- Compile-time configuration ---------------------------------------------
#ifndef AUDIOBABEL_SCRAMBLE_SEED
#    define AUDIOBABEL_SCRAMBLE_SEED 0x9E3779B97F4A7C15ULL
#endif

// Shortest/longest decoded duration (ms, at DEFAULT_SAMPLE_RATE) that the
// length-spread maps short indexes onto; every short index lands on one of
// AUDIOBABEL_SCRAMBLE_LENGTH_COUNT distinct, log-spaced lengths in this
// range. Indexes outside the PREFIX range are unaffected. MIN must be > 0
// and < MAX, and MIN's sample count must exceed the diversified prefix's
// band range so PREFIX and TARGET sets stay disjoint (static_asserted in
// IndexScramble.cpp).
#ifndef AUDIOBABEL_SCRAMBLE_MIN_MS
#    define AUDIOBABEL_SCRAMBLE_MIN_MS 100
#endif
#ifndef AUDIOBABEL_SCRAMBLE_MAX_MS
#    define AUDIOBABEL_SCRAMBLE_MAX_MS 5000
#endif

// log2 of the number of distinct target lengths a short index can land on.
// Must be < AUDIOBABEL_SCRAMBLE_PREFIX_BITS. Default 8 => 256 lengths spread
// log-uniformly across [MIN_MS, MAX_MS].
#ifndef AUDIOBABEL_SCRAMBLE_LENGTH_COUNT_LOG2
#    define AUDIOBABEL_SCRAMBLE_LENGTH_COUNT_LOG2 8
#endif

// Indexes in [1, 2^PREFIX_BITS) are "short" and length-diversified; this
// comfortably covers every browseable library position and any casually
// typed short index. PREFIX_BITS - LENGTH_COUNT_LOG2 must be even (it's the
// Feistel domain width for the per-target value permutation) — static_asserted
// in IndexScramble.cpp.
#ifndef AUDIOBABEL_SCRAMBLE_PREFIX_BITS
#    define AUDIOBABEL_SCRAMBLE_PREFIX_BITS 256
#endif

#ifdef AUDIOBABEL_SCRAMBLE
inline constexpr bool kScrambleEnabledByDefault = true;
#else
inline constexpr bool kScrambleEnabledByDefault = false;
#endif

// --- Pure transform (always available, independent of the toggle) -----------

// Keyed, reversible permutation of a non-negative index. A short input (band
// <= PREFIX_BITS/16) moves to one of the spread target lengths; longer
// inputs keep their length but have their content scattered.
auto scramble(const cpp_int& index, uint64_t seed) -> cpp_int;

// Exact inverse of scramble() for the same seed.
auto unscramble(const cpp_int& index, uint64_t seed) -> cpp_int;

// --- Runtime toggle (defaults from the compile-time flag) -------------------

struct Config {
    bool     enabled;
    uint64_t seed;
};

// Mutable process-wide config, initialised from the compile-time defaults.
auto config() -> Config&;

} // namespace AudioBabel::IndexScramble

#endif // AUDIOBABEL_INDEX_SCRAMBLE_H
