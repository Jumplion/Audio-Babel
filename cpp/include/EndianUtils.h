#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#include <array>
#include <cstdint>
#include <ostream>

// Small endian helpers used across the audio code. Templates are inline and
// header-only to avoid ODR issues.
namespace AudioBabel::EndianUtils {

template <typename T>
inline void write_le(std::ostream& out, T value) {
    std::array<uint8_t, sizeof(T)> buf{};
    for (size_t index = 0; index < sizeof(T); ++index) {
        buf[index] = static_cast<uint8_t>((value >> (index * 8)) & 0xFFU);
    }
    out.write(reinterpret_cast<const char*>(buf.data()), sizeof(T));
}

inline void write_u32_le(std::ostream& out, uint32_t value) {
    write_le<uint32_t>(out, value);
}
inline void write_u16_le(std::ostream& out, uint16_t value) {
    write_le<uint16_t>(out, value);
}

template <typename T>
inline auto read_le(const char* ptr) -> T {
    uint64_t acc = 0;
    for (size_t index = 0; index < sizeof(T); ++index) {
        acc |= (static_cast<uint64_t>(static_cast<uint8_t>(ptr[index])) << (index * 8));
    }
    return static_cast<T>(acc);
}

inline auto read_u16_le(const char* ptr) -> uint16_t {
    return read_le<uint16_t>(ptr);
}
inline auto read_u32_le(const char* ptr) -> uint32_t {
    return read_le<uint32_t>(ptr);
}

} // namespace AudioBabel::EndianUtils

#endif // ENDIAN_UTILS_H
