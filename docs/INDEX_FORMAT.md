# Audio Index Format Specification

**Version:** 1.0  
**Date:** October 30, 2025  
**Project:** Speaker of Babel - Audio Indexing Library

---

## Overview

The Speaker of Babel audio indexing system encodes audio files as deterministic big integers, enabling lossless reconstruction and hierarchical organization. Each **audio index** is a single large integer that embeds both format metadata and the complete PCM audio payload.

This document specifies the byte-level structure of audio indexes and their serialization formats.

---

## Index Structure

An audio index consists of two parts concatenated together:

1. **13-byte Header** - Format metadata (version, sample rate, bit depth, etc.)
2. **Variable-length PCM Payload** - Raw audio sample data

These components are concatenated into a single byte array, which is then converted to a big integer using MSB-first (most significant byte first) encoding.

```cpp
┌─────────────────────┬──────────────────────────────────┐
│   13-byte Header    │    PCM Sample Data (variable)    │
└─────────────────────┴──────────────────────────────────┘
         ↓
    Big Integer (MSB-first)
         ↓
    URL-safe Base64 (for storage/transmission)
```

---

## Header Format (13 bytes)

All multi-byte integer fields in the header use **little-endian** byte order.

| Byte Offset | Size (bytes) | Type     | Field Name      | Description                                      |
|-------------|--------------|----------|-----------------|--------------------------------------------------|
| 0           | 1            | uint8    | Version         | Format version (currently `0x01`)                |
| 1-4         | 4            | uint32   | Frame Count     | Number of audio frames (samples per channel)     |
| 5-8         | 4            | uint32   | Sample Rate     | Sample rate in Hz (e.g., 44100)                  |
| 9-10        | 2            | uint16   | Bit Depth       | Bits per sample (8, 16, or 32)                   |
| 11-12       | 2            | uint16   | Channel Count   | Number of audio channels (1=mono, 2=stereo)      |

### Header Field Details

#### Version (Byte 0)

- **Current Value:** `0x01`
- **Purpose:** Identifies the header format version for forward compatibility
- **Future:** New versions may extend the header or change serialization

#### Frame Count (Bytes 1-4)

- **Range:** 0 to 4,294,967,295 frames
- **Definition:** Number of samples *per channel*
- **Example:** For 2 minutes of 44.1kHz mono audio: `44100 × 120 = 5,292,000` frames

#### Sample Rate (Bytes 5-8)

- **Range:** 1 to 4,294,967,295 Hz (practical: 8000-192000 Hz)
- **Common Values:** 8000, 16000, 22050, 44100, 48000, 96000, 192000 Hz
- **Purpose:** Specifies playback rate for audio reconstruction

#### Bit Depth (Bytes 9-10)

- **Supported Values:** 8, 16, or 32 bits per sample
- **Purpose:** Determines how many bytes each sample occupies
- **Encoding:** Stored as integer (8, 16, or 32)

#### Channel Count (Bytes 11-12)

- **Common Values:** 1 (mono), 2 (stereo)
- **Range:** 1 to 65,535 channels (practical: 1-8)
- **Purpose:** Number of interleaved audio channels

---

## PCM Payload Format (Variable Length)

Following the 13-byte header, the PCM sample data is stored as raw bytes in **little-endian** format per sample.

### Sample Packing by Bit Depth

#### 8-bit Samples

- **Bytes per Sample:** 1
- **Format:** Unsigned 8-bit integer (0-255)
- **Example:** `[0x80, 0x90, 0x70]` = three samples

#### 16-bit Samples (Most Common)

- **Bytes per Sample:** 2
- **Format:** Signed 16-bit integer, little-endian (-32768 to 32767)
- **Example:** `[0x00, 0x10, 0xFF, 0x0F]` = two samples (0x1000, 0x0FFF)

#### 32-bit Samples

- **Bytes per Sample:** 4
- **Format:** Signed 32-bit integer, little-endian
- **Example:** `[0x00, 0x00, 0x10, 0x00]` = one sample (0x00100000)

### Payload Size Calculation

```cpp
payload_size = frame_count × channel_count × (bit_depth / 8)
```

**Example:** 2 minutes of 44.1kHz, 16-bit, mono audio:

```cpp
payload_size = 5,292,000 × 1 × 2 = 10,584,000 bytes
```

---

## Complete Index Layout Example

### Example: Minimal 16-bit Stereo Index

**Audio Properties:**

- 3 frames (6 samples total: 3 per channel)
- 44100 Hz sample rate
- 16-bit depth
- 2 channels (stereo)

**Header (13 bytes):**

```cpp
Offset  Hex Value   Field
------  ---------   -----
0       01          Version = 1
1-4     03 00 00 00 Frame Count = 3 (little-endian)
5-8     44 AC 00 00 Sample Rate = 44100 (little-endian: 0x0000AC44)
9-10    10 00       Bit Depth = 16
11-12   02 00       Channel Count = 2
```

**PCM Payload (12 bytes, interleaved L-R-L-R-L-R):**

```cpp
Offset  Hex Value   Description
------  ---------   -----------
13-14   00 10       Frame 0, Left channel  (0x1000 = 4096)
15-16   00 20       Frame 0, Right channel (0x2000 = 8192)
17-18   FF 0F       Frame 1, Left channel  (0x0FFF = 4095)
19-20   00 30       Frame 1, Right channel (0x3000 = 12288)
21-22   00 00       Frame 2, Left channel  (0x0000 = 0)
23-24   FF FF       Frame 2, Right channel (0xFFFF = -1)
```

**Total Index Size:** 25 bytes (13-byte header + 12-byte payload)

---

## Serialization to Big Integer

The complete byte array (header + payload) is converted to a big integer using **MSB-first (big-endian)** byte ordering:

```cpp
// C++ serialization (simplified)
std::vector<uint8_t> bytes = header_bytes + payload_bytes;
boost::multiprecision::cpp_int index = 0;
boost::multiprecision::import_bits(index, bytes.begin(), bytes.end(), 8, true);
```

**Key Point:** The byte array is treated as a single large number where:

- `bytes[0]` (version byte) becomes the *most significant* byte
- `bytes[n-1]` (last payload byte) becomes the *least significant* byte

### Deserialization from Big Integer

Reconstructing the byte array from the index:

```cpp
// C++ deserialization (simplified)
std::vector<uint8_t> bytes;
boost::multiprecision::export_bits(index, std::back_inserter(bytes), 8, true);
// bytes[0-12] = header
// bytes[13+] = PCM payload
```

**Important:** Leading zero bytes in the PCM payload are preserved by calculating the expected size from header metadata.

---

## Base64 Encoding for Storage

For file storage and web transmission, indexes are encoded using **URL-safe base64 without padding** (RFC 4648 Section 5):

### Encoding Rules

- **Alphabet:** `A-Z a-z 0-9 - _` (64 characters)
- **Padding:** None (no `=` characters)
- **Line Breaks:** None (single continuous string)

### Example

```cpp
Index bytes:  01 03 00 00 00 44 AC 00 00 10 00 02 00 ...
Base64:       AQMAAABELEGAABAACAA...
```

### Validation

Valid base64 strings must:

- Contain only characters from the URL-safe alphabet
- Have no `=` padding characters
- Decode to at least 13 bytes (minimum header size)

---

## Constraints and Limits

### Web Application Constraints (WASM)

For browser-based usage, the following limits are recommended:

| Parameter       | Limit          | Reason                                    |
|-----------------|----------------|-------------------------------------------|
| Duration        | 2 minutes      | Index size ~14MB base64 for typical audio|
| Sample Rate     | 44.1 kHz       | Standard quality, manageable size         |
| Bit Depth       | 16-bit         | CD quality, efficient encoding            |
| Channels        | 1 (mono)       | Simplifies indexing, reduces size         |

### Theoretical Limits

| Parameter       | Maximum                  | Notes                                |
|-----------------|--------------------------|--------------------------------------|
| Duration        | ~27,000 hours            | Based on uint32 frame count at 44.1kHz|
| Sample Rate     | 4.29 GHz                 | uint32 limit (impractical)           |
| Bit Depth       | 32-bit                   | Implementation supports up to 32     |
| Channels        | 65,535                   | uint16 limit                         |
| Index Size      | Unlimited                | Limited only by big integer library  |

---

## Validation and Error Handling

### Required Validations

When deserializing an index, implementations **must** validate:

1. **Minimum Size:** At least 13 bytes (header only)
2. **Version:** Header byte 0 must be `0x01` (current version)
3. **Bit Depth:** Must be 8, 16, or 32
4. **Payload Size:** Must match calculation: `frame_count × channel_count × (bit_depth / 8)`
5. **Base64:** Must use URL-safe alphabet with no padding

### Error Conditions

| Condition                          | Error Type              | Action                              |
|------------------------------------|-------------------------|-------------------------------------|
| Index < 13 bytes                   | `std::runtime_error`    | Reject: incomplete header           |
| Unknown version byte               | `std::runtime_error`    | Reject: unsupported format          |
| Bit depth not 8/16/32              | `std::runtime_error`    | Reject: unsupported audio format    |
| Payload size mismatch              | `std::runtime_error`    | Reject: corrupted index             |
| Invalid base64 character           | `std::invalid_argument` | Reject: malformed encoding          |

---

## Implementation Notes

### Endianness Summary

- **Header Fields:** Little-endian (matches WAV format convention)
- **PCM Samples:** Little-endian per sample
- **Big Integer Conversion:** MSB-first (big-endian byte order)

### WAV File Compatibility

The header format intentionally mirrors RIFF/WAVE structure to enable efficient conversion:

- Frame count ↔ Number of samples
- Sample rate ↔ Sample rate
- Bit depth ↔ Bits per sample
- Channels ↔ Number of channels

### Determinism Guarantee

For any given audio file, the index is **deterministic and reproducible**:

```cpp
WAV File → AudioData → Index → AudioData → WAV File
         (identical)        (identical)
```

This property enables:

- Content-addressable storage (index as unique identifier)
- Deduplication (same audio = same index)
- Verification (hash of index proves audio authenticity)

---

## Future Extensions

### Version 2 Considerations

Potential enhancements for future format versions:

- **Checksum Field:** CRC32 or hash for integrity verification
- **Compression:** Flag to indicate compressed payload
- **Metadata:** Embedded artist/title/album information
- **Multi-block:** Support for streaming large files

### Backwards Compatibility

Implementations should check the version byte and gracefully reject unknown versions with a clear error message.

---

## Reference Implementation

See the following files for the canonical C++ implementation:

- **Serialization:** `cpp/src/AudioIndex.cpp` - `audioDataToIndex()`
- **Deserialization:** `cpp/src/AudioIndex.cpp` - `indexToAudioData()`
- **Base64 Encoding:** `cpp/include/Utilities.h` - `encodeBase64Url()` / `decodeBase64Url()`
- **WAV Parsing:** `cpp/src/AudioIndex.cpp` - `extractAudioDataFromAudioFile()`

---

## Contact and Contributing

For questions or suggestions about this specification:

- **Repository:** [Audio-Babel](https://github.com/Jumplion/Audio-Babel)
- **Issues:** Submit format clarifications or extensions via GitHub Issues

---

*Last Updated: October 30, 2025*  
*Specification Version: 1.0*
