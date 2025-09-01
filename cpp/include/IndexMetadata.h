#pragma once

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

namespace AudioBabel {

struct IndexMetadata {
    std::string genre;
    std::string artist;
    std::string album;
    std::string track;
    std::vector<uint8_t> cover; // optional small image bytes
};

// Deterministically derive metadata tokens from a big-integer index.
IndexMetadata indexToMetadata(const boost::multiprecision::cpp_int& index);

} // namespace AudioBabel
