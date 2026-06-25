#ifndef AUDIOBABEL_INDEX_NAMING_H
#define AUDIOBABEL_INDEX_NAMING_H

#include <cstdint>
#include <string>
#include <vector>

namespace AudioBabel {

/**
 * @namespace IndexNaming
 * @brief Deterministic, decorrelated cosmetic names for library positions.
 *
 * Each hierarchy level (genre/wall, artist/shelf, album, track) gets a name
 * derived from an avalanche hash of its ancestor coordinates plus a
 * level-specific salt, NOT from slicing the full index's base64 string (the
 * old approach, which made neighbouring indices produce near-identical
 * names). This means:
 *  - A name depends only on its ancestors and its own slot, never on
 *    descendants — opening a different track doesn't change its album's name.
 *  - Sibling names (e.g. the 32 albums in one shelf) are guaranteed pairwise
 *    unique, via batch generation with collision retries.
 *  - Adjacent slots produce unrelated-looking names, since one mixIn() step
 *    avalanches roughly half the output bits for a one-slot change.
 *
 * Uniqueness is scoped to direct siblings only (e.g. albums within ONE
 * shelf) — never global across the unbounded room space, which would be
 * neither achievable nor meaningful.
 */
namespace IndexNaming {

    // --- Per-level seed builders --------------------------------------------
    // Each folds in exactly the ancestor coordinates that level depends on, plus
    // a level-specific salt, so genre/artist/album/track never look related.

    auto genreSeed(const std::string& room) -> uint64_t;
    auto artistSeed(const std::string& room, uint8_t wall) -> uint64_t;
    auto albumSeed(const std::string& room, uint8_t wall, uint8_t shelf) -> uint64_t;
    auto trackSeed(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> uint64_t;

    /**
 * @brief Generate `count` deterministic, pairwise-unique, decorrelated names
 * for one sibling group.
 * @param parentSeed Seed from one of the *Seed() builders above.
 * @param count Number of siblings (e.g. ALBUMS_PER_SHELF).
 * @return Names ordered by slot: result[i] is slot i's name.
 */
    auto generateSiblingNames(uint64_t parentSeed, size_t count) -> std::vector<std::string>;

    // --- Batch accessors (one WASM-exposed call per render) -----------------

    auto genreNames(const std::string& room) -> std::vector<std::string>;
    auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string>;
    auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string>;
    auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string>;

    // --- Single-leaf convenience wrappers ------------------------------------
    // Internally generate the full sibling batch and index into it, so a name
    // shown while listing siblings is identical to the name shown once that
    // specific slot is opened (single source of truth).

    auto genreNameFor(const std::string& room, uint8_t wall) -> std::string;
    auto artistNameFor(const std::string& room, uint8_t wall, uint8_t shelf) -> std::string;
    auto albumNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::string;
    auto trackNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album, uint8_t track) -> std::string;

} // namespace IndexNaming
} // namespace AudioBabel

#endif // AUDIOBABEL_INDEX_NAMING_H
