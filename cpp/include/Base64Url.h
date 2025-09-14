#ifndef BASE64URL_H
#define BASE64URL_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace AudioBabel {

// URL-safe base64 utilities (alphabet A-Z a-z 0-9 - _), no padding
// All functions are noexcept only for non-throwing wrappers; decode throws std::invalid_argument

// Note: accepts any string-like input via std::string_view; constexpr for small compile-time checks.
constexpr auto isValidBase64Url(std::string_view s) -> bool {
    for (char c : s) {
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// Decode URL-safe base64 (no padding) into bytes. Throws std::invalid_argument on invalid input.
auto decodeBase64Url(const std::string& s) -> std::vector<uint8_t>;

// Encode bytes into URL-safe base64 (no padding)
auto encodeBase64Url(const std::vector<uint8_t>& bytes) -> std::string;

} // namespace AudioBabel

#endif // BASE64URL_H
