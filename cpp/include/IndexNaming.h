#ifndef AUDIOBABEL_INDEX_NAMING_H
#define AUDIOBABEL_INDEX_NAMING_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace AudioBabel {

/**
 * @namespace IndexNaming
 * @brief Deterministic, byte-derived metadata names for an audio index.
 *
 * The four metadata fields (genre/artist/album/track) are derived directly from
 * the index itself (NOT its library coordinates) via one keyed, invertible
 * big-integer Feistel permutation over the "name material" low part of the index.
 * This makes the names deterministic, scatters neighbouring indexes wildly, and —
 * because every step is invertible — lets a desired set of names be turned
 * straight into concrete indexes carrying them (constructIndexesForNames), with no
 * search. See cpp/include/README.md for the full construction and properties.
 *
 * The batch accessors below feed the Browse UI, which navigates by library
 * coordinate. Since a name is a property of a whole index, each accessor names
 * a *representative* index for that sibling slot (deeper coordinates 0).
 */
namespace IndexNaming {

    using boost::multiprecision::cpp_int;

    /// Maximum display width, in base64 characters, of a single metadata field
    /// name. Names range from 1 to this many characters. Configurable — change
    /// it and the per-field name space (and UI hints) follow automatically.
    constexpr size_t NAME_MAX_CHARS = 16;

    /// The four metadata names of one index.
    struct Names {
        std::string genre;
        std::string artist;
        std::string album;
        std::string track;
    };

    /// A construction request: each field is either a name to pin down, or
    /// std::nullopt meaning "leave it free" (randomized per generated index).
    struct NameQuery {
        std::optional<std::string> genre;
        std::optional<std::string> artist;
        std::optional<std::string> album;
        std::optional<std::string> track;
    };

    /// Maximum characters per field name (returns NAME_MAX_CHARS) — exposed so
    /// callers (and the WASM/JS layer) need not duplicate the constant.
    auto nameMaxChars() -> size_t;

    // --- Forward: index -> names --------------------------------------------

    /// Derive the four metadata names from a full audio index.
    auto namesForIndex(const cpp_int& index) -> Names;

    // --- Inverse: names -> indexes (construction, no search) ----------------

    /**
     * @brief Build `count` indexes whose names match every pinned field.
     * @param query Per-field constraints; std::nullopt fields are randomized.
     * @param count How many distinct candidate indexes to return.
     * @param seed  Seed for the per-call randomness (free fields + discriminator),
     *              so results are reproducible for a given seed.
     * @return Up to `count` indexes, each satisfying the pinned names. Empty if
     *         any pinned name is not a producible name (too long or invalid
     *         character) — that is the expected "no such name" case, not an error.
     */
    auto constructIndexesForNames(const NameQuery& query, size_t count, uint64_t seed) -> std::vector<cpp_int>;

    // --- Batch accessors for the Browse UI ----------------------------------
    // Each names a representative index for the sibling slot (deeper
    // coordinates set to 0), in slot order.

    auto genreNames(const std::string& room) -> std::vector<std::string>;
    auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string>;
    auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string>;
    auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string>;

} // namespace IndexNaming
} // namespace AudioBabel

#endif // AUDIOBABEL_INDEX_NAMING_H
