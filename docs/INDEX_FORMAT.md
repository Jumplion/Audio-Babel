# Audio Index Format Specification

**Version:** 2.0
**Project:** Speaker of Babel - Audio Indexing Library

---

## Overview

The Speaker of Babel audio indexing system maps a PCM audio payload to a single
large integer (a `boost::multiprecision::cpp_int`) and back. The mapping is a
**TRUE BIJECTION**: every possible index maps to exactly one PCM payload, and
every payload maps to exactly one index — no gaps, no collisions, and no
rejected inputs.

The index encodes the **PCM sample payload only**. It contains **no header**,
version byte, frame count, sample rate, bit depth, or channel count. A fixed
default header is applied **only** when writing a WAV file on decode:

- PCM, 44100 Hz, 16-bit, 1 channel (mono).

---

## The Payload Is Indexed by Whole Samples

The atomic unit of the payload is **one PCM sample**, not one byte. At the 16-bit
default the sample alphabet size is:

```
B = 1u << DEFAULT_BIT_DEPTH = 1u << 16 = 65536
```

Each 16-bit sample is interpreted as an **unsigned little-endian** value in the
range `0 .. 65535`. Signedness is irrelevant to the bijection. Indexing whole
samples (rather than raw bytes) guarantees that every index decodes to a whole
number of samples at the default bit depth.

The empty payload corresponds to the integer `0`, which corresponds to the empty
index string.

---

## Core Algorithm: Bijective Numeration (digit = value + 1)

The integer is built with **bijective numeration**, where each digit is the
sample value plus one. This is what allows trailing-zero (silence) samples to be
preserved: a zero sample contributes a real digit (`1`) instead of vanishing.

### Payload samples → integer (bijective base `B`)

```text
n = 0
for each sample v in order (v in 0 .. B-1):
    n = n * B + (v + 1)
```

### Integer → payload samples

```text
while n > 0:
    n  -= 1
    v   = n mod B      // emit v
    n   = n / B
// then reverse the emitted samples
// serialize each sample little-endian into the payload bytes
```

### Properties

- `indexToAudioData(audioDataToIndex(x))` reproduces `x`'s samples exactly,
  including any leading **and** trailing zero (silence) samples and the exact
  sample count.
- A payload with `k` trailing zero samples and one with `k+1` produce
  **different** indices.

---

## Index String: Bijective Base-64

For storage and transmission the integer index is rendered as a string over the
URL-safe alphabet (64 characters, no padding):

```
ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_
```

The string encoding is itself a **bijection** using the same bijective
numeration (digit = value + 1). It is **not** the bit-packing base-64 used for
arbitrary byte blobs.

### Integer → index string

```text
while n > 0:
    n -= 1
    emit ALPHA[n mod 64]
    n  = n / 64
// then reverse
```

### Index string → integer

```text
n = 0
for each char c in order:
    n = n * 64 + (alphaValue(c) + 1)
```

### Properties

- `b64ToIndex(indexToB64(n)) == n` for all `n >= 0`.
- `indexToB64(b64ToIndex(s)) == s` for every string `s` over the 64-symbol
  alphabet.
- The empty string maps to `0`; `0` maps to the empty string.
- Every alphabet-valid string of any length `>= 0` decodes to a valid payload.
  Nothing throws on alphabet-valid input.

---

## No Integrity Check (By Design)

There is intentionally **no checksum and no version byte**. Every alphabet-valid
index decodes to a valid payload. The accepted consequence is that a truncated or
mistyped index silently decodes to a different valid WAV. This is a deliberate
property of the bijection, not a bug.

The only rejection that can occur is a character outside the 64-symbol alphabet
when parsing an index string (`b64ToIndex` throws `std::invalid_argument`).

---

## Performance Note

The integer ↔ payload conversion uses repeated `cpp_int` multiply/divide by `B`,
one operation per sample, which is **O(L²)** in the number of samples `L`. This
is acceptable for short clips. Long files should instead process the value in
machine-word chunks rather than per-sample big-integer division. See the `TODO`
in `cpp/src/AudioIndex.cpp`.

---

## Reference Implementation

- **Payload ↔ integer:** `cpp/src/AudioIndex.cpp` —
  `audioDataToIndex()` / `indexToAudioData()`
- **Integer ↔ index string:** `cpp/include/Utilities.h` —
  `indexToB64()` / `b64ToIndex()`
- **Constants:** `cpp/include/Constants.h` —
  `DEFAULT_BIT_DEPTH`, `DEFAULT_SAMPLE_RATE`, `SAMPLE_ALPHABET_SIZE`,
  `BASE64_ALPHABET_SIZE`
- **WAV parsing / writing:** `cpp/src/AudioIndex.cpp`
  (`extractAudioDataFromAudioFile`) and `cpp/src/FileWriters.cpp`
  (`exportAudioDataToWav`)
