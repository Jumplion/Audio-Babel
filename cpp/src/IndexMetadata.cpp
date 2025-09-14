#include "../include/IndexMetadata.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

namespace AudioBabel {

auto IndexMetadata::extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata {
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);

    auto generateMetaName = [&](size_t off, size_t len) {
        std::string name;
        for (size_t i = 0; i < len; ++i) {
            uint8_t base = (off + i < bytes.size()) ? bytes[off + i] : 0;
            char    c    = static_cast<char>((base % 36) < 10 ? ('0' + (base % 10)) : ('a' + ((base % 36) - 10)));
            name.push_back(c);
        }
        return name;
    };

    IndexMetadata meta;
    if (bytes.empty()) {
        meta.genre  = "g0";
        meta.artist = "a0";
        meta.album  = "al0";
        meta.track  = "t0";
        return meta;
    }

    meta.genre  = generateMetaName(0, 6);
    meta.artist = generateMetaName(6, 8);
    meta.album  = generateMetaName(14, 8);
    meta.track  = generateMetaName(22, 6);

    // generate a tiny SVG cover from first bytes
    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='256' height='256'>";
    svg += "<rect width='100%' height='100%' fill='#";
    unsigned int color = 0;
    for (size_t i = 0; i < 3; ++i) {
        color = (color << 8) | (i < bytes.size() ? bytes[i] : 0);
    }
    const char* hex = "0123456789abcdef";
    for (int i = 5; i >= 0; --i) {
        unsigned int nib = (color >> (i * 4)) & 0xF;
        svg.push_back(hex[nib]);
    }
    svg += "'/><text x='50%' y='50%' font-size='20' text-anchor='middle' fill='#fff' dominant-baseline='middle'>";
    svg += meta.track;
    svg += "</text></svg>";
    meta.cover.assign(svg.begin(), svg.end());
    return meta;
}

} // namespace AudioBabel
