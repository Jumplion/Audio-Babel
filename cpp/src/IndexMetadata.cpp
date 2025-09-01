#include "IndexMetadata.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <vector>

using boost::multiprecision::cpp_int;
namespace AudioBabel {

// Produce deterministic gibberish tokens from exported index bytes.
IndexMetadata indexToMetadata(const cpp_int& index) {
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);

    // Build tokens by mapping groups of bytes to printable characters
    auto make_token = [&](size_t offset, size_t len) {
        std::string s;
        s.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            uint8_t b = 0;
            if (offset + i < bytes.size()) b = bytes[offset + i];
            // map to 20..z range (alphanumeric-ish)
            char c = static_cast<char>((b % 36) < 10 ? ('0' + (b % 10)) : ('a' + ((b % 36) - 10)));
            s.push_back(c);
        }
        return s;
    };

    IndexMetadata m;
    if (bytes.empty()) {
        m.genre = "g0";
        m.artist = "a0";
        m.album = "al0";
        m.track = "t0";
        return m;
    }

    m.genre = make_token(0, 6);
    m.artist = make_token(6, 8);
    m.album = make_token(14, 8);
    m.track = make_token(22, 6);

    // cover: generate a minimal SVG (UTF-8 bytes) deterministically from first bytes
    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256'>";
    svg += "<rect width='100%' height='100%' fill='#";
    char buf[8];
    unsigned int color = 0;
    for (size_t i = 0; i < 3; ++i) {
        color = (color << 8) | (i < bytes.size() ? bytes[i] : 0);
    }
    // simple hex
    const char* hex = "0123456789abcdef";
    for (int i = 5; i >= 0; --i) {
        unsigned int nib = (color >> (i * 4)) & 0xF;
        svg.push_back(hex[nib]);
    }
    svg += "'/>";
    svg += "<text x='50%' y='50%' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>";
    svg += m.track;
    svg += "</text></svg>";
    m.cover.assign(svg.begin(), svg.end());

    return m;
}

} // namespace AudioBabel
