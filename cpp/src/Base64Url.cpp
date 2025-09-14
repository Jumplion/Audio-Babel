#include "../include/Base64Url.h"

#include <array>
#include <stdexcept>

namespace AudioBabel {

bool isValidBase64Url(const std::string& s) {
    for (char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> decodeBase64Url(const std::string& s) {
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

std::string encodeBase64Url(const std::vector<uint8_t>& bytes) {
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
    return b64str;
}

} // namespace AudioBabel
