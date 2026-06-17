#ifndef AUDIOBABEL_INDEX_SCRAMBLE_H
#define AUDIOBABEL_INDEX_SCRAMBLE_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>

/**
 * @file IndexScramble.h
 * @brief Optional, reversible pseudo-random placement of indices.
 *
 * By default the index of a payload sits numerically next to the indices of
 * very similar payloads, so neighbouring "library" positions hold near-identical
 * audio. This module applies a keyed, reversible permutation so that neighbours
 * are scattered across the space while the mapping stays a perfect bijection.
 *
 * @section algorithm Algorithm (keyed Feistel permutation per length-band)
 * Every index lives in a length-band: an L-sample payload maps to an integer in
 * [S_L, S_{L+1}) where S_L = (B^L - 1)/(B - 1) and the band width is exactly
 * B^L = 2^(16L). We subtract S_L to get the band value y in [0, 2^(16L)), then
 * permute y with a keyed Feistel network (4 rounds; halves of 8L bits each, since
 * 16L is always even). The round keys are derived from a seed and the band index.
 *
 * A Feistel network is a bijection for ANY round function and is inverted simply
 * by running the rounds in reverse — so unscramble() needs no modular inverse and
 * both directions are O(N) in the payload size. Because the permutation stays
 * inside the band, the payload length is preserved, 0 maps to 0, and every integer
 * maps to a valid integer, so the bijection and "nothing is ever rejected"
 * invariants still hold. With four rounds a single-sample change avalanches across
 * the whole index, so numerically adjacent payloads land far apart.
 *
 * (An earlier design used an affine map y -> (a*y + c) mod 2^(16L); it was
 * replaced because undoing it needs a big-integer modular inverse, which made
 * decoding take seconds on multi-MB clips. Feistel is O(N) both ways.)
 *
 * @section toggle Toggling
 * The pure scramble()/unscramble() functions are always available. Whether the
 * core encode/decode actually applies them is controlled by config() (a runtime
 * switch used for testing) whose default comes from the compile-time flag
 * AUDIOBABEL_SCRAMBLE. Define AUDIOBABEL_SCRAMBLE (optionally with
 * AUDIOBABEL_SCRAMBLE_SEED) to bake it in for a final build; the runtime setter
 * can then be dropped.
 */

namespace AudioBabel::IndexScramble {

using boost::multiprecision::cpp_int;

// --- Compile-time configuration ---------------------------------------------
#ifndef AUDIOBABEL_SCRAMBLE_SEED
#    define AUDIOBABEL_SCRAMBLE_SEED 0x9E3779B97F4A7C15ULL
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
 * @return The scrambled index (same length-band as the input).
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

/// Apply scramble() if enabled, otherwise return the index unchanged.
auto applyScramble(const cpp_int& index) -> cpp_int;

/// Apply unscramble() if enabled, otherwise return the index unchanged.
auto applyUnscramble(const cpp_int& index) -> cpp_int;

} // namespace AudioBabel::IndexScramble

#endif // AUDIOBABEL_INDEX_SCRAMBLE_H
