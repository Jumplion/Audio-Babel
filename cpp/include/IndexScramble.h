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
 * @section Algorithm (keyed Feistel permutation, per tier)
 * Every index lives in a length-band: an L-sample payload maps to an integer in
 * [S_L, S_{L+1}) where S_L = (B^L - 1)/(B - 1) and the band width is exactly
 * B^L = 2^(16L).
 *
 * Length-bands are grouped into TIERS, each a contiguous run of bands capped at
 * a target audio duration. The tier boundaries (in seconds) are a compile-time
 * list, AUDIOBABEL_SCRAMBLE_TIER_SECONDS (default 1s, 5s, 10s, 15s, 4 tiers);
 * overriding it changes both the number of tiers and where they fall, e.g.
 * `-DAUDIOBABEL_SCRAMBLE_TIER_SECONDS="{2, 30}"` for two tiers capped at 2s and
 * 30s. See kTierMaxSamples in the .cpp for the derived per-tier sample caps.
 * Keep the top tier small: each Feistel round's cost scales with the tier's
 * byte width, so a much larger top tier (e.g. the previous 240s default) costs
 * multiple seconds per scramble()/unscramble() call.
 * Instead of permuting within one band, scramble() permutes across the whole
 * tier domain [S_lo, S_hi): it subtracts the tier's low end to get a value in
 * [0, N), runs it through a keyed Feistel network (4 rounds, balanced halves)
 * defined on the smallest even-bit power-of-two domain 2^e >= N, and cycle-walks
 * (re-applies the permutation until the result is back in [0, N)) so the domain
 * size need not be a power of two. Because each extra sample multiplies a band's
 * size by B, a tier's top band holds ~(1 - 1/B) of the tier, so a short index
 * almost always lands near the tier's maximum length — short user input now
 * yields a wide, interesting range of audio lengths instead of near-silence.
 * Payloads longer than the last tier keep the original per-band permutation.
 *
 * A Feistel network is a bijection for ANY round function and is inverted simply
 * by running the rounds in reverse — so unscramble() needs no modular inverse and
 * both directions are O(tier width). Cycle-walking preserves the bijection on
 * [0, N), the permutation never leaves the tier, 0 maps to 0, and every integer
 * maps to a valid integer, so the bijection and "nothing is ever rejected"
 * invariants still hold. The mapping is NOT length-preserving inside a tier: a
 * given length still has exactly as many indices as before (every payload, down
 * to 3 samples, remains reachable), but which indices land on it are scattered
 * across the tier.
 *
 * (An earlier design used an affine map y -> (a*y + c) mod 2^(16L); it was
 * replaced because undoing it needs a big-integer modular inverse, which made
 * decoding take seconds on multi-MB clips. Feistel is O(N) both ways.)
 *
 * @section references References
 * - Feistel networks: https://en.wikipedia.org/wiki/Feistel_cipher
 * - Cycle-walking: Black & Rogaway, "Ciphers with Arbitrary Finite Domains"
 *   (CT-RSA 2002), https://www.cs.ucdavis.edu/~rogaway/papers/subset.pdf
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

/// Tier boundaries in seconds, brace-initializer-list form. Defines both the
/// number of tiers and where they fall; must be non-empty and strictly
/// increasing (enforced by static_assert in IndexScramble.cpp). Override at
/// build time to experiment, e.g. -DAUDIOBABEL_SCRAMBLE_TIER_SECONDS="{2, 30}".
/// Kept short on purpose: each Feistel round processes a half as wide as the
/// whole tier, so cost grows with the top tier's size (a 240s top tier cost
/// multiple seconds per call; 15s keeps it well under a second).
#ifndef AUDIOBABEL_SCRAMBLE_TIER_SECONDS
#    define AUDIOBABEL_SCRAMBLE_TIER_SECONDS {1, 5, 10, 15}
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
 * @return The scrambled index. It stays within the same tier as the input, so
 *         the decoded length may change but is bounded by that tier's maximum
 *         (indices longer than the last tier keep their exact length).
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
