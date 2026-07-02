#ifndef AUDIOBABEL_INDEX_NAMING_H
#define AUDIOBABEL_INDEX_NAMING_H

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace AudioBabel {

// Deterministic, byte-derived genre/artist/album/track names for an audio
// index. All four are derived from the index itself (not its library
// coordinates) via one keyed, invertible Feistel permutation over the
// index's "name material" low part, so names scatter wildly between
// neighbouring indexes and, being invertible, a desired set of names can be
// turned straight into indexes that carry them (constructIndexesForNames),
// no search. See cpp/include/README.md for the full construction.
//
// The batch accessors below feed the Browse UI, which navigates by library
// coordinate; since a name is a property of a whole index, each names a
// representative index for that sibling slot (deeper coordinates 0).
namespace IndexNaming {

    using boost::multiprecision::cpp_int;

    // Max display width, in base64 characters, of one metadata field name.
    constexpr size_t NAME_MAX_CHARS = 16;

    struct Names {
        std::string genre;
        std::string artist;
        std::string album;
        std::string track;
    };

    // A construction request: each field is a name to pin down, or
    // std::nullopt meaning "leave it free" (randomized per generated index).
    struct NameQuery {
        std::optional<std::string> genre;
        std::optional<std::string> artist;
        std::optional<std::string> album;
        std::optional<std::string> track;
    };

    auto nameMaxChars() -> size_t;

    // --- Forward: index -> names ---------------------------------------
    auto namesForIndex(const cpp_int& index) -> Names;

    // --- Inverse: names -> indexes (construction, no search) -----------

    // Builds up to `count` indexes whose names match every pinned field in
    // `query`; free fields and the discriminator are randomized from `seed`.
    // Returns empty if a pinned name isn't producible (too long or invalid
    // character) — that's the expected "no such name" case, not an error.
    auto constructIndexesForNames(const NameQuery& query, size_t count, uint64_t seed) -> std::vector<cpp_int>;

    // --- Batch accessors for the Browse UI ------------------------------
    // Each names a representative index for the sibling slot (deeper
    // coordinates set to 0), in slot order.
    auto genreNames(const std::string& room) -> std::vector<std::string>;
    auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string>;
    auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string>;
    auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string>;

} // namespace IndexNaming
} // namespace AudioBabel

#endif // AUDIOBABEL_INDEX_NAMING_H
