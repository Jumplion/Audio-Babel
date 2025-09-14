#ifndef BASE64URL_H
#define BASE64URL_H

#include <cstdint>
#include <string>
#include <vector>


namespace AudioBabel {

// URL-safe base64 utilities (alphabet A-Z a-z 0-9 - _), no padding
// All functions are noexcept only for non-throwing wrappers; decode throws std::invalid_argument

bool isValidBase64Url(const std::string& s);

// Decode URL-safe base64 (no padding) into bytes. Throws std::invalid_argument on invalid input.
std::vector<uint8_t> decodeBase64Url(const std::string& s);

// Encode bytes into URL-safe base64 (no padding)
std::string encodeBase64Url(const std::vector<uint8_t>& bytes);

} // namespace AudioBabel

#endif // BASE64URL_H
