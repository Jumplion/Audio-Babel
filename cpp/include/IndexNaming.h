#ifndef AUDIOBABEL_INDEX_NAMING_H
#define AUDIOBABEL_INDEX_NAMING_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace AudioBabel {

/**
 * @namespace IndexNaming
 * @brief Deterministic, invertible cosmetic names for library positions.
 *
 * Each hierarchy level (genre/wall, artist/shelf, album, track) gets a name
 * derived from a keyed Feistel permutation (the same cycle-walked construction
 * IndexScramble uses for audio indices, see IndexScramble.h) over that level's
 * within-room coordinate, NOT from a one-way hash. This means:
 *  - A name depends only on its ancestors and its own slot, never on
 *    descendants — opening a different track doesn't change its album's name.
 *  - Sibling names are pairwise unique *for free*: distinct coordinates feed
 *    distinct inputs into a bijection, so collisions are structurally
 *    impossible rather than merely retried away.
 *  - Given a room and a name, the coordinate can be recovered in O(1) via the
 *    `*SlotFor` decode functions below — no enumeration of siblings needed.
 *    This is what makes cross-room name search (see IndexFinder) practical:
 *    checking whether a room contains a given name is a single permutation,
 *    not a generate-and-compare over every slot.
 *
 * Uniqueness is scoped to direct siblings only (e.g. albums within ONE
 * shelf) — never global across the unbounded room space, which would be
 * neither achievable nor meaningful (see IndexFinder.h for why).
 */
namespace IndexNaming {

    /// Shelf coordinate decoded from an artist name (wall is also unconstrained
    /// by a name alone unless paired with the genre name, hence both fields).
    struct ArtistSlot {
        uint8_t wall;
        uint8_t shelf;
    };

    struct AlbumSlot {
        uint8_t wall;
        uint8_t shelf;
        uint8_t album;
    };

    struct TrackSlot {
        uint8_t wall;
        uint8_t shelf;
        uint8_t album;
        uint8_t track;
    };

    // --- Batch accessors (one WASM-exposed call per render) -----------------

    auto genreNames(const std::string& room) -> std::vector<std::string>;
    auto artistNames(const std::string& room, uint8_t wall) -> std::vector<std::string>;
    auto albumNames(const std::string& room, uint8_t wall, uint8_t shelf) -> std::vector<std::string>;
    auto trackNames(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::vector<std::string>;

    // --- Single-leaf convenience wrappers ------------------------------------
    // Internally compute the same permutation generateSiblingNames would have
    // for this one slot, so a name shown while listing siblings is identical
    // to the name shown once that specific slot is opened (single source of
    // truth), with no batch generation required.

    auto genreNameFor(const std::string& room, uint8_t wall) -> std::string;
    auto artistNameFor(const std::string& room, uint8_t wall, uint8_t shelf) -> std::string;
    auto albumNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album) -> std::string;
    auto trackNameFor(const std::string& room, uint8_t wall, uint8_t shelf, uint8_t album, uint8_t track) -> std::string;

    // --- Decode: name -> coordinate, given the room -------------------------
    // Inverse of the *NameFor functions above. Returns std::nullopt if `name`
    // is not a name that *NameFor could have produced for this room (wrong
    // width, invalid character, or fails the built-in decoration checksum) —
    // these are NOT exceptions because a mistyped/foreign-room query is the
    // expected common case for callers like IndexFinder, not an error.

    auto genreSlotFor(const std::string& room, const std::string& name) -> std::optional<uint8_t>;
    auto artistSlotFor(const std::string& room, const std::string& name) -> std::optional<ArtistSlot>;
    auto albumSlotFor(const std::string& room, const std::string& name) -> std::optional<AlbumSlot>;
    auto trackSlotFor(const std::string& room, const std::string& name) -> std::optional<TrackSlot>;

} // namespace IndexNaming
} // namespace AudioBabel

#endif // AUDIOBABEL_INDEX_NAMING_H
