#include "../include/IndexMetadata.h"

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace AudioBabel {

static std::string generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track) {
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
    svg += track;
    svg += "</text></svg>";
    return svg;
}

// Public wrapper to expose generateSvgCover through the IndexMetadata API
std::string IndexMetadata::generateSvgCover(const std::vector<uint8_t>& bytes, const std::string& track) {
    return ::AudioBabel::generateSvgCover(bytes, track);
}

bool IndexMetadata::isValidBase64(const std::string& s) {
    for (char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

// Helper: decode URL-safe base64 (alphabet A-Z a-z 0-9 - _) without padding
static std::vector<uint8_t> decodeUrlSafeBase64(const std::string& s) {
    static const std::array<int8_t, 256> rev = []() {
        std::array<int8_t, 256> table;
        table.fill(-1);
        const std::string alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        for (size_t i = 0; i < alpha.size(); ++i)
            table[static_cast<unsigned char>(alpha[i])] = static_cast<int8_t>(i);
        return table;
    }();

    std::vector<uint8_t> out;
    uint32_t             acc      = 0;
    int                  acc_bits = 0;
    for (char ch : s) {
        int8_t v = rev[static_cast<unsigned char>(ch)];
        if (v < 0) {
            throw std::invalid_argument("Invalid base64 character in input");
        }
        acc = (acc << 6) | static_cast<uint32_t>(v);
        acc_bits += 6;
        if (acc_bits >= 8) {
            acc_bits -= 8;
            uint8_t b = static_cast<uint8_t>((acc >> acc_bits) & 0xFF);
            out.push_back(b);
        }
    }
    return out;
}

// Forward declaration for helper used by both overloads
static IndexMetadata buildMetadataFromBytesAndB64(const std::vector<uint8_t>& bytes, const std::string& b64str);

// Overload: accept a URL-safe base64 string representing the index bytes
auto IndexMetadata::extractMetadataFromIndex(const std::string& base64Index) -> IndexMetadata {
    std::vector<uint8_t> bytes = decodeUrlSafeBase64(base64Index);
    return buildMetadataFromBytesAndB64(bytes, base64Index);
}

auto IndexMetadata::extractMetadataFromIndex(const boost::multiprecision::cpp_int& index) -> IndexMetadata {
    std::vector<uint8_t> bytes;
    boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);

    // Convert all bytes to a deterministic URL-safe base64 string (no padding).
    // This matches the alphabet used elsewhere in the project.
    static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string       b64str;
    b64str.reserve((bytes.size() * 8 + 5) / 6);
    uint32_t acc      = 0;
    int      acc_bits = 0;
    for (uint8_t byte : bytes) {
        acc = (acc << 8) | byte;
        acc_bits += 8;
        while (acc_bits >= 6) {
            acc_bits -= 6;
            uint8_t idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
            b64str.push_back(b64_alpha[idx]);
        }
    }
    if (acc_bits > 0) {
        uint8_t idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
        b64str.push_back(b64_alpha[idx]);
    }

    return buildMetadataFromBytesAndB64(bytes, b64str);
}

// Centralized helper that builds IndexMetadata from raw bytes and the
// corresponding base64 string. This removes duplication between overloads.
static IndexMetadata buildMetadataFromBytesAndB64(const std::vector<uint8_t>& bytes, const std::string& b64str) {
    IndexMetadata meta;
    if (b64str.empty()) {
        meta.genre  = "g0";
        meta.artist = "a0";
        meta.album  = "al0";
        meta.track  = "t0";
        return meta;
    }

    std::array<uint32_t, 4> weights = {0, 0, 0, 0};
    for (size_t i = 0; i < bytes.size(); ++i) {
        weights[i % 4] += static_cast<uint32_t>(bytes[i]);
    }
    uint32_t totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
    if (totalWeight == 0) {
        weights     = {1, 1, 1, 1};
        totalWeight = 4;
    }

    size_t                b64Len = b64str.size();
    std::array<size_t, 4> lens   = {0, 0, 0, 0};

    size_t sum = 0;
    for (int i = 0; i < 4; ++i) {
        lens[i] = (b64Len * weights[i]) / totalWeight;
        if (lens[i] == 0) {
            lens[i] = 1;
        }
        sum += lens[i];
    }

    for (size_t i = 0; sum < b64Len; ++i) {
        lens[i % 4]++;
        sum++;
    }
    for (int i = 3; sum > b64Len && i >= 0; --i) {
        if (lens[i] > 1) {
            lens[i]--;
            sum--;
        }
        if (i == 0 && sum > b64Len) {
            i = 4;
        }
    }

    size_t pos = 0;
    meta.genre = b64str.substr(pos, lens[0]);
    pos += lens[0];
    meta.artist = (pos < b64Len) ? b64str.substr(pos, lens[1]) : std::string();
    pos += lens[1];
    meta.album = (pos < b64Len) ? b64str.substr(pos, lens[2]) : std::string();
    pos += lens[2];
    meta.track = (pos < b64Len) ? b64str.substr(pos, lens[3]) : std::string();

    std::string svg = generateSvgCover(bytes, meta.track);
    meta.cover.assign(svg.begin(), svg.end());
    return meta;
}

} // namespace AudioBabel
